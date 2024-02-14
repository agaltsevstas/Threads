#include "Mutex.hpp"

#include <iostream>
#include <functional>
#include <mutex>
#include <ranges>
#include <thread>
#include <shared_mutex>


/*
 Сайты:
 https://en.cppreference.com/w/cpp/thread/shared_mutex
 https://stackoverflow.com/questions/46452973/difference-between-stdmutex-and-stdshared-mutex
 https://stackoverflow.com/questions/57706952/does-stdshared-mutex-favor-writers-over-readers
 */


namespace MUTEX
{
    void Start()
    {
        // Race condition/data race (состояние гонки) - обращение к общим данным в разных потоках одновременно
        {
            std::cout << "Race condition/data race (состояние гонки)" << std::endl;
            auto PrintSymbol = [](char c)
                {
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
         std::mutex — механизм синхронизации и потокобезопасности (thread safety), который предназначен для контроля доступа к общим данным для нескольких потоков с использованием барьеров памяти.
         Методы, внутри используются синхронизации уровня операционной системы (ОС):
         - lock - происходит захват mutex текущим потоком и блокирует доступ к данным другим потокам; или поток блокируется, если мьютекс уже захвачен другим потоком.
         - unlock - освобождение mutex, разблокирует данные другим потокам.
         Замечание: при повторном вызове lock - вылезет std::exception или приведет к состоянию бесконечного ожидания, при повторном вызове unlock - ничего не будет или вылезет std::exception.
         - try_lock - происходит захват mutex текущим потоком, НО НЕ блокирует доступ к данным другим потокам, а возвращает значение: true - можно захватить mutex / false - нельзя; НО МОЖЕТ возвращать ложное значение, потому что в момент вызова try_lock mutex может быть уже lock/unlock. Использовать try_lock в редких случаях, когда поток мог что-то сделать полезное, пока ожидает unlock.
         */
        {
            std::cout << "mutex" << std::endl;
            std::mutex mutex;
            /*
             - lock - происходит захват mutex текущим потоком и блокирует доступ к данным другим потокам; или поток блокируется, если mutex уже захвачен другим потоком.
             - unlock - освобождение mutex, разблокирует данные другим потокам.
             Замечание: При повторном вызове lock - вылезет std::exception или приведет к состоянию бесконечного ожидания, при повторном вызове unlock - ничего не будет или вылезет std::exception.
             */
            {
                std::cout << "std::mutex, lock - unlock" << std::endl;
                auto PrintSymbol = [&mutex](char c)
                    {
                        mutex.lock();
                        auto try_lock = mutex.try_lock(); // вернет false т.к. mutex уже захвачен, но не будет блокировки, потому что try_lock не блокирует
                        for (int i = 0; i < 10; ++i)
                        {
                            std::this_thread::sleep_for(std::chrono::milliseconds(1));
                            std::cout << c;
                        }
                        std::cout << std::endl;
                        mutex.unlock();
                    };

                std::thread thread1(PrintSymbol, '+');
                std::thread thread2(PrintSymbol, '-');

                thread1.join();
                thread2.join();
                std::cout << std::endl;
            }
            /*
             try_lock - происходит захват mutex текущим потоком, НО НЕ блокирует доступ к данным другим потокам, а возвращает значение: true - можно захватить mutex / false - нельзя; НО МОЖЕТ возвращать ложное значение, потому что в момент вызова try_lock mutex может быть уже lock/unlock.
            */
            {
#if defined (__APPLE__) || defined(__APPLE_CC__) || defined(__OSX__)
                // Неправильное использование: не синхронизирует данные, потому что НЕ блокирует доступ к данным другим потокам
                {
                    std::cout << "Неправильное использование try_lock: не синхронизирует данные, потому что НЕ блокирует доступ к данным другим потокам" << std::endl;
                    auto PrintSymbol = [&mutex](char c)
                        {
                            auto try_lock = mutex.try_lock();
                            // mutex.lock(); // будет deadlock, потому что try_lock сделал захват
                            std::cout << "try_lock: " << try_lock << ", в потоке: " << std::this_thread::get_id() << std::endl;
                            for (int i = 0; i < 10; ++i)
                            {
                                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                                std::cout << c;
                            }
                            std::cout << std::endl;
                            mutex.unlock(); // в Visual Studio падание при повторном использовании
                        };

                    std::thread thread1(PrintSymbol, '+');
                    std::thread thread2(PrintSymbol, '-');

                    thread1.join();
                    thread2.join();
                    std::cout << std::endl;
                }
#endif
                /*
                 Правильное использование: работает по аналогии lock, происходит проверка захвата mutex в цикле.
                 Плюсы:
                 - использовать try_lock в редких случаях, когда поток мог что-то сделать полезное, пока ожидает unlock.
                 Минусы:
                 - задержка в цикле
                 */
                {
                    std::cout << "Правильное использование try_lock: работает по аналогии lock, происходит проверка захвата mutex в цикле." << std::endl;
                    auto PrintSymbol = [&mutex](char c)
                        {
                            while (true)
                            {
                                if (mutex.try_lock())
                                {
                                    for (int i = 0; i < 10; ++i)
                                    {
                                        std::this_thread::sleep_for(std::chrono::milliseconds(1));
                                        std::cout << c;
                                    }
                                    std::cout << std::endl;
                                    mutex.unlock();
                                    break;
                                }
                                else
                                {
                                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                                    std::cout << "std::mutex еще не освободился!" << std::endl;
                                }
                            }
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
         std::recursive_mutex - использовать при рекурсии. Правило: кол-во lock = кол-во unlock. Если использовать обычный std::mutex, то при повторном вызове lock, либо будет deadlock, либо выскачит ошибка.
         */
        {
            std::cout << "std::recursive_mutex" << std::endl;
            std::recursive_mutex mutex;
            // 1 Способ: обычный
            {
                std::cout << "1 Способ: обычный" << std::endl;
                std::function<void(int)> Recursive = nullptr;
                Recursive = [&](int number)
                    {
                        mutex.lock();
                        std::cout << number << ' ';
                        if (number <= 1)
                        {
                            mutex.unlock();
                            std::cout << std::endl;
                            return;
                        }
                        Recursive(--number);
                        mutex.unlock();
                    };

                std::thread thread1(Recursive, 5);
                std::thread thread2(Recursive, 6);

                thread1.join();
                thread2.join();
                std::cout << std::endl;
            }
            // 2 Способ: std::lock_guard
            {
                std::cout << "2 Способ: std::lock_guard" << std::endl;
                std::function<void(int)> Recursive = nullptr;
                Recursive = [&](int number)
                    {
                        std::lock_guard lock(mutex);
                        std::cout << number << ' ';
                        if (number <= 1)
                        {
                            std::cout << std::endl;
                            return;
                        }
                        Recursive(--number);
                    };

                std::thread thread1(Recursive, 5);
                std::thread thread2(Recursive, 6);

                thread1.join();
                thread2.join();
                std::cout << std::endl;
            }
        }
        /*
         std::timed_mutex - по сравнению с std::mutex имеет дополнительные методы:
         - try_lock_for - блокирует доступ к данным другим потокам на ОПРЕДЕЛЕННОЕ ВРЕМЯ и возвращает true - произошел захват mutex текущим потоком/false - нет; НО МОЖЕТ возвращать ложное значение, потому что в момент вызова try_lock_for std::timed_mutex может быть уже lock/unlock.
         - try_lock_until - блокирует выполнение текущего потока до НАСТУПЛЕНИЕ МОМЕНТА ВРЕМЕНИ (например, 11:15:00) и возвращает true - произошел захват mutex текущим потоком/false - нет; НО МОЖЕТ возвращать ложное значение, потому что в момент вызова try_lock_until std::timed_mutex может быть уже lock/unlock.
         */
        {
            std::cout << "std::timed_mutex" << std::endl;
            std::timed_mutex mutex;

            // Неправильное использование: unlock разблокирует lock для других потоков, не дожидаясь времени
            {
                std::cout << "Неправильное использование try_lock_for: unlock разблокирует lock не дожидаясь времени" << std::endl;
                auto PrintSymbol = [&mutex](char c)
                    {
                        auto try_lock_for = mutex.try_lock_for(std::chrono::milliseconds(11)); // нет проверки на захват std::timed_mutex
                        for (int i = 0; i < 10; ++i)
                        {
                            std::this_thread::sleep_for(std::chrono::milliseconds(10));
                            std::cout << c;
                        }

                        std::cout << std::endl;
                        mutex.unlock(); // немедленно разблокирует для других потоков
                    };

                std::thread thread1(PrintSymbol, '+');
                std::thread thread2(PrintSymbol, '-');

                thread1.join();
                thread2.join();
                std::cout << std::endl;
            }
            /*
             Правильное использование: работает по аналогии lock, происходит проверка захвата mutex в цикле.
             Плюсы:
             - использовать try_lock_for/try_lock_until в случаях, когда поток мог что-то сделать полезное, пока ожидает unlock.
             Минусы:
             - задержка в цикле
             */
            {
                // 1 Способ: try_lock_for
                {
                    std::cout << "Правильное использование try_lock_for: unlock разблокирует lock через ОПРЕДЕЛЕНОЕ ВРЕМЯ" << std::endl;
                    auto PrintSymbol = [&mutex](char c)
                        {
                            while (true)
                            {
                                if (mutex.try_lock_for(std::chrono::milliseconds(12))) // потоки ждут
                                {
                                    for (int i = 0; i < 10; ++i)
                                    {
                                        std::this_thread::sleep_for(std::chrono::milliseconds(1));
                                        std::cout << c;
                                    }
                                    std::cout << std::endl;
                                    mutex.unlock();
                                    break;
                                }
                                else // НЕ зайдет сюда, т.к. время в try_lock_until > время в цикле
                                {
                                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                                    std::cout << "std::timed_mutex еще не освободился! Для потока: " << std::this_thread::get_id() << std::endl;
                                }
                            }
                        };

                    std::thread thread1(PrintSymbol, '+');
                    std::thread thread2(PrintSymbol, '-');

                    thread1.join();
                    thread2.join();
                    std::cout << std::endl;
                }
                // 2 Способ: try_lock_until
                {
                    std::cout << "Правильное использование try_lock_until: unlock разблокирует lock до НАСТУПЛЕНИЯ МОМЕНТА ВРЕМЕНИ (например, 11:15:00)" << std::endl;
                    auto PrintSymbol = [&mutex](char c)
                        {
                            while (true)
                            {
                                if (mutex.try_lock_until(std::chrono::system_clock::now() + std::chrono::milliseconds(10))) // потоки ждут
                                {
                                    for (int i = 0; i < 10; ++i)
                                    {
                                        std::this_thread::sleep_for(std::chrono::milliseconds(2));
                                        std::cout << c;
                                    }
                                    std::cout << std::endl;
                                    mutex.unlock();
                                    break;
                                }
                                else // зайдет сюда, т.к. время в try_lock_until < время в цикле
                                {
                                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                                    std::cout << "std::timed_mutex еще не освободился! Для потока: " << std::this_thread::get_id() << std::endl;
                                }
                            }
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
         std::recursive_timed_mutex - обладает свойствами std::recursive_mutex + std::timed_mutex.
         */
        {
            std::cout << "std::recursive_timed_mutex" << std::endl;
            std::recursive_timed_mutex mutex;

            std::function<void(int)> Recursive = nullptr;
            Recursive = [&](int number)
                {
                    if (mutex.try_lock_for(std::chrono::milliseconds(5))) // thread2 не зайдет сюда
                    {
                        std::lock_guard lock(mutex);
                        std::this_thread::sleep_for(std::chrono::milliseconds(1));
                        std::cout << number << ' ';
                        if (number <= 1)
                        {
                            std::cout << std::endl;
                            return;
                        }
                        Recursive(--number);
                    }
                    else // thread2 зайдет сюда, т.к. std::recursive_timed_mutex занят std::thread1
                    {
                        std::this_thread::sleep_for(std::chrono::milliseconds(10));
                        std::cout << "std::recursive_timed_mutex еще не освободился! Для потока: " << std::this_thread::get_id() << std::endl;
                    }
                };

            std::thread thread1(Recursive, 5);
            std::thread thread2(Recursive, 6);

            std::cout << "Запустился поток: " << thread1.get_id() << std::endl;
            std::cout << "Запустился поток: " << thread2.get_id() << std::endl;

            thread1.join();
            thread2.join();
            std::cout << std::endl;
        }
        /*
         std::shared_mutex - имеет 2 разных блокировки:
         - эксклюзивная блокировка (exclusive lock) - запись данных только для одного потока одновременно. Если один поток получил эксклюзивный доступ (через lock, try_lock), то никакие другие потоки не могут получить блокировку (включая общую).
           Работает по аналогии std::mutex. Методы:
           - lock
           - unlock
           - try_lock
         - общая блокировка (shared lock) - чтение данных из нескольких потоков одновременно. Если один поток получил общую блокировку (через lock_shared, try_lock_shared), ни один другой поток не может получить эксклюзивную блокировку, но может получить общую блокировку.
           Методы:
           - shared_lock
           - unlock_shared
           - try_lock_shared
         Замечание: в пределах одного потока одновременно может быть получена только одна блокировка (общая или эксклюзивная).
         Плюсы:
         - можно выделить участок кода, где будет происходить чтение из нескольких потоков.
         - синхронизирует данные при записи/чтении, не вызывая Race condition/data race (состояние гонки). При записи данных одним потоком, чтение/запись данных блокируется для всех потоков. При чтении данных одним потоком, чтение данных - доступно для всех потоков, запись данных блокируется для всех потоков.
         - параллелизм чтения и общая блокировка для чтения, которые недоступны при обычном std::mutex.
         - отсутствие Livelock (голодание потоков) - ситуация, в которой поток не может получить доступ к общим ресурсам, потому что на эти ресурсы всегда претендуют какие-то другие потоки.
         Минусы:
         - больше весит, чем std::mutex.
         */
        {
            // Неправильное использование без std::shared_mutex: рассинхронизация чтения/записи при использовании обычного std::mutex
            {
                std::cout << "Неправильное использование без std::shared_mutex: рассинхронизация чтения/записи при использовании обычного std::mutex" << std::endl;
                std::mutex mutex;
                int number = 0;
                
                auto Read = [&]()->int
                {
                    std::cout << "read: " << number << ", thread: " << std::this_thread::get_id() << std::endl;
                    return number;
                };
                
                auto Write = [&](int iNumber)
                {
                    std::unique_lock lock(mutex);
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                    number = iNumber;
                    std::cout << "write: " << iNumber << ", thread: " << std::this_thread::get_id() << std::endl;
                };
                
                std::thread thread1(Read);
                std::thread thread2(Write, 1); // На время блокирует доступ для всех потоков ТОЛЬКО в режиме записи
                std::thread thread3(Read);
                std::thread thread4(Read);
                std::this_thread::sleep_for(std::chrono::milliseconds(1)); // задержка, чтобы thread2 успел записать
                std::thread thread5(Read);
                
                thread1.join();
                thread2.join();
                thread3.join();
                thread4.join();
                thread5.join();
                
                std::cout << std::endl;
            }
            // Правильное использование std::shared_mutex: синхронизация чтения/записи
            {
                std::cout << "Правильное использование std::shared_mutex: синхронизация чтения/записи" << std::endl;
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
        /*
         std::shared_timed_mutex - обладает свойствами std::shared_mutex + std::timed_mutex.
        */
        {
            std::cout << "std::shared_timed_mutex" << std::endl;
            std::shared_timed_mutex mutex;
            int number = 0;
            
            auto Read = [&]()->int
            {
                while (true)
                {
                    if (mutex.try_lock_shared_for(std::chrono::milliseconds(2))) // попытка общей блокировки для чтения
                    {
                        std::shared_lock(mutex, std::adopt_lock);
                        std::cout << "read: " << number << ", thread: " << std::this_thread::get_id() << std::endl;
                        return number;
                    }
                    else
                    {
                        std::this_thread::sleep_for(std::chrono::milliseconds(1));
                        std::cout << "std::shared_timed_mutex еще не освободился для чтения! Потока: " << std::this_thread::get_id() << std::endl;
                    }
                }
            };
            
            auto Write = [&](int iNumber)
            {
                while (true)
                {
                    if (mutex.try_lock_until(std::chrono::system_clock::now() + std::chrono::milliseconds(2))) // попытка эксклюзивной блокировки для записи
                    {
                        std::unique_lock(mutex, std::adopt_lock);
                        number = iNumber;
                        std::cout << "write: " << iNumber << ", thread: " << std::this_thread::get_id() << std::endl;
                        break;
                    }
                    else
                    {
                        std::this_thread::sleep_for(std::chrono::milliseconds(1));
                        std::cout << "std::shared_timed_mutex еще не освободился для записи! Потока: " << std::this_thread::get_id() << std::endl;
                    }
                    
                }
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
