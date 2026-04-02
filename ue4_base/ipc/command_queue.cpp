#include "command_queue.h"
#include <iostream>

namespace ipc {

CommandQueue& CommandQueue::instance() {
    static CommandQueue instance;
    return instance;
}

bool CommandQueue::execute_sync(const Message& msg, HANDLE response_pipe, Message& out_response) {
    // Crear la promesa para esperar el resultado
    std::promise<CommandResult> promise;
    std::future<CommandResult> future = promise.get_future();

    // Encolar el comando con la promesa
    {
        std::lock_guard<std::mutex> lock(mutex_);
        queue_.push({ msg, response_pipe, &promise });
    }

    // Esperar a que el game thread complete el comando (con timeout)
    // Usar un timeout razonable para no bloquear indefinidamente
    auto status = future.wait_for(std::chrono::seconds(5));

    if (status == std::future_status::timeout) {
        std::cerr << "[IPC] Timeout esperando ejecución del comando" << std::endl;
        return false;
    }

    try {
        CommandResult result = future.get();
        if (result.success) {
            out_response = result.response;
        }
        return result.success;
    } catch (const std::exception& e) {
        std::cerr << "[IPC] Error obteniendo resultado: " << e.what() << std::endl;
        return false;
    }
}

void CommandQueue::enqueue(const Message& msg, HANDLE response_pipe) {
    std::lock_guard<std::mutex> lock(mutex_);
    queue_.push({ msg, response_pipe, nullptr });
}

std::vector<QueuedCommand> CommandQueue::dequeue_all() {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<QueuedCommand> result;

    while (!queue_.empty()) {
        result.push_back(queue_.front());
        queue_.pop();
    }

    return result;
}

void CommandQueue::enqueue_response(const Message& msg, HANDLE pipe) {
    std::lock_guard<std::mutex> lock(response_mutex_);
    responses_.push({ msg, pipe });
}

std::vector<PendingResponse> CommandQueue::dequeue_responses() {
    std::lock_guard<std::mutex> lock(response_mutex_);
    std::vector<PendingResponse> result;

    while (!responses_.empty()) {
        result.push_back(responses_.front());
        responses_.pop();
    }

    return result;
}

bool CommandQueue::has_pending() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return !queue_.empty();
}

bool CommandQueue::has_responses() const {
    std::lock_guard<std::mutex> lock(response_mutex_);
    return !responses_.empty();
}

void CommandQueue::clear() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        // Completar todas las promesas pendientes con error antes de limpiar
        while (!queue_.empty()) {
            auto& cmd = queue_.front();
            if (cmd.promise != nullptr) {
                CommandResult result;
                result.success = false;
                cmd.promise->set_value(result);
            }
            queue_.pop();
        }
    }
    {
        std::lock_guard<std::mutex> lock(response_mutex_);
        while (!responses_.empty()) {
            responses_.pop();
        }
    }
}

} // namespace ipc
