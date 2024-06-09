#include "Promise_Future.hpp"
#include "Timer.h"

#include <iostream>
#include <future>
#include <numeric>
#include <vector>

#if defined (__APPLE__) || defined(__APPLE_CC__) || defined(__OSX__)
    #include <boost/asio.hpp>
#endif

/*
 Видео: https://www.youtube.com/watch?v=g7dno0SupKY&list=WL&index=1&ab_channel=C%2B%2BUserGroup
        https://www.youtube.com/watch?v=DQ72ZyPqHRc
        TODO: сделать умный future как в видео
 */

namespace Promise_Future
{
    int Multi1(int number, int factor)
    {
        return number * factor;
    }

    void Start()
    {
        Timer timer;
        constexpr int size = 100;
        std::vector<int> numbers(size);
        std::iota(numbers.begin(), numbers.end(), 1);

        // Race condition/data race (состояние гонки) - обращение к общим данным в разных потоках одновременно
        {
            std::cout << "Race condition/data race (состояние гонки) - обращение к общим данным в разных потоках одновременно" << std::endl;
            {
                auto PrintSymbol = [](char c)
                    {
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
            /*
             std::future - интерфейс чтения результата, который вычислится в будущем. Работает по pull-модели, идет опрос, готовы ли данные. Позволяет не использовать std::mutex, std::condition_variable. В деструкторе вызывает метод get.
             Методы:
             - get - блокирует выполнение потока до появления вычисления результата из другого потока, он должен вызываться в том же потоке, что и объект std::future, иначе terminate. Может вывести исключение сохраненное в std::promise с помощью метода set_exception.
             - valid - можно переодически опрашивать готов ли std::future
             - share - создает shared_future, который может вызывать get в другом потоке. Может вывести исключение сохраненное в std::promise с помощью метода set_exception.
             - swap -
             - wait - встанет и будет ждать
             - wait_for - готов ли результат через определенное время и возвращает future_status(ready, timeout, deferred).
             блокирует доступ к данным другим потокам на ОПРЕДЕЛЕННОЕ ВРЕМЯ и возвращает true - произошел захват mutex текущим потоком/false - нет; НО МОЖЕТ возвращать ложное значение, потому что в момент вызова try_lock_for std::timed_mutex может быть уже lock/unlock.
             - wait_until - спрашивает готов ли результат или нет в определенное время и возвращает future_status(ready, timeout, deferred)
             блокирует выполнение текущего потока до НАСТУПЛЕНИЕ МОМЕНТА ВРЕМЕНИ (например, 11:15:00) и возвращает true - произошел захват mutex текущим потоком/false - нет; НО МОЖЕТ возвращать ложное значение, потому что в момент вызова try_lock_until std::timed_mutex может быть уже lock/unlock.
             */
            {
                /*
                 1 Способ: std::promise - интерфейс для записи результата, который вычисляется в отедельном потоке. std::promise сулжит каналом связи между текущим и основным потоками. Он более удобный, чем std::mutex + std::condition_variable.
                 Методы:
                 - set_value - сохраняет значение, которое можно запросить с помощью связанного объекта std::future.
                 - get_future - получение другого канал передачи std::future.
                 - set_exception - можно сохранить указатель на исключение (exception_ptr) и вывести исключение с помощью std::future метода get.
                 */
                {
                    std::cout << "1 Способ: std::promise" << std::endl;
                    
                    // 1 Способ: обычный
                    {
                        std::cout << "1 Способ: обычный" << std::endl;
                        auto Function = [](int factor, std::promise<int> result)
                            {
                                int sum = 0;
                                for (int i = 0; i < 10; ++i)
                                {
                                    sum += Multi1(i, factor);
                                }
                                result.set_value(sum);
                            };

                        std::promise<int> promise1, promise2;
                        std::future<int> future1 = promise1.get_future();
                        std::future<int> future2 = promise2.get_future();
                        std::thread thread1(Function, 2, std::move(promise1));
                        std::thread thread2(Function, 3, std::move(promise2));

                        // Если результат готов, то получение результата. Если результат не готов, то lock, до тех пор, пока результат не будет готов
                        std::cout << "future1: " << future1.get() << std::endl;
                        std::cout << "future2: " << future2.get() << std::endl;
                        
                        thread1.join();
                        thread2.join();
                    }
                    // 2 Способ: чтение future из разных потоков
                    {
                        std::cout << "2 Способ: чтение future из разных потоков" << std::endl;
                        // Неверный способ: чтение future из разных потоков - будет exception.
                        {
                            /*
                             std::promise<int> promise;
                             std::future<int> future = promise.get_future();
                             
                             std::thread thread1([&future]()
                             {
                                 std::cout << "thread1, future: " << future.get() << std::endl;
                             });
                             
                             std::thread thread2([&future]()
                             {
                                 std::cout << "thread2, future: " << future.get() << std::endl;
                             });
                             
                             promise.set_value(10);
                             thread1.join();
                             thread2.join();
                             */
                        }
                        // Правильный способ: чтение shared future из разных потоков - НЕ будет exception.
                        {
                            std::promise<int> promise;
                            std::future<int> future = promise.get_future();
                            auto shared_future = future.share();
                            
                            std::thread thread1([&shared_future]()
                            {
                                std::cout << "thread1, future: " << shared_future.get() << std::endl;
                            });
                            
                            std::thread thread2([&shared_future]()
                            {
                                std::cout << "thread2, future: " << shared_future.get() << std::endl;
                            });
                            
                            promise.set_value(10);
                            thread1.join();
                            thread2.join();
                        }
                    }
                }
                /* 2 Способ: std::packaged_task - оборачивает функцию и будет спрашивать результат сам. Работает как std::function + std::promise. Более удобный, чем std::promise.
                 */
                {
                    std::cout << "2 Способ: std::packaged_task" << std::endl;
                    auto Function = [](int factor)->int
                        {
                            int sum = 0;
                            for (int i = 0; i < 10; ++i)
                            {
                                sum += Multi1(i, factor);
                            }
                            
                            return sum;
                        };

                    std::packaged_task<int(int)> task1(Function);
                    std::packaged_task<int(int)> task2(Function);
                    std::future<int> future1 = task1.get_future();
                    std::future<int> future2 = task2.get_future();
                    std::thread thread1(std::move(task1), 2);
                    std::thread thread2(std::move(task2), 3);

                    // Если результат готов, то получение результата. Если результат не готов, то lock, до тех пор, пока результат не будет готов
                    std::cout << "future1: " << future1.get() << std::endl;
                    std::cout << "future2: " << future2.get() << std::endl;
                    
                    thread1.join();
                    thread2.join();
                }
                /*
                 3 Способ: std::async. Самый продвинутый способ: не нужны std::mutex, std::promise, std::packaged_task. Внутри std::async скорее всего использует Threadpool для создания потоков и аналог std::packaged_task и во std::future переводит. Если результат std::future работы std::async не сохранять, тогда деструктор std::future заблокируется в конце выражения до тех пор, пока асинхронная операция не будет завершена, т.е. std::async будет выполняться не асинхронно, а просто последовательно в однопоточном режиме. При сохранении результатов объекта std::future, вызов деструктора объекта std::future будет отложен, он будет разрушен при выходе из стека. Внутри деструктора std::future вызывают std::get и дожидается окончания асинхронного результата.
                 Стратегии запуска:
                 - std::launch::async - другой поток.
                 - std::launch::deferred - текущий поток.
                 - по умолчанию std::async выберет стратегию в зависимости от загруженности потоков, но лучше на это не полагаться.
                 Минусы:
                 - std::async возвращает std::future которое содержит возвращаемое значение, которое будет вычисляться функцией. Когда это future будет уничтожено, он ждет, пока поток не завершится, делая ваш код эффективно одиночным. Это легко упустить из виду, когда вам не требуется возвращаемое значение.
                 */
                {
                    std::cout << "3 Способ: std::async + std::launch::async" << std::endl;
                    std::cout << "Стратегии запуска:" << std::endl;
                    std::cout << "- std::launch::async - другой поток" << std::endl;
                    std::cout << "- std::launch::deferred - текущий поток" << std::endl;
                    
                    std::future<int> future1 = std::async(std::launch::async, Multi1, 10, 2);
                    std::future<int> future2 = std::async(std::launch::deferred, Multi1, 11, 2);
                    std::future<int> future3 = std::async(Multi1, 11, 2);
                    
                    std::cout << "future1 в другом потоке: " << future1.get() << std::endl;
                    std::cout << "future2 в текущем потоке: " << future2.get() << std::endl;
                    std::cout << "future3 по-умолчанию: " << future3.get() << std::endl;
                }
                // 4 Способ: Threadpool - на каждоый итерации создается поток, кол-во потоков зависит от ядер на процессоре, потоки будут делить процессорное время и мешать друг другу, поэтому может долго выполняться.
                {
                    std::cout << "4 Способ: Threadpool" << std::endl;
                    std::vector<std::future<int>> futures;
                    futures.reserve(10);
                    int sum = 0;
                    
                    timer.start();
                    for (int i = 0; i < 10; ++i)
                        futures.emplace_back(std::async(std::launch::async, Multi1, i, 2));

                    for (auto& future : futures)
                        sum += future.get();

                    timer.stop();
                    std::cout << "1 Способ Future, Сумма: " << sum << " Время: " << timer.elapsedMilliseconds() << " мс" << std::endl;
                }
                // 5 Способ: Threadpool + lambda
                {
                    std::cout << "4 Способ: Threadpool + lambda" << std::endl;
                    std::vector<std::future<int>> futures;
                    futures.reserve(10);
                    int sum = 0;

                    timer.start();
                    for (int i = 0; i < size; ++i)
                    {
                        futures.emplace_back(std::async(std::launch::async, [numbers, i]()->int
                            {
                                return numbers[i];
                            }));
                    }

                    for (auto& future : futures)
                        sum += future.get();

                    timer.stop();
                    std::cout << "3 Способ Future, Сумма: " << sum << " Время: " << timer.elapsedMilliseconds() << " мс" << std::endl;
                }
                
                std::cout << std::endl;
            }
#if defined (__APPLE__) || defined(__APPLE_CC__) || defined(__OSX__)
            // 6 Способ: Push - модель (брокеры сообщений, очереди сообщений). Посчитать вычисления в другом потоке и этот поток уведомил (notification) о результате в основной поток. По в C++ пока нет ничего, только в boost::asio
            {
                boost::asio::io_service io;
                
                std::cout << "6 Способ: очередь событий/сообщений" << std::endl;
                auto Function = [&io](const std::function<void(int&)>& handler, int factor)
                    {
                        std::cout << "Рассчет значения в другом потоке: " << std::this_thread::get_id() << std::endl;
                        int sum = 0;
                        for (int i = 0; i < 10; ++i)
                        {
                            sum += Multi1(i, factor);
                        }
                        
                        io.post(std::bind(handler, sum)); // иногда не работает
                    };
                
                auto PrintResult = [](int& result)
                {
                    std::cout << "Result: " << result << ", в потоке: " << std::this_thread::get_id() << std::endl;
                };

                std::thread thread1(Function, PrintResult, 2);
                std::thread thread2(Function, PrintResult, 3);
                
                std::cout << "Разгребание очереди сообщений в потоке: " << std::this_thread::get_id() << std::endl;
                io.run();
                thread1.join();
                thread2.join();
            }
            std::cout << std::endl;
#endif
        }
        /*
         std::shared_future - TODO
         */
        {
            
        }
        
        std::cout << std::endl;
    }
}
