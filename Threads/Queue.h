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
        ThreadSafeQueue(const ThreadSafeQueue&) = delete;
        ThreadSafeQueue(ThreadSafeQueue&&) noexcept = delete;
        ThreadSafeQueue& operator=(const ThreadSafeQueue&) = delete;
        ThreadSafeQueue& operator=(ThreadSafeQueue&&) noexcept = delete;
    public:
        ThreadSafeQueue() = default;
        ~ThreadSafeQueue() = default;
        
        void Push(T&& value)
        {
            {
                std::lock_guard lock(_mutex);
                _queue.push(std::forward<T>(value));
                // _cv.notify_one(); Нужно уведомлять условную переменную после того, как отпустили mutex (unlock), чтобы wait не тратил время на ожидание разблокировки mutex для захвата (lock) mutex, поэтому он выносится за скобки
            }
            _cv.notify_one(); // в wait удаляется ОДИН поток из очереди и он пробуждается
        }

        T Pop()
        {
            std::unique_lock lock(_mutex);
            /*
             Тоже самое, что:
             while (_queue.empty())
                 cv.wait(lock);
             
              в случае НЕвыполнения условия (например, поток-писатель еще не выполнил условие): идет засыпание потока и помещение его в очередь ожидающих потоков с помощью планировщика (Scheduler) через поход в ядро процессора - и так все потоки-читатели поочередно помещаются в очередь ожидания.
              в случае ВЫПОЛНЕНИЯ условия: происходит попытка захвата mutex (lock) ОДНИМ потоком, продолжающаяся до тех пор, пока он не будет захвачен (бывает другой поток успел раньше захватить mutex и не отпустил его еще). После захвата (lock) mutexа ОДНИМ потоком, он выходит из wait, читает данные, выходит из области видимости unique_lock, освобождая mutex (unlock).
             */
            _cv.wait(lock, [this]() { return !_queue.empty(); }); // ждать пока данные пустые + обработка ложных пробуждений (spurious wakeup)
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
        ThreadSafeQueue(const ThreadSafeQueue&) = delete;
        ThreadSafeQueue(ThreadSafeQueue&&) noexcept = delete;
        ThreadSafeQueue& operator=(const ThreadSafeQueue&) = delete;
        ThreadSafeQueue& operator=(ThreadSafeQueue&&) noexcept = delete;
    public:
        ThreadSafeQueue() = default;
        ~ThreadSafeQueue() = default;
        
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
