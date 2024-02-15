#ifndef ThreadSafeQueue_h
#define ThreadSafeQueue_h

#include <shared_mutex>
#include <queue>

template <class TType>
class ThreadSafeQueue
{
public:
    ThreadSafeQueue() noexcept = default;
    ~ThreadSafeQueue() noexcept = default;
    
    TType Back()
    {
        std::shared_lock lock(_mutex);
        return _queue.back();
    }
    
    TType Front()
    {
        std::shared_lock lock(_mutex);
        return _queue.front();
    }

    void Push(TType&& value)
    {
        std::unique_lock lock(_mutex);
        _queue.push(std::forward<TType>(value));
    };
    
    void Clear()
    {
        std::unique_lock lock(_mutex);
        if (!_queue.empty())
            _queue.pop();
    };

private:
    std::queue<int> _queue;
    std::shared_mutex _mutex;
};

#endif /* ThreadSafeQueue_h */
