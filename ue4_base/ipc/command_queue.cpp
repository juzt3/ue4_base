#include "command_queue.h"

namespace ipc {

CommandQueue& CommandQueue::instance() {
    static CommandQueue instance;
    return instance;
}

void CommandQueue::enqueue(const Message& msg, HANDLE response_pipe) {
    std::lock_guard<std::mutex> lock(mutex_);
    queue_.push({ msg, response_pipe });
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
        while (!queue_.empty()) {
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
