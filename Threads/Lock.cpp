#include "Lock.hpp"

#include <iostream>
#include <mutex>
#include <shared_mutex>
#include <thread>


/*
 Lock management - RAII обертки, захват mutex.lock() ресурса происходит на стеке в конструкторе и высвобождение unlock при выходе из стека в деструкторе.
*/
namespace LOCK
{
    void Start()
    {
        std::mutex mutex;

        /*
        std::lock_guard - делает захват mutex.lock() ресурса происходит на стеке в конструкторе и высвобождение unlock при выходе из стека в деструкторе, использование идиомы RAII.
        Дополнительные тэги:
        - std::adopt_lock - считается lock сделан до этого.
        Замечание:
        - нельзя копировать.
        */
        {
            std::cout << "std::lock_guard" << std::endl;
            auto PrintSymbol = [&mutex](char c)
                {
                    std::lock_guard lock(mutex);
                    for (int i = 0; i < 10; ++i)
                    {
                        std::this_thread::sleep_for(std::chrono::milliseconds(1));
                        std::cout << c;
                    }

                    std::cout << std::endl;
                };

            std::thread thread1(PrintSymbol, '+');
            std::thread thread2(PrintSymbol, '-');

            thread1.join();
            thread2.join();
            std::cout << std::endl;
        }
        /*
        std::unique_lock - имеет возможности std::lock_guard + std::mutex. Приоритетнее использовать std::unique_lock, чем std::lock_guard.
        Методы:
        - owns_lock - проверка на блокировку, возвращает значение: true - mutex заблокирован / false - нет.
        Дополнительные тэги:
        - std::try_to_lock - происходит попытка захвата mutex текущим потоком, НО НЕ блокирует доступ к данным другим потокам.
        - std::defer_lock - lock не выполнется.
        - std::adopt_lock - считается lock сделан до этого.
        Замечание:
        - можно копировать.
        */
        {
            std::cout << "std::unique_lock" << std::endl;
            // 1 Способ: обычный
            {
                auto PrintSymbol = [&mutex](char c)
                    {
                        std::unique_lock lock1(mutex);
                        std::unique_lock lock2(mutex, std::try_to_lock);
                        auto owns_lock = lock2.owns_lock(); // вернет false т.к. mutex уже захвачен, но не будет блокировки, потому что try_lock не блокирует
                        for (int i = 0; i < 10; ++i)
                        {
                            std::this_thread::sleep_for(std::chrono::milliseconds(1));
                            std::cout << c;
                        }

                        std::cout << std::endl;
                        lock1.unlock(); // потому что далее идет задержка

                        std::this_thread::sleep_for(std::chrono::milliseconds(10));
                    };

                std::thread thread1(PrintSymbol, '+');
                std::thread thread2(PrintSymbol, '-');

                thread1.join();
                thread2.join();
                std::cout << std::endl;
            }
            // 2 Способ: std::try_to_lock - происходит попытка захвата mutex текущим потоком, НО НЕ блокирует доступ к данным другим потокам.
            {
                std::cout << "std::try_to_lock" << std::endl;
                auto PrintSymbol = [&mutex](char c)
                    {
                        while (true)
                        {
                            std::unique_lock lock(mutex, std::try_to_lock); // lock сразу же вынолнится для thread1, для thread2 - нет
                            if (lock.owns_lock())
                            {
                                for (int i = 0; i < 10; ++i)
                                {
                                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                                    std::cout << c;
                                }

                                std::cout << std::endl;
                                lock.unlock();
                                break;
                            }
                            else // thread2 зайдет сюда, пока thread1 обрабатывается
                            {
                                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                                std::cout << "std::unique_lock еще не освободился! Для потока: " << std::this_thread::get_id() << std::endl;
                            }
                        }
                    };

                std::thread thread1(PrintSymbol, '+');
                std::thread thread2(PrintSymbol, '-');

                thread1.join();
                thread2.join();
                std::cout << std::endl;
            }
            // 3 Способ: std::defer_lock - lock не выполнется в конструкторе.
            {
                std::cout << "std::defer_lock" << std::endl;
                auto PrintSymbol = [&mutex](char c)
                    {
                        std::unique_lock lock(mutex, std::defer_lock); // lock не выполнется
                        bool owns_lock = lock.owns_lock(); // false
                        std::this_thread::sleep_for(std::chrono::milliseconds(10));
                        lock.lock();
                        owns_lock = lock.owns_lock(); // true
                        for (int i = 0; i < 10; ++i)
                        {
                            std::this_thread::sleep_for(std::chrono::milliseconds(1));
                            std::cout << c;
                        }

                        std::cout << std::endl;
                    };

                std::thread thread1(PrintSymbol, '+');
                std::thread thread2(PrintSymbol, '-');

                thread1.join();
                thread2.join();
                std::cout << std::endl;
            }
            // 4 Способ: std::adopt_lock - считается lock сделан до этого.
            {
                // Неправильное использование: std::unique_lock cчитает, что std::mutex заблокирован, хотя это не так
                {
                    std::cout << "Неправильное использование std::adopt_lock: std::unique_lock cчитает, что std::mutex заблокирован, хотя это не так" << std::endl;
                    auto PrintSymbol = [&mutex](char c)
                        {
                            std::unique_lock lock(mutex, std::adopt_lock); // lock не выполнется
                            bool owns_lock = lock.owns_lock(); // true - это неверно, std::mutex - unlock
                            // lock.lock(); // будет deadlock, потому считается std::adopt_lock - уже lock, хотя std::mutex - unlock
                            mutex.lock();
                            owns_lock = lock.owns_lock(); // true - это верно, std::mutex - lock
                            for (int i = 0; i < 10; ++i)
                            {
                                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                                std::cout << c;
                            }

                            std::cout << std::endl;
                        };

                    std::thread thread1(PrintSymbol, '+');
                    std::thread thread2(PrintSymbol, '-');

                    thread1.join();
                    thread2.join();
                    std::cout << std::endl;
                }
                /*
                 Правильное использование: std::mutex уже lock
                 */
                {
                    std::cout << "Правильное использование std::adopt_lock: std::mutex уже lock" << std::endl;
                    auto PrintSymbol = [&mutex](char c)
                        {
                            mutex.lock();
                            std::unique_lock lock(mutex, std::adopt_lock); // lock не выполнется
                            bool owns_lock = lock.owns_lock(); // true - это верно, std::mutex - lock
                            for (int i = 0; i < 10; ++i)
                            {
                                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                                std::cout << c;
                            }

                            std::cout << std::endl;
                        };

                    std::thread thread1(PrintSymbol, '+');
                    std::thread thread2(PrintSymbol, '-');

                    thread1.join();
                    thread2.join();
                    std::cout << std::endl;
                }
            }
        }
        /*
         std::shared_lock - имеет возможности std::unique_lock, но для std::shared_timed_mutex. Для обычного std::mutex не подойдет.
         Делает общую блокировку (shared lock) - чтение данных из нескольких потоков одновременно. Если один поток получил общую блокировку, ни один другой поток не может получить эксклюзивную блокировку (exclusive lock) - запись данных только для одного потока одновременно, но может получить общую блокировку.
         */
        {
            std::cout << "std::shared_lock" << std::endl;
            std::shared_mutex mutex;
            int number = 0;
            
            auto Read = [&]()->int
            {
                std::shared_lock lock(mutex); // общая блокировка для чтения
                std::cout << "read: " << number << ", thread: " << std::this_thread::get_id() << std::endl;
                return number;
            };
            
            auto Write = [&](int iNumber)
            {
                std::unique_lock lock(mutex); // эксклюзивная блокировка для записи
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                number = iNumber;
                std::cout << "write: " << iNumber << ", thread: " << std::this_thread::get_id() << std::endl;
            };
            
            std::thread thread1(Read); // На время блокирует доступ для всех потоков в режиме записи, но для чтения - нет
            std::thread thread2(Write, 1); // На время блокирует доступ для всех потоков в режиме чтения/записи
            std::thread thread3(Read);
            std::thread thread4(Read);
            std::thread thread5(Read);
            
            thread1.join();
            thread2.join();
            thread3.join();
            thread4.join();
            thread5.join();
            
            std::cout << std::endl;
        }
    }
}
