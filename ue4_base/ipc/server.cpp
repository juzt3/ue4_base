#include "server.h"
#include "command_queue.h"
#include <iostream>
#include <vector>

// Declaraciones externas de funciones que necesitamos
namespace ue4::engine { class u_world; extern u_world** world; }
namespace ue4::game_framework { class a_player_controller; }

namespace ipc {

PipeServer::~PipeServer() {
    stop();
}

PipeServer& PipeServer::instance() {
    static PipeServer instance;
    return instance;
}

void PipeServer::start() {
    if (running_.exchange(true)) {
        return; // Ya está corriendo
    }

    std::cout << "[IPC] Iniciando servidor..." << std::endl;
    server_thread_ = std::thread(&PipeServer::server_loop, this);
}

void PipeServer::stop() {
    if (!running_.exchange(false)) {
        return; // No estaba corriendo
    }

    std::cout << "[IPC] Deteniendo servidor..." << std::endl;

    // Forzar la desconexión del pipe para desbloquear ConnectNamedPipe
    if (pipe_handle_ != INVALID_HANDLE_VALUE) {
        CancelIoEx(pipe_handle_, nullptr);
        CloseHandle(pipe_handle_);
        pipe_handle_ = INVALID_HANDLE_VALUE;
    }

    if (server_thread_.joinable()) {
        server_thread_.join();
    }

    std::cout << "[IPC] Servidor detenido." << std::endl;
}

bool PipeServer::is_running() const {
    return running_;
}

HANDLE PipeServer::create_pipe() {
    // SECURITY_ATTRIBUTES para permitir que el handle sea heredado
    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.lpSecurityDescriptor = nullptr;
    sa.bInheritHandle = FALSE;

    // Primero intentar crear el pipe sin FILE_FLAG_FIRST_PIPE_INSTANCE
    HANDLE pipe = CreateNamedPipeW(
        PIPE_NAME,
        PIPE_ACCESS_DUPLEX,
        PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
        1,  // Solo 1 instancia para forzar reciclaje
        static_cast<DWORD>(MAX_MESSAGE_SIZE),
        static_cast<DWORD>(MAX_MESSAGE_SIZE),
        5000,
        &sa
    );

    // Si falla porque ya existe, intentar conectar al existente y cerrarlo
    if (pipe == INVALID_HANDLE_VALUE) {
        DWORD err = GetLastError();
        std::cerr << "[IPC] Error al crear pipe: " << err << std::endl;

        if (err == ERROR_ACCESS_DENIED || err == ERROR_PIPE_BUSY) {
            std::cerr << "[IPC] Pipe existe o está ocupado, intentando limpiar..." << std::endl;

            // Intentar abrir el pipe existente para cerrarlo
            HANDLE existingPipe = CreateFileW(
                PIPE_NAME,
                GENERIC_READ | GENERIC_WRITE,
                0, nullptr, OPEN_EXISTING, 0, nullptr
            );

            if (existingPipe != INVALID_HANDLE_VALUE) {
                std::cerr << "[IPC] Pipe existente abierto, cerrando..." << std::endl;
                CloseHandle(existingPipe);
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
            }

            // Reintentar crear el pipe
            pipe = CreateNamedPipeW(
                PIPE_NAME,
                PIPE_ACCESS_DUPLEX,
                PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
                1,
                static_cast<DWORD>(MAX_MESSAGE_SIZE),
                static_cast<DWORD>(MAX_MESSAGE_SIZE),
                5000,
                &sa
            );
        }
    }

    return pipe;
}

void PipeServer::server_loop() {
    std::wcout << L"[IPC] Servidor iniciado en " << PIPE_NAME << std::endl;

    while (running_) {
        // Crear un pipe nuevo para cada iteración - NO reusar handles
        HANDLE pipe = create_pipe();
        if (pipe == INVALID_HANDLE_VALUE) {
            std::cerr << "[IPC] Error creando pipe, reintentando..." << std::endl;
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }

        std::cout << "[IPC] Esperando conexión de cliente..." << std::endl;

        // Esperar conexión del cliente
        BOOL connected = ConnectNamedPipe(pipe, nullptr);
        DWORD connectError = GetLastError();

        if (!connected && connectError != ERROR_PIPE_CONNECTED) {
            std::cerr << "[IPC] Error en ConnectNamedPipe: " << connectError << std::endl;
            CloseHandle(pipe);
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }

        // ERROR_PIPE_CONNECTED significa que el cliente ya se conectó antes de ConnectNamedPipe
        std::cout << "[IPC] Cliente conectado! (error code: " << connectError << ")" << std::endl;

        // Guardar handle global solo para poder cancelarlo en stop()
        pipe_handle_ = pipe;

        // Manejar al cliente
        handle_client(pipe);

        // Limpiar después de que el cliente se desconecte
        std::cout << "[IPC] Limpiando conexión..." << std::endl;

        // Resetear el handle global primero
        pipe_handle_ = INVALID_HANDLE_VALUE;

        // Cerrar el pipe completamente - ESTO ES CRÍTICO
        // El orden correcto es: Flush -> Disconnect -> Close
        FlushFileBuffers(pipe);  // Ignorar errores aquí
        DisconnectNamedPipe(pipe);  // Marca el pipe como disponible para reuso
        CloseHandle(pipe);  // Libera el handle del sistema

        std::cout << "[IPC] Pipe cerrado correctamente" << std::endl;

        // Pequeña pausa para asegurar que el OS libere el nombre del pipe
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    std::cout << "[IPC] Loop del servidor terminado" << std::endl;
}

void PipeServer::handle_client(HANDLE pipe) {
    int consecutive_errors = 0;
    const int MAX_CONSECUTIVE_ERRORS = 5;

    while (running_) {
        // Primero enviar cualquier respuesta pendiente
        if (CommandQueue::instance().has_responses()) {
            auto responses = CommandQueue::instance().dequeue_responses();
            for (const auto& resp : responses) {
                if (resp.pipe == pipe) {
                    if (!send_response(pipe, resp.message)) {
                        std::cerr << "[IPC] Error enviando respuesta, cliente probablemente desconectado" << std::endl;
                        goto disconnect_client;
                    }
                }
            }
        }

        // Intentar leer con timeout corto para poder enviar respuestas pendientes
        DWORD bytes_available = 0;
        BOOL peek_result = PeekNamedPipe(pipe, nullptr, 0, nullptr, &bytes_available, nullptr);

        if (!peek_result) {
            DWORD err = GetLastError();
            if (err == ERROR_BROKEN_PIPE || err == ERROR_PIPE_NOT_CONNECTED) {
                std::cout << "[IPC] Pipe roto o desconectado (PeekNamedPipe)" << std::endl;
                goto disconnect_client;
            }
            // Otro error, incrementar contador
            consecutive_errors++;
            if (consecutive_errors >= MAX_CONSECUTIVE_ERRORS) {
                std::cerr << "[IPC] Demasiados errores consecutivos, desconectando" << std::endl;
                goto disconnect_client;
            }
            Sleep(10);
            continue;
        }

        // Resetear contador de errores si peek tuvo éxito
        consecutive_errors = 0;

        if (bytes_available == 0) {
            // No hay datos, esperar un poco y continuar
            if (CommandQueue::instance().has_responses()) {
                continue; // Hay respuestas pendientes, procesarlas
            }
            Sleep(10);
            continue;
        }

        // Inicializar mensaje completo
        Message request{};  // Zero-initialize
        Message response{}; // Zero-initialize
        DWORD bytes_read = 0;
        DWORD total_bytes_read = 0;

        // Leer el header primero (manejar ERROR_MORE_DATA)
        BOOL success = FALSE;
        bytes_read = 0;
        
        while (total_bytes_read < sizeof(MessageHeader)) {
            success = ReadFile(pipe, 
                reinterpret_cast<char*>(&request.header) + total_bytes_read,
                sizeof(MessageHeader) - total_bytes_read,
                &bytes_read, nullptr);
            
            if (!success) {
                DWORD err = GetLastError();
                if (err == ERROR_MORE_DATA) {
                    total_bytes_read += bytes_read;
                    continue; // Seguir leyendo
                }
                if (err == ERROR_BROKEN_PIPE) {
                    std::cout << "[IPC] Cliente desconectado." << std::endl;
                } else {
                    std::cerr << "[IPC] Error leyendo header: " << err << std::endl;
                }
                goto disconnect_client;
            }
            total_bytes_read += bytes_read;
        }

        // Validar el mensaje
        if (!request.header.is_valid()) {
            std::cerr << "[IPC] Mensaje inválido recibido (magic/version incorrecto)" << std::endl;
            response.header.magic = MessageHeader::MAGIC;
            response.header.version = PROTOCOL_VERSION;
            response.header.request_id = request.header.request_id;
            response.header.type = CommandType::RESPONSE;
            response.payload.response.result = ResultCode::ERROR_INVALID_PARAM;
            response.header.payload_size = sizeof(ResponsePayload);
            send_response(pipe, response);
            continue;
        }

        // Validar payload_size
        if (request.header.payload_size > sizeof(request.payload)) {
            std::cerr << "[IPC] Payload size inválido: " << request.header.payload_size << std::endl;
            response.header.magic = MessageHeader::MAGIC;
            response.header.version = PROTOCOL_VERSION;
            response.header.request_id = request.header.request_id;
            response.header.type = CommandType::RESPONSE;
            response.payload.response.result = ResultCode::ERROR_INVALID_PARAM;
            response.header.payload_size = sizeof(ResponsePayload);
            send_response(pipe, response);
            continue;
        }

        // Leer el payload si existe
        if (request.header.payload_size > 0) {
            // Limpiar payload antes de leer
            memset(&request.payload, 0, sizeof(request.payload));
            
            success = ReadFile(pipe, &request.payload, request.header.payload_size, &bytes_read, nullptr);
            if (!success || bytes_read != request.header.payload_size) {
                std::cerr << "[IPC] Error leyendo payload: " << GetLastError() << std::endl;
                break;
            }
            
            std::cout << "[IPC] Payload leído: " << bytes_read << " bytes, tipo: " 
                      << static_cast<int>(request.header.type) << std::endl;
        }

        // Procesar el mensaje
        process_message(request, response, pipe);
    }

    // Enviar respuestas pendientes antes de cerrar
    {
        auto responses = CommandQueue::instance().dequeue_responses();
        for (const auto& resp : responses) {
            if (resp.pipe == pipe) {
                send_response(pipe, resp.message);
            }
        }
    }

disconnect_client:
    return;
}

void PipeServer::process_message(const Message& request, Message& response, HANDLE pipe) {
    response.header.magic = MessageHeader::MAGIC;
    response.header.version = PROTOCOL_VERSION;
    response.header.request_id = request.header.request_id;
    response.header.type = CommandType::RESPONSE;

    switch (request.header.type) {
    case CommandType::PING:
        // Responder inmediatamente
        response.payload.response.result = ResultCode::SUCCESS;
        response.header.payload_size = sizeof(ResponsePayload);
        send_response(pipe, response);
        break;

    case CommandType::MOVE_TO_LOCATION:
    case CommandType::USE_COMMAND:
    case CommandType::GET_PLAYER_POS:
    case CommandType::USE_SKILL:
    case CommandType::ATTACK:
    case CommandType::SELECT_TARGET:
    case CommandType::TARGET_ATTACK:
        // Ejecutar sincrónicamente en el hilo del juego
        // El servidor esperará a que el comando se complete antes de continuar
        {
            Message sync_response;
            bool success = CommandQueue::instance().execute_sync(request, pipe, sync_response);

            if (success) {
                // Enviar la respuesta directamente
                send_response(pipe, sync_response);
            } else {
                // Timeout o error
                response.payload.response.result = ResultCode::ERROR_EXECUTION_FAILED;
                response.header.payload_size = sizeof(ResponsePayload);
                send_response(pipe, response);
            }
        }
        break;

    default:
        response.payload.response.result = ResultCode::ERROR_UNKNOWN_COMMAND;
        response.header.payload_size = sizeof(ResponsePayload);
        send_response(pipe, response);
        break;
    }
}

bool PipeServer::send_response(HANDLE pipe, const Message& response) {
    DWORD bytes_written = 0;

    // Enviar header
    BOOL success = WriteFile(pipe, &response.header, sizeof(MessageHeader), &bytes_written, nullptr);
    if (!success || bytes_written != sizeof(MessageHeader)) {
        return false;
    }

    // Enviar payload si existe
    if (response.header.payload_size > 0) {
        success = WriteFile(pipe, &response.payload, response.header.payload_size, &bytes_written, nullptr);
        if (!success || bytes_written != response.header.payload_size) {
            return false;
        }
    }

    FlushFileBuffers(pipe);
    return true;
}

// Funciones de inicialización/limpieza globales
void initialize() {
    PipeServer::instance().start();
}

void shutdown() {
    PipeServer::instance().stop();
    CommandQueue::instance().clear();
}

} // namespace ipc
