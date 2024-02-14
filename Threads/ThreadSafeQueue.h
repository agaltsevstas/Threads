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
    
    TType& Get()
    {
        std::unique_lock lock(_mutex);
        return _queue.back();
    }

    void Push(TType&& value)
    {
        std::shared_lock lock(_mutex);
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
