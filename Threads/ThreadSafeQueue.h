#ifndef ThreadSafeQueue_h
#define ThreadSafeQueue_h

#include <shared_mutex>
#include <queue>

namespace MUTEX
{
    template <typename T> 
    class Queue
    {
    public:
        void push(T&& value)
        {
            std::lock_guard lock (_mutex);
            bool empty = _queue.empty();
            _queue.push(std::forward<T>(value));
            if (empty)
                _cv.notify_one();
        }

        T pop()
        {
            std::unique_lock lock(_mutex);
            _cv.wait(lock, [this]()
            {
                return !_queue.empty();
            });
            T retval = _queue.front();
            _queue.pop();
            return retval;
        }
        
    private:
        std::queue<T> _queue;
        std::mutex _mutex;
        std::condition_variable _cv;
    };
}

namespace SHARED_MUTEX
{
    template <class T>
    class ThreadSafeQueue
    {
    public:
        ThreadSafeQueue() noexcept = default;
        ~ThreadSafeQueue() noexcept = default;
        
        T Back()
        {
            std::shared_lock lock(_mutex);
            return _queue.back();
        }
        
        T Front()
        {
            std::shared_lock lock(_mutex);
            return _queue.front();
        }
        
        bool Empty()
        {
            std::shared_lock lock(_mutex);
            return _queue.empty();
        }
        
        size_t Size()
        {
            std::shared_lock lock(_mutex);
            return _queue.Size();
        }

        void Push(T&& value)
        {
            std::unique_lock lock(_mutex);
            _queue.push(std::forward<T>(value));
        };
        
        void Pop()
        {
            std::unique_lock lock(_mutex);
            _queue.pop();
        };
        
        void Clear()
        {
            std::unique_lock lock(_mutex);
            if (!_queue.empty())
                _queue.pop();
        };

    private:
        std::queue<T> _queue;
        std::shared_mutex _mutex;
    };
}

#endif /* ThreadSafeQueue_h */
