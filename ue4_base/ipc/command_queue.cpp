#include "command_queue.h"
#include <mutex>
#include <vector>

namespace ipc {

CommandQueue& CommandQueue::instance() {
    static CommandQueue instance;
    return instance;
}

void CommandQueue::enqueue(const Message& msg, std::function<void(const Message&)> callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    queue_.push({ msg, callback });
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

bool CommandQueue::has_pending() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return !queue_.empty();
}

void CommandQueue::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    while (!queue_.empty()) {
        queue_.pop();
    }
}

} // namespace ipc
