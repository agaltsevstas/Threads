#pragma once

#include <atomic>
#include <shared_mutex>
#include <thread>


class shared_recursive_mutex : public std::shared_mutex
{
public:
    void lock() 
    {
        std::thread::id this_id = std::this_thread::get_id();
        if (_owner == this_id) // Рекурсивная блокировка
        {
            ++_count;
        }
        else // Обычная блокировка
        {
            shared_mutex::lock();
            _owner = this_id;
            _count = 1;
        }
    }
    void unlock() 
    {
        if (_count > 1) // Рекурсивная разблокировка
        {
            --_count;
        }
        else  // Обычная разблокировка
        {
            _owner = std::thread::id();
            _count = 0;
            shared_mutex::unlock();
        }
    }

private:
    std::atomic<std::thread::id> _owner;
    int _count;
};
