#ifndef ThreadSafeQueue_h
#define ThreadSafeQueue_h

#include <mutex>
#include <queue>
#include <shared_mutex>
#include <condition_variable>


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
        
        T Back() const
        {
            std::lock_guard lock(_mutex);

            if (_queue.empty())
                return std::nullopt;

            return _queue.back();
        }
        
        std::optional<T> Front() const
        {
            std::lock_guard lock(_mutex);

            if (_queue.empty())
                return std::nullopt;

            return _queue.front();
        }
        
        template <typename U>
        void Push(U&& value)
        {
            {
                std::lock_guard lock(_mutex);
                _queue.push(std::forward<U>(value));
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
            T item = std::move(_queue.front());
            _queue.pop();
            return item;
        }
        
        size_t Size() const
        {
            std::lock_guard lock(_mutex);
            return _queue.size();
        }
        
        bool Empty() const
        {
            std::lock_guard lock(_mutex);
            return _queue.empty();
        }
        
        void Clear()
        {
            std::lock_guard lock(_mutex);

            while (!_queue.empty())
                _queue.pop();
        }
        
    private:
        std::queue<T> _queue;
        mutable std::mutex _mutex;
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
        
        std::optional<T> Back() const
        {
            std::shared_lock lock(_mutex);

            if (_queue.empty())
                return std::nullopt;

            return _queue.back();
        }
        
        std::optional<T> Front() const
        {
            std::shared_lock lock(_mutex);

            if (_queue.empty())
                return std::nullopt;

            return _queue.front();
        }

        template <typename U>
        void Push(U&& value)
        {
            {
                std::unique_lock lock(_mutex);
                _queue.push(std::forward<U>(value));
            }

            _cv.notify_one();
        }
        
        T Pop()
        {
            std::unique_lock lock(_mutex);

            _cv.wait(lock, [this] {
                return !_queue.empty();
            });

            T value = std::move(_queue.front());
            _queue.pop();

            return value;
        }
        
        size_t Size() const
        {
            std::shared_lock lock(_mutex);
            return _queue.size();
        }
        
        bool Empty() const
        {
            std::shared_lock lock(_mutex);
            return _queue.empty();
        }
        
        void Clear()
        {
            std::unique_lock lock(_mutex);
            while (!_queue.empty())
                _queue.pop();
        }

    private:
        std::queue<T> _queue;
        mutable std::shared_mutex _mutex;
        std::condition_variable_any _cv;
    };
}

#endif /* ThreadSafeQueue_h */
