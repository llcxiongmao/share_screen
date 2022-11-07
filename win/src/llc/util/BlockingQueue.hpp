#pragma once
#include <mutex>
#include <condition_variable>
#include <deque>

namespace llc {
/** blocking queue impl. */
template <typename T>
class BlockingQueue {
public:
    BlockingQueue() {}

    /**
     * push to queue.
     *
     * @param value value to push.
     * @return return true if push success or false if this closed.
     */
    template <typename U>
    bool push(U&& value) {
        {
            std::unique_lock<std::mutex> lock(mLock);
            if (mIsClose)
                return false;
            mQueue.push_front(std::forward<U>(value));
        }
        mCondition.notify_all();
        return true;
    }

    /**
     * same as push, but only move value if success push.
     *
     * @param value
     * @return return true if push success or false if this closed.
     */
    bool pushEx(T& value) {
        {
            std::unique_lock<std::mutex> lock(mLock);
            if (mIsClose) {
                return false;
            }
            mQueue.push_front(std::move(value));
        }
        mCondition.notify_all();
        return true;
    }

    /**
     * pop value.
     *
     * @param value receive value.
     * @return return true if receive success or false if this closed.
     */
    bool pop(T& value) {
        std::unique_lock<std::mutex> lock(mLock);
        while (true) {
            if (mQueue.empty()) {
                if (mIsClose)
                    return false;
            } else {
                break;
            }
            mCondition.wait(lock);
        }
        value = std::move(mQueue.back());
        mQueue.pop_back();
        return true;
    }

    /** close queue, after call push pushEx will return false. */
    void close() {
        {
            std::unique_lock<std::mutex> lock(mLock);
            mIsClose = true;
        }
        mCondition.notify_all();
    }

private:
    bool mIsClose = false;
    std::mutex mLock;
    std::condition_variable mCondition;
    std::deque<T> mQueue;
};
}  // namespace llc
