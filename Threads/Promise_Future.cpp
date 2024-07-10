#include "Promise_Future.hpp"
#include "Timer.h"

#include <iostream>
#include <future>
#include <numeric>
#include <vector>
#include <future>
#include <utility>
#include <type_traits>

#if defined (__APPLE__) || defined(__APPLE_CC__) || defined(__OSX__)
    #include <boost/asio.hpp>
#endif

/*
 Видео: https://www.youtube.com/watch?v=g7dno0SupKY&list=WL&index=1&ab_channel=C%2B%2BUserGroup
        https://www.youtube.com/watch?v=DQ72ZyPqHRc
 */

namespace then
{
/// Функция,  осуществляющая запуск цепочки  (pipeline)  выполнения в будущем последовательных асинхронных
/// Класс, связывающий результат значения в будущем с настоящим временем.
 
    /*
     Лекции: https://www.youtube.com/watch?v=g7dno0SupKY&t=2022s
             https://www.youtube.com/watch?v=DQ72ZyPqHRc&t=1919s
             
     */
    /*
     Реализация Then: https://github.com/jaredhoberock/then
                      https://scottroy.github.io/futures.html https://github.com/tirimatangi/Lazy/tree/main
                      https://gist.github.com/iain17/529a091bb18dc76f936ff24edcac981c
                      https://gist.github.com/hikarin522/e60bccbd424df5ed6f1298e85c519ea4
                      https://github.com/thousandeyes/thousandeyes-futures/blob/master/include/thousandeyes/futures/then.h
           
     */

    namespace first_implementation
    {
        /*
         Класс, связывающий результат значения в будущем с настоящим временем. Then - осуществляет запуск цепочки  (pipeline) выполнения в будущем последовательных асинхронных.
         Получение возвращаемого значения из std::function: https://ru.stackoverflow.com/questions/1306255
         */
        template <class T>
        class Future
        {
        public:
            
            T Get()
            {
                return _future.get();
            }

            template <class TFunction>
            auto Then(TFunction&& function)
            {
                return Then(std::launch::async | std::launch::deferred, std::forward<TFunction>(function));
            }
            
            template <class TFunction>
            auto Then(TFunction&& function, std::launch policy)
            {
                return Then(policy, std::forward<TFunction>(function));
            }
            
        public:
            template <class TFunction>
            auto Then(std::launch policy, TFunction&& function)
            {
                Future<decltype(std::forward<TFunction>(function)(_future.get()))> future;
                
                future._future = std::async(policy, [](std::future<T>&& future, auto&& function)
                {
                    future.wait();
                    return std::invoke(function, future.get());
                },
                std::move(_future), std::forward<TFunction>(function));
                
                return future;
            }
            
        public:
            std::future<T> _future;
        };

        template <class TFunction, class... TArgs>
        Future<typename std::invoke_result_t<TFunction, TArgs...>>
        MakeTask(TFunction&& function, TArgs&& ...args)
        {
            using Result = typename std::invoke_result_t<TFunction, TArgs...>;
            Future<Result> future;
            
            future._future = std::async(std::launch::async | std::launch::deferred, [](TFunction&& function, TArgs&& ...args)
            {
                return std::invoke(std::forward<TFunction>(function), std::forward<TArgs>(args)...);
            },
            std::forward<TFunction>(function),
            std::forward<TArgs>(args)...);
            
            return future;
        };
    }

    namespace second_implementation
    {
        /*
         Конвертирование lambda to function: https://gist.github.com/khvorov/cd626ea3685fd5e8bf14
                                             https://cplusplus.com/forum/general/223816/
         */
        namespace detail
        {
            template <typename F>
            struct function_traits : public function_traits<decltype(&F::operator())> {};

            template <typename R, typename C, typename... Args>
            struct function_traits<R(C::*)(Args...) const>
            {
                using function_type = std::function<R(Args...)>;
            };
        }

        template <typename F>
        using function_type_t = typename detail::function_traits<F>::function_type;

        template <typename TLambda>
        function_type_t<TLambda> lambda_to_function(TLambda& lambda) // Приводим lambda к std::function
        {
            return static_cast<function_type_t<TLambda>>(lambda);
        }

        template <class T>
        class Future
        {
        public:
            
            T Get()
            {
                return _future.get();
            }

            template <class TFunction>
            auto Then(TFunction&& function)
            {
                return Then(std::launch::async | std::launch::deferred, lambda_to_function(function));
            }
            
            template <class TFunction>
            auto Then(TFunction&& function, std::launch policy)
            {
                return Then(policy, lambda_to_function(function));
            }
            
        public:
            template <class R>
            auto Then(std::launch policy, const std::function<R(const T&)>& function)
            {
                Future<R> future;
                
                future._future = std::async(policy, [](std::future<T>&& future, auto&& function)
                {
                    future.wait();
                    return std::invoke(function, future.get());
                },
                std::move(_future), function);
                
                return future;
            }
            
