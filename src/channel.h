#pragma once

#include <queue>
#include <mutex>
#include <condition_variable>
#include <optional>

#include <tracy/Tracy.hpp>

// Simple blocking multithreaded queue.
template <typename T>
class Channel {
public:
    Channel(size_t maxSize);
    ~Channel() = default;

    void push(const T& element);
    T pop();
    std::optional<T> tryPop();

private:
    size_t maxQueueSize;
    std::queue<T> buffer{};
    std::mutex lock{};
    std::condition_variable dequeueVar{};
    std::condition_variable enqueueVar{};
};

template <typename T>
inline Channel<T>::Channel(size_t maxSize) {
    maxQueueSize = maxSize;
}

template <typename T>
inline void Channel<T>::push(const T& element) {
    //ZoneScoped;

    {
        std::unique_lock scopeLock{ lock };

        // Check if the queue has exceeded the set depth. If so, pause and wait for it to drain.
        if (maxQueueSize > 0 && buffer.size() >= maxQueueSize) {
            dequeueVar.wait(scopeLock, [this]() { return buffer.size() < maxQueueSize; });
        }

        buffer.emplace(std::move(element));
    }

    enqueueVar.notify_one();
}

template <typename T>
inline T Channel<T>::pop() {
    //ZoneScoped;

    T result;
    {
        std::unique_lock scopeLock{ lock };

        // Wait until an element arrives in the buffer.
        enqueueVar.wait(scopeLock, [this]() { return !buffer.empty(); });

        result = std::move(buffer.front());
        buffer.pop();
    }

    dequeueVar.notify_one();

    return result;
}

template <typename T>
inline std::optional<T> Channel<T>::tryPop() {
    //ZoneScoped;

    std::optional<T> result{};
    {
        std::unique_lock scopeLock{ lock };

        if (!buffer.empty()) {
            result = std::move(buffer.front());
            buffer.pop();
        }
    }

    if (result) {
        dequeueVar.notify_one();
    }

    return result;
}
