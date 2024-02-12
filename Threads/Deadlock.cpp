#include "Deadlock.hpp"

#include <iostream>
#include <mutex>
#include <thread>

/*
 Deadlock — ситуация, в которой есть 2 mutex и 2 thread и 2 функции. В function1 идет mutex1.lock(), mutex2.lock(). В функции function2 идет обратная очередность mutex2.lock(), mutex1.lock(). Получается thread1 захватывает mutex1, thread2 захватывает mutex2 и возникает взаимная блокировка.
 Решение:
 1. Захватывать (lock) несколько мьютексов всегда в одинаковом порядке
 2. Отпускать (unlock) захваченные (lock) mutex в порядке LIFO («последним пришёл — первым ушёл»)
 3. Можно использовать алгоритм предотвращения взаимоблокировок
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
         Решение 3: std::try_lock - порядок std::mutexов важен, т.к.
         */
        {
            auto function1 = [&]()
                {
                    //std::try_lock(mutex1, mutex2);
                    std::lock(mutex1, mutex2);
                    std::lock_guard lock1(mutex1, std::adopt_lock);
                    std::this_thread::sleep_for(std::chrono::milliseconds(10)); // задержка, чтобы thread2 успел сделать lock в mutex2 в function2
                    std::lock_guard lock2(mutex2, std::adopt_lock);
                };
            auto function2 = [&]()
                {
                    std::lock(mutex1, mutex2); //
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
         Решение 5: std::scoped_lock - улучшенная версия std::lock_guard, конструктор которого блокирует произвольное количество мьютексов в фиксированном порядке, C++17
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
                    std::scoped_lock scoped_lock(mutex2, mutex1);
                    std::this_thread::sleep_for(std::chrono::milliseconds(10)); // задержка, чтобы thread1 успел сделать lock в mutex1
                };
            std::thread thread1(function1);
            std::thread thread2(function2);

            thread1.join();
            thread2.join();
        }
    }
}