        public:
            std::future<T> _future;
        };

        template <class TFunction, class... TArgs>
        Future<typename std::invoke_result_t<std::decay_t<TFunction>, std::decay_t<TArgs>...>>
        MakeTask(TFunction&& function, TArgs&& ...args)
        {
            using Result = typename std::invoke_result_t<std::decay_t<TFunction>, std::decay_t<TArgs>...>;
            Future<Result> future;
            
            future._future = std::async(std::launch::async | std::launch::deferred, [](TFunction&& function, TArgs&& ...args)
            {
                return std::invoke(std::forward<TFunction>(function), std::forward<TArgs>(args)...);
            },
            std::forward<TFunction>(function),
            std::forward<TArgs>(args)...);
            
            return future;
        };
    }

    int function(const int& number)
    {
        return number * 100;
    }
}



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
        
        /*
         Реализация future.then, где then осуществляет запуск цепочки выполнения в будущем последовательных асинхронных операций для вычисления промежуточных результатов.
         */
        {
            using namespace then;
            std::cout << "then" << std::endl;
            
            /// first implementation
            {
                // Вычисление (2v + 1)^ 2 * 100
                
                using namespace first_implementation;
                std::cout << "first implementation" << std::endl;
                
                auto future = MakeTask([](int number)
                {
                    std::cout << "Поток: " << std::this_thread::get_id() << std::endl;
                    return 2 * number;
                }, 1).
                Then([](const int number)
                {
                    std::cout << "Поток: " << std::this_thread::get_id() << std::endl;
                    return number + 1;
                }).
                Then([](const int& number)
                {
                    std::cout << "Поток: " << std::this_thread::get_id() << std::endl;
                    return number * number;
                }).
                Then(function);
                
                [[maybe_unused]] auto result = future.Get();
                std::cout << std::endl;
            }
            /// second implementation
            {
                // Вычисление (2v + 1)^ 2
                
                using namespace second_implementation;
                std::cout << "second implementation" << std::endl;
                
                auto future = MakeTask([](const int& number)
                {
                    std::cout << "Поток: " << std::this_thread::get_id() << std::endl;
                    return 2 * number;
                }, 1).
                Then([](const int& number)
                {
                    std::cout << "Поток: " << std::this_thread::get_id() << std::endl;
                    return number + 1;
                }).
                Then([](const int& number)
                {
                    std::cout << "Поток: " << std::this_thread::get_id() << std::endl;
                    return number * number;
                })/*.
                Then(function)*/; // Для обычных функций не работает
                
                [[maybe_unused]] auto result = future.Get();
                std::cout << std::endl;
            }
        }
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
             Механизмы асинхронной работы потоков:
             
             std::future - низкоуровневый интерфейс чтения результата, который вычислится в будущем. Работает по pull-модели, идет опрос, готовы ли данные. Позволяет не использовать std::mutex, std::condition_variable.
             Методы:
             - wait - блокирует текущий поток, пока promise не запишет значение. Если std::future не связан с std::promise, то поведение неопределено.
             - get - возвращает результат вычисления из другого потока, если результат не готов, то блокирует выполнение потока до появления, внутри вызывая wait. Если нужно получить один и тот же результат в разных потоках, то нужно использовать std::shared_future, иначе terminate. Может вывести исключение сохраненное в std::promise с помощью метода set_exception.
             - valid - проверяет готов ли результат вычисления, если готов, то можно вызвать метод get(). Если std::future не связан с std::promise, то поведение неопределено.
             - wait_for - блокирует текущий поток на ОПРЕДЕЛЕННОЕ ВРЕМЯ или пока promise не запишет значение. Возвращает future_status(ready - результат готов, timeout - время истекло, deferred - время не истекло).
             - wait_until - блокирует текущий поток до НАСТУПЛЕНИЕ МОМЕНТА ВРЕМЕНИ (например, 11:15:00) или пока promise не запишет значение. Возвращает future_status(ready - результат готов, timeout - время истекло, deferred - время не истекло).
             - share - создает shared_future, который помогает получить один и тот же результат в разных потоках. Может вывести исключение сохраненное в std::promise с помощью метода set_exception.
             */
            {
                /*
                 1 Способ: std::promise - низкоуровневый интерфейс для записи результата, который вычисляется в отедельном потоке. std::promise служит каналом связи между текущим и основным потоками. Каждый объект promise связан с объектом future - интерфейс чтения результата. Он более удобный, чем std::mutex + std::condition_variable.
                 Методы:
                 - set_value - сохраняет значение, которое можно запросить с помощью связанного объекта std::future.
                 - get_future - получение другого канал передачи результата - std::future.
                 - set_exception - можно сохранить указатель на исключение (exception_ptr) и вывести исключение с помощью std::future метода get.
                 - set_value_at_thread_exit и set_exception_at_thread_exit() - сохраняют значение или исключение после завершения потока аналогично тому, как работает std::notify_all_at_thread_exit.
                 */
                {
                    std::cout << "1 Способ: std::promise" << std::endl;
                    
                    // 1 Способ: метод valid
                    {
                        std::promise<void> promise;
                        std::future<void> future = promise.get_future();
                     
                        std::cout << std::boolalpha;
                     
                        std::cout << future.valid() << std::endl; // true
                        promise.set_value(); // установили результат, future - valid
                        std::cout << future.valid() << std::endl; // true
                        future.get(); // отдали результат, future - invalid, потому что уже не связан с promise
                        std::cout << future.valid() << std::endl; // false
                    }
                    // 2 Способ: обычный
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
                    // 3 Способ: чтение future из разных потоков
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
                /* 2 Способ: std::packaged_task - оборачивает функцию и записывает результат вычисления функции сам вместо std::promise. Работает как std::function + std::promise и более удобный, чем std::promise.
                 Методы:
                 - get_future - получение другого канал передачи результата - std::future.
                 - make_ready_at_thread_exit - позволяет дождаться полного завершения потока перед тем, как привести std::future в состояние готовности.
                 - reset - очищает результаты предыдущего запуска задачи.
                 */
                {
                    std::cout << "2 Способ: std::packaged_task" << std::endl;
                    
                    // 1 Способ: обычный
                    {
                        std::cout << "1 Способ: обычный" << std::endl;
                        
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
                        std::cout << std::endl;
                    }
                    // 2 Способ: make_ready_at_thread_exit - позволяет дождаться полного завершения потока перед тем, как привести std::future в состояние готовности
                    {
                        std::cout << "2 Способ: make_ready_at_thread_exit - позволяет дождаться полного завершения потока перед тем, как привести std::future в состояние готовности" << std::endl;
                        
                        auto Function = [](std::future<void>& future)->void
                        {
                            std::packaged_task<void(bool&)> task{[](bool& done) { done = true; }};
                            auto result = task.get_future();
                         
                            bool done = false;
                            task.make_ready_at_thread_exit(done); // Начать выполнение
                         
                            std::cout << "task выполнился: " << std::boolalpha << done << std::endl;
                         
                            auto status = result.wait_for(std::chrono::seconds(0));
                            if (status == std::future_status::timeout)
                                std::cout << "Результат еще не готов" << std::endl;
                         
                            future = std::move(result);
                        };
                        
                        std::future<void> future;
                         
                        std::thread(Function, std::ref(future)).join();
                     
                        auto status = future.wait_for(std::chrono::seconds(0));
                        if (status == std::future_status::ready)
                            std::cout << "Результат готов" << std::endl;
                    }
                }
                /*
                 3 Способ: std::async - высокоуровневый интерфейс, внутри которого находится std::promise и Threadpool для создания потоков. Аналог std::packaged_task, возвращает std::future, куда передается результат вычислений. 
                 Если результат объекта std::future работы std::async не сохранять, тогда деструктор std::future заблокируется до тех пор, пока асинхронная операция не будет завершена, т.е. std::async будет выполняться не асинхронно, а просто последовательно в однопоточном режиме. Это легко упустить из виду, когда не требуется возвращаемое значение.
                 При сохранении результата объекта std::future, вызов деструктора объекта std::future будет отложен, он будет разрушен при выходе из стека. Внутри деструктора std::future вызывается std::get и дожидается окончания асинхронного результата. Деструкторы объектов std::future, полученных не из std::async, не блокируют поток.
                 Стратегии запуска:
                 - std::launch::async - другой поток.
                 - std::launch::deferred - текущий поток.
                 - по умолчанию std::async выберет стратегию в зависимости от загруженности потоков, но лучше на это не полагаться.
                 */
                {
                    std::cout << "3 Способ: std::async + std::launch::async" << std::endl;
                    std::cout << "Стратегии запуска:" << std::endl;
                    std::cout << "- std::launch::async - другой поток" << std::endl;
                    std::cout << "- std::launch::deferred - текущий поток" << std::endl;
                    
                    std::future<int> future1 = std::async(std::launch::async, Multi1, 10, 2);
                    std::future<int> future2 = std::async(std::launch::deferred, Multi1, 11, 2);
                    std::future<int> future3 = std::async(std::launch::async | std::launch::deferred, Multi1, 11, 2);
                    std::future<int> future4 = std::async(Multi1, 12, 2);
                    std::async(Multi1, 13, 2); // будет выполняться последовательно в однопоточном режиме, т.к. std::future не сохранен
                    
                    std::cout << "future1 в другом потоке: " << future1.get() << std::endl;
                    std::cout << "future2 в текущем потоке: " << future2.get() << std::endl;
                    std::cout << "future3 по-умолчанию: " << future3.get() << std::endl;
                    std::cout << "future4 по-умолчанию: " << future4.get() << std::endl;
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
