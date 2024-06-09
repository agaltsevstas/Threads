#include "Interview.hpp"

#include <chrono>
#include <iostream>
#include <queue>
#include <mutex>
#include <thread>

namespace interview
{
    namespace MUTEX
    {
        namespace first_task
        {
            namespace incorrect
            {
                std::queue<std::string> messages;
            
                void function(const std::string& message)
                {
                    
                }
                
                void handle_message()
                {
                    std::mutex mutex; // mutex - локальная переменная, поэтому НЕ будет работать для нескольких потоков
                    mutex.lock(); // Если условие дальше не выполнится unlock не будет. Лучше использовать Lock management - RAII обертки (std::lock_guard, std::unique_lock)
                    if (!messages.empty())
                    {
                        const auto& message = messages.front();
                        mutex.unlock();
                        function(message);
                        mutex.lock();
                        messages.pop();
                        mutex.unlock();
                    }
                }
            }
        
            namespace correct
            {
                std::queue<std::string> messages;
                std::mutex mutex; // mutex - глабальная переменная, поэтому будет работать для нескольких потоков
            
                void function(const std::string& message)
                {
                    
                }
                
                void handle_message()
                {
                    if (!messages.empty())
                    {
                        std::lock_guard lock(mutex);
                        auto message = messages.front();
                        messages.pop();
                        function(message);
                    }
                }
            }
        }
    }
    
    void start()
    {
        std::cout << "interview" << std::endl;
        /// Thread
        {
            /// Задача 1: стоит незабывать про join и detach, а  joinable - проверяет ассоциирован std::thread с потоком, если нет (не было detach или join или std::move) - возвращает true.
            {
                auto print = []()
                {
                    std::cout << "Задача 1: стоит незабывать про join и detach, а  joinable - проверяет ассоциирован std::thread с потоком, если нет (не было detach или join или std::move) - возвращает true" << std::endl;
                };
                std::thread thread(print);
                std::thread moved = std::move(thread);
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                
                if (thread.joinable())
                    thread.join();
                
                if (moved.joinable())
                    moved.join();
            }
            /// Задача 2: Race condition/data race (состояние гонки) - обращение к общим данным в разных потоках одновременно
            {
                std::cout << "Задача 2: Race condition/data race (состояние гонки) - обращение к общим данным в разных потоках одновременно" << std::endl;
                
                std::mutex mutex;
                auto PrintSymbol = [&mutex](char c)
                    {
                        std::lock_guard lock(mutex);
                        for (int i = 0; i < 10; ++i)
                            std::cout << c;
                        std::cout << std::endl;
                    };

                std::thread thread1(PrintSymbol, '+');
                std::thread thread2(PrintSymbol, '-');

                if (thread1.joinable())
                    thread1.join();
                
                if (thread2.joinable())
                    thread2.join();
            }
        }
        /// Mutex
        {
            using namespace MUTEX;
            std::cout << "mutex" << std::endl;
            
            /// Задача 1
            {
                using namespace first_task;
                std::cout << "first task" << std::endl;
                
                /// Неправильное решение
                {
                    using namespace incorrect;
                    std::cout << "incorrect" << std::endl;
                    
                    handle_message();
                }
                /// Правильное решение
                {
                    using namespace correct;
                    std::cout << "correct" << std::endl;
                    
                    handle_message();
                }
            }
        }
        /// Deadlock
        {
            class Data
            {
            public:
                explicit Data(size_t value):
                _value(value)
                {}
                
                /// Может быть deadlock
                bool operator<(const Data& other) const
                {
                    std::scoped_lock(_mutex, other._mutex);
                    // std::lock_guard lhs_lock(_mutex);
                    // std::lock_guard rhs_lock(other._mutex);
                    
                    return _value < other._value;
                }
            private:
                size_t _value;
                mutable std::mutex _mutex;
            };
            
            Data data1(1u);
            Data data2(2u);
            
            auto Compare1 = [&data1, &data2]()
            {
                for (int i = 0; i < 10; ++i)
                {
                    [[maybe_unused]] auto result = data1 < data2;
                }
            };
            
            auto Compare2 = [&data1, &data2]()
            {
                for (int i = 0; i < 10; ++i)
                {
                    // [[maybe_unused]] auto result = data2 < data1; // Возможен deadlock, нужно поменять местами
                    [[maybe_unused]] auto result = data1 < data2;
                }
            };
            
            std::thread thread1(Compare1);
            std::thread thread2(Compare2);

            if (thread1.joinable())
                thread1.join();
            
            if (thread2.joinable())
                thread2.join();
        }
    }
}
        
