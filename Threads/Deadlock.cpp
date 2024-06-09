#include "Deadlock.hpp"

#include <iostream>
#include <mutex>
#include <thread>


/*
 Сайты: https://stackoverflow.com/questions/50286056/why-stdlock-supports-deallock-avoidence-but-stdtry-lock-does-not
 */

/* Виды блокировок:
 1. Livelock - голодание потоков.
 2. Starvation - нехватка ресурсов для потока.
 3. Deadlock - взаимная блокировка mutex.
 Отличия:
 В Livelock потоки находятся в состоянии ожидания и выполняются одновременно. Livelock — это частный случай Starvation - процесс в потоке не прогрессирует.
 В Deadlock потоки находятся в состоянии ожидания.
 */

 /*
 1. Livelock (голодание потоков) - ситуация, в которой поток не может получить доступ к общим ресурсам, потому что на эти ресурсы всегда претендуют какие-то другие потоки, которым отдаётся предпочтение. Потоки не блокируются — они выполняют не столь полезные действия.
 Решение: Расставлять приоритеты потоков и правильно использовать mutex.

 2. Starvation (голодание) - поток не может получить все ресурсы, необходимые для выполнения его работы.
 
 3. Deadlock — ситуация, в которой есть 2 mutex и 2 thread и 2 функции. В function1 идет mutex1.lock(), mutex2.lock(). В функции function2 идет обратная очередность mutex2.lock(), mutex1.lock(). Получается thread1 захватывает mutex1, thread2 захватывает mutex2 и возникает взаимная блокировка.
 Решение До С++17:
 1. Захватывать (lock) несколько мьютексов всегда в одинаковом порядке.
 2. Отпускать (unlock) захваченные (lock) mutex в порядке LIFO («последним пришёл — первым ушёл»).
 3. Можно использовать алгоритм предотвращения взаимоблокировок std::lock, порядок std::mutexов неважен:
 3.1:
 std::lock(mutex1, mutex2);
 std::lock_guard lock1(mutex1, std::adopt_lock); // считается lock сделан до этого
 std::lock_guard lock2(mutex2, std::adopt_lock); // считается lock сделан до этого
 3.2:
 std::unique_lock lock1(mutex1, std::defer_lock); // lock не выполнется
 std::unique_lock lock2(mutex2, std::defer_lock); // lock не выполнется
 std::lock(lock1, lock2);
 4. std::try_lock - пытается захватить (lock) в очередном порядке каждый std::mutex.
 При успешном захвате всех переданных std::mutex возвращает -1.
 При неуспешном захвате всех std::mutexoв, все std::mutexы освобождаются и возвращается индекс (0,1,2..) первого std::mutex, которого не удалось заблокировать.
 Замечание: при повторном захвате все std::mutex вызовется unlock.
 При исключении в std::try_lock вызывается unlock для всех std::mutex.
 Порядок std::mutexов неважен:
 if (std::try_lock(mutex1, mutex2) == -1)
 {
     std::lock_guard lock1(mutex1, std::adopt_lock); // считается lock сделан до этого
     std::lock_guard lock2(mutex2, std::adopt_lock); // считается lock сделан до этого
 }
 Решение C++17:
 std::scoped_lock - улучшенная версия std::lock_guard, конструктор которого делает захват (lock) произвольного кол-во мьютексов в очередном порядке и высвобождает (unlock) при выходе из стека в деструкторе, использование идиомы RAII.
 Замечание: отсутствует конструктор копироввания
 std::scoped_lock scoped_lock(mutex1, mutex2);
 */

