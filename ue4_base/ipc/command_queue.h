#pragma once

#include "protocol.h"
#include <mutex>
#include <queue>
#include <functional>

namespace ipc {

// Comando encolado para ejecución en el hilo del juego
struct QueuedCommand {
    Message message;
    std::function<void(const Message&)> response_callback;
};

// Cola thread-safe para comandos
class CommandQueue {
public:
    static CommandQueue& instance();

    // Agregar comando a la cola (llamado desde el hilo del servidor)
    void enqueue(const Message& msg, std::function<void(const Message&)> callback);

    // Obtener todos los comandos pendientes (llamado desde el hilo del juego)
    std::vector<QueuedCommand> dequeue_all();

    // Verificar si hay comandos pendientes
    bool has_pending() const;

    // Limpiar la cola
    void clear();

private:
    CommandQueue() = default;
    ~CommandQueue() = default;
    CommandQueue(const CommandQueue&) = delete;
    CommandQueue& operator=(const CommandQueue&) = delete;

    mutable std::mutex mutex_;
    std::queue<QueuedCommand> queue_;
};

} // namespace ipc
