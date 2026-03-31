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
    HANDLE pipe = CreateNamedPipeW(
        PIPE_NAME,
        PIPE_ACCESS_DUPLEX,
        PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
        PIPE_UNLIMITED_INSTANCES,
        static_cast<DWORD>(MAX_MESSAGE_SIZE),
        static_cast<DWORD>(MAX_MESSAGE_SIZE),
        5000, // Timeout de 5 segundos
        nullptr // Seguridad por defecto
    );

    if (pipe == INVALID_HANDLE_VALUE) {
        std::cerr << "[IPC] Error al crear pipe: " << GetLastError() << std::endl;
    }

    return pipe;
}

void PipeServer::server_loop() {
    std::wcout << L"[IPC] Servidor iniciado en " << PIPE_NAME << std::endl;

    while (running_) {
        pipe_handle_ = create_pipe();
        if (pipe_handle_ == INVALID_HANDLE_VALUE) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            continue;
        }

        std::cout << "[IPC] Esperando conexión de cliente..." << std::endl;

        BOOL connected = ConnectNamedPipe(pipe_handle_, nullptr);
        if (!connected && GetLastError() != ERROR_PIPE_CONNECTED) {
            std::cerr << "[IPC] Error en ConnectNamedPipe: " << GetLastError() << std::endl;
            CloseHandle(pipe_handle_);
            pipe_handle_ = INVALID_HANDLE_VALUE;
            continue;
        }

        std::cout << "[IPC] Cliente conectado!" << std::endl;
        handle_client(pipe_handle_);

        DisconnectNamedPipe(pipe_handle_);
        CloseHandle(pipe_handle_);
        pipe_handle_ = INVALID_HANDLE_VALUE;
    }
}

void PipeServer::handle_client(HANDLE pipe) {
    while (running_) {
        Message request;
        Message response;
        DWORD bytes_read = 0;
        DWORD bytes_written = 0;

        // Leer el header primero
        BOOL success = ReadFile(pipe, &request.header, sizeof(MessageHeader), &bytes_read, nullptr);
        if (!success || bytes_read != sizeof(MessageHeader)) {
            if (GetLastError() == ERROR_BROKEN_PIPE) {
                std::cout << "[IPC] Cliente desconectado." << std::endl;
            } else {
                std::cerr << "[IPC] Error leyendo header: " << GetLastError() << std::endl;
            }
            break;
        }

        // Validar el mensaje
        if (!request.header.is_valid()) {
            std::cerr << "[IPC] Mensaje inválido recibido" << std::endl;
            response.header.type = CommandType::RESPONSE;
            response.payload.response.result = ResultCode::ERROR_INVALID_PARAM;
            send_response(pipe, response);
            continue;
        }

        // Leer el payload si existe
        if (request.header.payload_size > 0 && request.header.payload_size <= sizeof(request.payload)) {
            success = ReadFile(pipe, &request.payload, request.header.payload_size, &bytes_read, nullptr);
            if (!success || bytes_read != request.header.payload_size) {
                std::cerr << "[IPC] Error leyendo payload: " << GetLastError() << std::endl;
                break;
            }
        }

        // Procesar el mensaje
        process_message(request, response, pipe);
    }
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
        // Encolar para ejecución en el hilo del juego
        CommandQueue::instance().enqueue(request, [pipe, this](const Message& result) {
            send_response(pipe, result);
            });
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
