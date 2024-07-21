#ifndef ThreadSafeQueue_h
#define ThreadSafeQueue_h

#include <mutex>
#include <queue>
#include <shared_mutex>


/*
 Потокобезопасная очередь - позволяет безопасно нескольким потоков получать доступ к элементам очереди без необходимости синхронизации (синхронизация внутри структуры).
 */
namespace MUTEX
{
    /*
     mutex используется для блокировки очереди всякий раз, когда поток пытается получить к элементу очереди.
     condition_variable используется для ожидания изменений в очереди:
     1. когда поток добавляет элемент в очередь, то он подает сигнал об этом condition_variable.
     2. когда поток пытается удалить элемент из очереди, то он должен сначала проверить, пуста ли очередь - это гарантия, что поток не удалит элемент из пустой очереди. Если очередь пуста, то он ждет сигнала от condition_variable, пока элемент не будет добавлен в очередь.
     */
    template <typename T>
    class ThreadSafeQueue
    {
    public:
        
        void Push(T&& value)
        {
            std::lock_guard lock(_mutex);
            _queue.push(std::forward<T>(value));
            _cv.notify_one(); // Уведомить один ожижающий поток, что он может удалить элемент
        }

        T Pop()
        {
            std::unique_lock lock(_mutex);
            /*
             Тоже самое, что:
             while (_queue.empty())
                 cv.wait(lock);
             */
            _cv.wait(lock, [this]() { return !_queue.empty(); }); // если очередь пуста, то ждем
            T item = _queue.front();
            _queue.pop();
            return item;
        }
        
        bool Empty()
        {
            std::lock_guard lock(_mutex);
            return _queue.empty();
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
