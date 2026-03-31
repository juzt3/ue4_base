#pragma once

#include "protocol.h"
#include <Windows.h>
#include <atomic>
#include <thread>

namespace ipc {

class PipeServer {
public:
    static PipeServer& instance();

    // Iniciar el servidor en un hilo separado
    void start();

    // Detener el servidor
    void stop();

    // Verificar si está corriendo
    bool is_running() const;

private:
    PipeServer() = default;
    ~PipeServer();
    PipeServer(const PipeServer&) = delete;
    PipeServer& operator=(const PipeServer&) = delete;

    // Hilo principal del servidor
    void server_loop();

    // Manejar una conexión de cliente
    void handle_client(HANDLE pipe);

    // Procesar un mensaje recibido
    void process_message(const Message& request, Message& response, HANDLE pipe);

    // Enviar respuesta al cliente
    bool send_response(HANDLE pipe, const Message& response);

    // Crear el Named Pipe
    HANDLE create_pipe();

    std::atomic<bool> running_{ false };
    std::thread server_thread_;
    HANDLE pipe_handle_ = INVALID_HANDLE_VALUE;
};

// Funciones de inicialización/limpieza
void initialize();
void shutdown();

} // namespace ipc
