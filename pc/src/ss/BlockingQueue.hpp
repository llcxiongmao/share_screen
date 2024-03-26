#pragma once
#include <mutex>
#include <condition_variable>
#include <deque>
#include <chrono>
#include <optional>

namespace ss {
template <typename T>
class BlockingQueue {
public:
    BlockingQueue() {}

    template <typename U>
    void push_back(U&& value) {
        {
            std::unique_lock<std::mutex> lock(mLock);
            mQueue.push_back(std::forward<U>(value));
        }
        mCondition.notify_one();
    }

    // return std::nullopt if timeout.
    std::optional<T> pop_front(std::chrono::milliseconds timeout) {
        std::unique_lock<std::mutex> lock(mLock);
        if (timeout < std::chrono::milliseconds(0)) {
            mCondition.wait(lock, [this]() { return !mQueue.empty(); });
            std::optional<T> value = std::move(mQueue.front());
            mQueue.pop_front();
            return value;
        } else {
            if (mCondition.wait_for(lock, timeout, [this]() { return !mQueue.empty(); })) {
                std::optional<T> value = std::move(mQueue.front());
                mQueue.pop_front();
                return value;
            } else {
                return std::nullopt;
            }
        }
    }

private:
    std::mutex mLock;
    std::condition_variable mCondition;
    std::deque<T> mQueue;
};
}  // namespace ss