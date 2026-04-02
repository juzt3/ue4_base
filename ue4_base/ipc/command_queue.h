#pragma once

#include "protocol.h"
#include <Windows.h>
#include <mutex>
#include <queue>
#include <functional>
#include <future>
#include <atomic>

namespace ipc {

// Respuesta del comando incluyendo el mensaje y el pipe
struct CommandResult {
    Message response;
    bool success = false;
};

// Comando encolado para ejecución en el hilo del juego
struct QueuedCommand {
    Message message;
    HANDLE response_pipe;
    std::promise<CommandResult>* promise;  // Para sincronización síncrona (puede ser nullptr)
};

// Respuesta pendiente para enviar desde el hilo del servidor
struct PendingResponse {
    Message message;
    HANDLE pipe;
};

// Cola thread-safe para comandos
class CommandQueue {
public:
    static CommandQueue& instance();

    // Agregar comando a la cola y esperar su ejecución (llamado desde el hilo del servidor)
    // Retorna true si el comando fue ejecutado exitosamente
    bool execute_sync(const Message& msg, HANDLE response_pipe, Message& out_response);

    // Agregar comando a la cola sin esperar (llamado desde el hilo del servidor) - mantener compatibilidad
    void enqueue(const Message& msg, HANDLE response_pipe);

    // Obtener todos los comandos pendientes (llamado desde el hilo del juego)
    std::vector<QueuedCommand> dequeue_all();

    // Agregar respuesta pendiente (llamado desde el hilo del juego)
    void enqueue_response(const Message& msg, HANDLE pipe);

    // Obtener respuestas pendientes (llamado desde el hilo del servidor)
    std::vector<PendingResponse> dequeue_responses();

    // Verificar si hay comandos pendientes
    bool has_pending() const;

    // Verificar si hay respuestas pendientes
    bool has_responses() const;

    // Limpiar la cola
    void clear();

private:
    CommandQueue() = default;
    ~CommandQueue() = default;
    CommandQueue(const CommandQueue&) = delete;
    CommandQueue& operator=(const CommandQueue&) = delete;

    mutable std::mutex mutex_;
    mutable std::mutex response_mutex_;
    std::queue<QueuedCommand> queue_;
    std::queue<PendingResponse> responses_;
};

} // namespace ipc