namespace deadlock
{
    void start()
    {
        std::cout << "Deadlock" << std::endl;
        std::mutex mutex1;
        std::mutex mutex2;
        /*
         Решение 1:
         1. Захватывать (lock) несколько мьютексов всегда в одинаковом порядке.
         2. Отпускать (unlock) захваченные (lock) mutex в порядке LIFO («последним пришёл — первым ушёл»).
         */
        {
            auto function1 = [&]()
                {
                    std::lock_guard lock1(mutex1);
                    std::this_thread::sleep_for(std::chrono::milliseconds(10)); // задержка, чтобы thread2 успел сделать lock в mutex2 в function2
                    std::lock_guard lock2(mutex2);
                };
            auto function2 = [&]()
                {
                    std::lock_guard lock1(mutex1);
                    // std::lock_guard lock1(mutex2); // Если будет первым, то будет deadlock
                    std::this_thread::sleep_for(std::chrono::milliseconds(10)); // задержка, чтобы thread1 успел сделать lock в mutex1
                    // std::lock_guard lock2(mutex1); // Если будет вторым, то будет deadlock
                    std::lock_guard lock2(mutex2);
                };
            std::thread thread1(function1);
            std::thread thread2(function2);

            thread1.join();
            thread2.join();
        }
        /*
         Решение 2: std::lock - порядок std::mutexов неважен
         */
        {
            /*
             1 Способ: std::lock (вызывает lock у mutexов) + std::adopt_lock (считается lock сделан до этого) в std::lock_guard, до C++17
             */
            {
                auto function1 = [&]()
                    {
                        std::lock(mutex1, mutex2);
                        std::lock_guard lock1(mutex1, std::adopt_lock);
                        std::this_thread::sleep_for(std::chrono::milliseconds(10)); // задержка, чтобы thread2 успел сделать lock в mutex2 в function2
                        std::lock_guard lock2(mutex2, std::adopt_lock);
                    };
                auto function2 = [&]()
                    {
                        std::lock(mutex2, mutex1);  // порядок неважен
                        std::lock_guard lock1(mutex2, std::adopt_lock);
                        std::this_thread::sleep_for(std::chrono::milliseconds(10)); // задержка, чтобы thread1 успел сделать lock в mutex1
                        std::lock_guard lock2(mutex1, std::adopt_lock);
                    };
                std::thread thread1(function1);
                std::thread thread2(function2);

                thread1.join();
                thread2.join();
            }
            /*
             2 Способ: std::defer_lock (lock не выполнется) в std::unique_lock + std::lock (вызывает lock у mutexов), до C++17
             */
            {
                auto function1 = [&]()
                {
                    std::unique_lock lock1(mutex1, std::defer_lock);
                    std::unique_lock lock2(mutex2, std::defer_lock);
                    std::lock(lock1, lock2);
                    std::this_thread::sleep_for(std::chrono::milliseconds(10)); // задержка, чтобы thread2 успел сделать lock в mutex2 в function2
                };
                auto function2 = [&]()
                {
                    std::unique_lock lock1(mutex1, std::defer_lock);
                    std::unique_lock lock2(mutex2, std::defer_lock);
                    std::lock(lock2, lock1); // порядок неважен
                    std::this_thread::sleep_for(std::chrono::milliseconds(10)); // задержка, чтобы thread1 успел сделать lock в mutex1
                };
                std::thread thread1(function1);
                std::thread thread2(function2);

                thread1.join();
                thread2.join();
            }
        }
        /*
         Решение 3: std::try_lock - пытается захватить (lock) в очередном порядке каждый std::mutex.
         При успешном захвате всех переданных std::mutex возвращает -1.
         При неуспешном захвате всех std::mutexoв, все std::mutexы освобождаются и возвращается индекс (0,1,2..) первого std::mutex, которого не удалось заблокировать.
         Замечание: при повторном захвате все std::mutex вызовется unlock.
         При исключении в std::try_lock вызывается unlock для всех std::mutex.
         Порядок std::mutexов неважен.
         */
        {
            /*
             Правильное использование: std::try_lock блокирует все std::mutex
             Плюсы:
             - использовать try_lock_for/try_lock_until в случаях, когда поток мог что-то сделать полезное, пока ожидает unlock.
             Минусы:
             - задержка в цикле
             */
            {
                std::cout << "Правильное использование: std::try_lock блокирует все std::mutex" << std::endl;
                auto function1 = [&]()
                {
                    while (true)
                    {
                        if (std::try_lock(mutex1, mutex2) == -1)
                        {
                            std::lock_guard lock1(mutex1, std::adopt_lock);
                            std::this_thread::sleep_for(std::chrono::milliseconds(10)); // задержка, чтобы thread2 успел сделать lock в mutex2 в function2
                            std::cout << "function1" << std::endl;
                            std::lock_guard lock2(mutex2, std::adopt_lock);
                            break;
                        }
                        else
                        {
                            std::this_thread::sleep_for(std::chrono::milliseconds(10));
                            std::cout << "std::try_lock еще не освободил std::mutexы! Для потока: " << std::this_thread::get_id() << std::endl;
                        }
                    }
                };
                auto function2 = [&]()
                {
                    while (true)
                    {
                        if (std::try_lock(mutex1, mutex2) == -1)
                        {
                            std::lock_guard lock1(mutex2, std::adopt_lock);
                            std::this_thread::sleep_for(std::chrono::milliseconds(10)); // задержка, чтобы thread1 успел сделать lock в mutex1
                            std::cout << "function2" << std::endl;
                            std::lock_guard lock2(mutex1, std::adopt_lock);
                            break;
                        }
                        else
                        {
                            std::this_thread::sleep_for(std::chrono::milliseconds(10));
                            std::cout << "std::try_lock еще не освободил std::mutexы! Для потока: " << std::this_thread::get_id() << std::endl;
                        }
                    }
                };
                
                std::thread thread1(function1);
                std::thread thread2(function2);

                thread1.join();
                thread2.join();
            }
            // Неправильное использование: std::try_lock сначала блокирует, а потом разблокирует std::mutex
            {
                std::cout << "Неправильное использование: std::try_lock сначала блокирует, а потом разблокирует std::mutex" << std::endl;
                
                auto PrintSymbol1 = [&](char c)
                {
                    std::lock_guard lock(mutex2); // Блокирование mutex2
                    auto index = std::try_lock(mutex1, mutex2); // Разблокирование mutex2 - 1 индекс
                    std::cout << "PrintSymbol1, Разблокировка всех std::mutex, начиная с index: " << index << std::endl;
                    for (int i = 0; i < 10; ++i)
                    {
                        std::this_thread::sleep_for(std::chrono::milliseconds(1));
                        std::cout << c;
                    }
                    std::cout << std::endl;
                };
                auto PrintSymbol2 = [&](char c)
                {
                    std::this_thread::sleep_for(std::chrono::milliseconds(1)); // задержка, чтобы thread1 успел сделать lock в mutex2
                    std::lock_guard lock(mutex1); // Блокирование mutex1
                    auto index = std::try_lock(mutex2, mutex1); // Разблокирование mutex2 - 0 индекс, потому что в thread1 std::try_lock захватил mutex1 и mutex2
                    std::cout << "PrintSymbol2, Разблокировка всех std::mutex, начиная с index: " << index << std::endl;
                    for (int i = 0; i < 10; ++i)
                    {
                        std::this_thread::sleep_for(std::chrono::milliseconds(1));
                        std::cout << c;
                    }
                    std::cout << std::endl;
                };
                
                std::thread thread1(PrintSymbol1, '+');
                std::thread thread2(PrintSymbol2, '-');

                thread1.join();
                thread2.join();
                std::cout << std::endl;
            }
            // Неправильное использование: std::try_lock сначала блокирует, а потом разблокирует std::mutex
            {
                std::cout << "Неправильное использование: std::try_lock сначала блокирует, а потом разблокирует std::mutex" << std::endl;
                
                auto PrintSymbol = [&](char c)
                {
                    auto index = std::try_lock(mutex1, mutex2);
                    if (index == -1)
                    {
                        std::cout << "Блокировка всех std::mutex, index: " << index << std::endl;
                        mutex1.unlock();
                        mutex2.unlock();
                    }
                    else
                        std::cout << "Разблокировка всех std::mutex, начиная с index: " << index << std::endl;
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
        /*
         Решение 5: std::scoped_lock - улучшенная версия std::lock_guard, конструктор которого делает захват (lock) произвольного кол-во мьютексов в очередном порядке и высвобождает (unlock) при выходе из стека в деструкторе, использование идиомы RAII.
         Замечание:
         - нельзя копировать
         */
        {
            auto function1 = [&]()
                {
                    std::scoped_lock scoped_lock(mutex1, mutex2);
                    std::this_thread::sleep_for(std::chrono::milliseconds(10)); // задержка, чтобы thread2 успел сделать lock в mutex2 в function2
                };
            auto function2 = [&]()
                {
                    std::scoped_lock scoped_lock(mutex2, mutex1); // порядок неважен
                    std::this_thread::sleep_for(std::chrono::milliseconds(10)); // задержка, чтобы thread1 успел сделать lock в mutex1
                };
            std::thread thread1(function1);
            std::thread thread2(function2);

            thread1.join();
            thread2.join();
        }
    }
}
