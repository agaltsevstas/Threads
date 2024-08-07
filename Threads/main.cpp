#include "Atomic.hpp"
#include "Condition_Variable.hpp"
#include "Coroutine.hpp"
#include "Promise_Future.hpp"
#include "Latch_Barrier.hpp"
#include "Lock.hpp"
#include "Deadlock.hpp"
#include "Mutex.hpp"
#include "Singleton.h"
#include "shared_recursive_mutex.h"
#include "Semaphore.hpp"
#include "Timer.h"
#include "Queue.h"

#include <iomanip>
#include <iostream>
#include <future>
#include <numeric>
#include <ranges>
#include <stack>
#include <vector>


#if defined(_MSC_VER) || defined(_MSC_FULL_VER) || defined(_WIN32) || defined(_WIN64)
    #include "OpenMP.h"
    #include "TBB.h"

    #include <syncstream>
#endif

 
/*
 Лекция: https://www.youtube.com/watch?v=z6M5YCWm4Go&ab_channel=ComputerScience%D0%BA%D0%BB%D1%83%D0%B1%D0%BF%D1%80%D0%B8%D0%9D%D0%93%D0%A3
 
 Сайт: https://habr.com/ru/companies/otus/articles/549814/
       https://habr.com/ru/articles/182626/
       http://scrutator.me/post/2012/04/04/parallel-world-p1.aspx
 
 Atomic: https://vk.com/@habr-kak-rabotat-s-atomarnymi-tipami-dannyh-v-c?ysclid=lyfjpgv4gn827137332
         https://habr.com/ru/articles/328362/
         https://habr.com/ru/articles/195948/
         https://stackoverflow.com/questions/50298358/where-is-the-lock-for-a-stdatomic
 
 Mutex: https://rsdn.org/forum/cpp/7147751.all

 Семафоры: https://www.geeksforgeeks.org/cpp-20-semaphore-header/
           https://www.geeksforgeeks.org/std-barrier-in-cpp-20/
*/

class MyClass
{
public:
    int Multi1(int number, int factor)
    {
        return number * factor;
    }

    void Multi2(int number, int factor, int& result)
    {
        result = number * factor;
    }
};

void SleepFor(int sleep)
{
    for (int i = 0; i < 10; ++i)
    {
        std::cout << "Текущий поток: " << std::this_thread::get_id() << ", i = " << i << std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(sleep));
    }
}

void SleepUntil()
{
    using std::chrono::system_clock;
    std::time_t curTime = system_clock::to_time_t(system_clock::now());

    const auto currentTime = *std::localtime(&curTime);
    auto untilTime = *std::localtime(&curTime);

    while (std::modulus<int>()(untilTime.tm_sec, 5))
        ++untilTime.tm_sec;
    std::this_thread::sleep_until(system_clock::from_time_t(mktime(&untilTime)));

    std::cout << "Ждать пока секунды не будут кратны 5" << std::endl;
    std::cout << "Предыдущее время: " << std::put_time(&currentTime, "%X") << std::endl;
    std::cout << "Текущее время: " << std::put_time(&untilTime, "%X") << ", " << std::put_time(&untilTime, "%S") << " секунд кратно 5" << std::endl;
}

int Multi1(int number, int factor)
{
    return number * factor;
}

void Multi2(int number, int factor, int& result)
{
    result = number * factor;
}


int main()
{
    setlocale(LC_ALL, "Russian");
    
    Timer timer;
    constexpr int size = 100;
    std::vector<int> numbers(size);
    std::iota(numbers.begin(), numbers.end(), 1);

    std::cout << "Кол-во возможных потоков на этом компьютере: " << std::thread::hardware_concurrency();

    // Без многопоточности
    {
        std::cout << "Без многопоточности" << std::endl;
        int sum = 0;
        timer.start();
        SleepFor(10);
        timer.stop();
        std::cout << "Сумма: " << sum << " Время: " << timer.elapsedMilliseconds() << " мс" << std::endl;
        std::cout << std::endl;
    }
    /*
     Многопоточность
     */
    {
        std::cout << "Многопоточность" << std::endl;
        
        // Вызов объекта ровно один раз, даже если он вызывается одновременно из нескольких потоков.
        {
            std::once_flag flag;
            auto CallOnce = [&flag](int number, int factor)
            {
                std::call_once(flag, [&](){ std::cout << "result in std::call_once: " << number * factor << std::endl;});
            };
            
            std::thread thread1(CallOnce, 10, 2);
            std::thread thread2(CallOnce, 11, 2);
            std::thread thread3(CallOnce, 12, 2);
            
            thread1.join();
            thread2.join();
            thread3.join();
        }

        // Синхронизация
        {
            std::cout << "Синхронизация" << std::endl;
            /*
             std::thread - это RAII обертка вокруг операционной системы, POSIX - API Linuxa, WindowsRest - API Windows
             Методы:
             - detach - разрывает связь между объектом thread и потоком, который начал выполняться. detach - не блокирует основной поток, не заставляя дожидаться окончания выполнения потока.
             - join - блокирует основной поток, заставляя дожидаться окончания выполнения потока.
             - joinable - проверяет ассоциирован std::thread с потоком, если нет (не было detach или join или std::move) - возвращает true.
             Замечание:
             - нельзя копировать
             - можно перемещать
             */
            {
                std::cout << "std::thread" << std::endl;
                // 1 Способ: Error: terminating, попытается вызваться деструктор и программа попытается закрыться из другого потока
                {
                    /*
                     int sum;
                     std::thread thread(Work, 100); // Error: terminating
                     Work(10);
                     // thread.~thread() // Error: terminating, попытается вызваться деструктор и программа попытается закрыться из другого потока
                     */
                }
                // 2 Способ: detach - разрывает связь между объектом thread и потоком, который начал выполняться. detach - не блокирует основной поток, не заставляя дожидаться окончания выполнения потока.
                {
                    std::cout << "2 Способ: detach" << std::endl;
                    std::thread thread(SleepFor, 100); // Error: terminating
                    if (thread.joinable()) // Проверяет ассоциирован std::thread с потоком, если нет (не было detach или join) - возвращает true
                        thread.detach();
                    SleepFor(10);
                    std::this_thread::sleep_for(std::chrono::seconds(1)); // подождать завершение всех потоков
                    std::cout << std::endl;
                }
                // 3 Способ: join - блокирует основной поток, заставляя дожидаться окончания выполнения потока.
                {
                    // join - выполнение 2 потоков последовательно
                    {
                        std::cout << "3 Способ: join - выполнение 2 потоков последовательно" << std::endl;
                        std::thread thread1(SleepFor, 100);
                        std::thread thread2 = std::move(thread1);
                        if (thread1.joinable()) // Проверяет ассоциирован std::thread с потоком, если нет (не было detach или join) - возвращает true
                            thread1.join();
                        if (thread2.joinable()) // Проверяет ассоциирован std::thread с потоком, если нет (не было detach или join) - возвращает true
                            thread2.join();
                        SleepFor(10);
                        std::cout << std::endl;
                    }
                    // join - выполнение 2 потоков параллельно
                    {
                        std::cout << "3 Способ: join - выполнение 2 потоков параллельно" << std::endl;
                        std::thread thread(SleepUntil);
                        SleepFor(10);
                        thread.join();
                        std::cout << std::endl;
                    }
                }
                // Получение результата
                {
                    // Получить результат не получится
                    {
                        std::thread thread(Multi1, 10, 2);
                        thread.join();
                    }
                    // Получение результата через Lambda
                    {
                        int result = 0;
                        std::thread thread([&result]() { result = Multi1(10, 2); });
                        thread.join();
                        std::cout << "Получение результата из Multi2 через Lambda: " << result << std::endl;
                        std::cout << result << std::endl;
                    }
                    // Получение результата по ссылке
                    {
                        int result;
                        std::thread thread(Multi2, 10, 2, std::ref(result));
                        thread.join();
                        std::cout << "Получение результата из Multi2 по ссылке: " << result << std::endl;
                        std::cout << std::endl;
                    }
                    // Работа с классами
                    {
                        MyClass myclass;
                        // Получение результата через Lambda
                        {
                            int result = 0;
                            std::thread thread([&]() { result = myclass.Multi1(10, 2); });
                            thread.join();
                            std::cout << "Получение результата из Multi2 через Lambda: " << result << std::endl;
                            std::cout << result << std::endl;
                        }
                        // Получение результата по ссылке
                        {
                            int result;
                            std::thread thread(&MyClass::Multi2, myclass, 10, 2, std::ref(result));
                            thread.join();
                            std::cout << "Получение результата из Multi2 по ссылке: " << result << std::endl;
                            std::cout << std::endl;
                        }
                    }
                }
            }
            /* C++20
             std::jthread - по сравнению с std::thread преимущества:
             - в деструкторе вызывается join.
             - безопасный и предотвращает утечку ресурсов: исключение будет перехвачено и обработано.
             */
            {
                std::cout << "jthread" << std::endl;
                // std::jthread thread(SleepFor, 100); XCode не поддерживает
                SleepFor(10);
                std::cout << std::endl;
            }
            /*
             Исключения (std::exception) в разных потоках (std::thread) - не пересекаются. Чтобы пробросить исключение в главный поток можно использовать:
             - std::exception_ptr - обертка для исключения (std::exception), из нее ничего нельзя получить, только пробросить дальше с помощью std::rethrow_exception.
             - std::rethrow_exception() - пробрасывает исключение в другой try/catch.
             Сохранение исключений:
                1 Способ: stack<exception_ptr> - сохраняет указатель на исключение (exception_ptr) из другого потока.
                2 Способ: promise - сохраняет указатель на исключение (exception_ptr) из другого потока с помощью метода set_exception и выводит исключение с помощью std::future метода get.
             - std::uncaught_exceptions() - возвращает кол-во неперехваченных исключений в текущем потоке, помогает избежать double exception в деструкторе, написав проверку if (!std::uncaught_exceptions()) throw.
             Решение: в деструкторе должна быть гаранти
             */
            {
                // Перехват исключений (exception catching)
                {
                    struct DoubleException
                    {
                        ~DoubleException() noexcept(false)
                        {
                            if (int count = std::uncaught_exceptions(); count == 0)
                                throw std::runtime_error("Dobule exception");
                        }
                    };
                    
                    std::cout << "Перехват исключений (exception catching)" << std::endl;
                    
                    // 1 Способ: std::stack<std::exception_ptr>
                    {
                        std::cout << "1 Способ: std::stack<std::exception_ptr>" << std::endl;
                        std::stack<std::exception_ptr> exception_queue;
                        auto Function = [&]()
                        {
                            try
                            {
                                DoubleException doubleException;
                                throw std::runtime_error("Test exception");
                            }
                            catch (...)
                            {
                                std::exception_ptr exception = std::current_exception();
                                exception_queue.push(exception);
                            }
                        };
                        
                        std::thread thread(Function);
                        thread.join();
                        
                        while (!exception_queue.empty())
                        {
                            try
                            {
                                auto exception = exception_queue.top();
                                exception_queue.pop();
                                std::rethrow_exception(exception);
                            }
                            catch (const std::exception& exception)
                            {
                                std::cout << "Exception: " << exception.what() << std::endl;
                            }
                        }
                    }
                    // 2 Способ: std::promise + std::future
                    {
                        std::cout << "2 Способ: std::promise + std::future" << std::endl;
                        std::promise<int> promise;
                        auto future = promise.get_future();
                        
                        auto Function = [&]()
                        {
                            try
                            {
                                DoubleException doubleException;
                                throw std::runtime_error("Test exception");
                            }
                            catch (...)
                            {
                                std::exception_ptr exception = std::current_exception();
                                promise.set_exception(exception);
                            }
                        };
                        
                        std::thread thread(Function);
                        thread.join();
                        
                        try
                        {
                            future.get();
                        }
                        catch (const std::exception& exception)
                        {
                            std::cout << "Exception: " << exception.what() << std::endl;
                        }
                    }
                    
                    std::cout << std::endl;
                }
                // Безопасность исключений (exception safety)
                {
                    std::cout << "Перехват исключений (exception catching)" << std::endl;
                    // Неправильный захват std::mutex при exception: использование lock/unlock в std::mutex
                    {
                        std::cout << "Неправильный захват std::mutex при exception: использование lock/unlock в std::mutex" << std::endl;
                        std::mutex mutex;
                        auto Function = [&mutex]()
                        {
                            try
                            {
                                mutex.lock();
                                throw std::runtime_error("Test exception");
                                mutex.unlock();
                            }
                            catch (...)
                            {
                                mutex.unlock(); // приходится здесь писать unlock
                            }
                        };
                        
                        std::thread thread(Function);
                        thread.join();
                    }
                    /*
                     Правильный захват std::mutex при exception: вместо использования lock/unlock в std::mutex, используются RAII оберкти std::lock_guard/std::unique_lock, при выходе из стека в деструкторе вызывается unlock
                     */
                    {
                        std::cout << "Правильный захват std::mutex при exception: вместо использования lock/unlock в std::mutex, используются RAII оберкти std::lock_guard/std::unique_lock, при выходе из стека в деструкторе вызывается unlock" << std::endl;
                        std::mutex mutex;
                        auto Function = [&mutex]()
                        {
                            std::unique_lock lock(mutex);
                            try
                            {
                                throw std::runtime_error("Test exception");
                            }
                            catch (...)
                            {
                                auto exception = std::current_exception();
                            }
                        };
                        
                        std::thread thread(Function);
                        thread.join();
                    }
                    
                    std::cout << std::endl;
                }
            }
            // Синхронизация потоков или потокобезопасность (thread safety)
            {
                std::cout << "Синхронизация потоков" << std::endl;
                /*
                 1 Способ: std::mutex — механизм синхронизации, который предназначен для контроля доступа к общим данным для нескольких потоков. Под капотом mutex работает через системный вызов (syscall) futex (fast userspace mutex), который работает в ядре. Он усыпляет поток с помощью планировщика задач (Schedule) и добавляет поток в очередь ожидающих потоков, если мьютекс занят. Когда mutex освобождается, futex выберет один из ждущих в очереди потоков и помечает его исполнить планировщику задач (Schedule). Планировщик переключает контекст на этот поток, и он окажется захваченным mutex. Поход в ядро через syscall и переключение контекста на другой поток — достаточно долгие операции.
                 Методы, внутри используются синхронизации уровня операционной системы (ОС):
                 - lock - происходит захват mutex текущим потоком и блокирует доступ к данным другим потокам; или поток блокируется, если мьютекс уже захвачен другим потоком.
                 - unlock - освобождение mutex, разблокирует данные другим потокам.
                 Замечание: при повторном вызове lock - вылезет std::exception или приведет к состоянию бесконечного ожидания, при повторном вызове unlock - ничего не будет или вылезет std::exception.
                 - try_lock - происходит захват mutex текущим потоком, НО НЕ блокирует доступ к данным другим потокам, а возвращает значение: true - можно захватить mutex / false - нельзя; НО МОЖЕТ возвращать ложное значение, потому что в момент вызова try_lock mutex может быть уже lock/unlock. Использовать try_lock в редких случаях, когда поток мог что-то сделать полезное, пока ожидает unlock.
                 */
                {
                    MUTEX::Start();
                }
#if defined(_MSC_VER) || defined(_MSC_FULL_VER) || defined(_WIN32) || defined(_WIN64)
                // 2 Способ: C++20
                {
                    std::cout << "std::osyncstream" << std::endl;
                    auto printSymbol1 = [](char c)
                    {
                        for (int i = 0; i < 10; ++i)
                        {
                            std::osyncstream(std::cout) << c;
                        }
                        std::cout << std::endl;
                    };
                    
                    auto printSymbol2 = [](char c)
                    {
                        for (int i = 0; i < 10; ++i)
                        {
                            std::osyncstream(std::cout) << c;
                        }
                        std::cout << std::endl;
                    };
                    
                    std::thread thread1(printSymbol1, '+');
                    std::thread thread2(printSymbol2, '-');
                    thread1.join();
                    thread2.join();
                    std::cout << std::endl;
                }
#endif
            }
            /*
             Lock management - RAII обертки, захват mutex.lock() ресурса происходит на стеке в конструкторе и высвобождение unlock при выходе из стека в деструкторе.
             */
            {
                LOCK::Start();
            }
            /*
             Deadlock — ситуация, в которой есть 2 mutex и 2 thread и 2 функции. В function1 идет mutex1.lock(), mutex2.lock(). В функции function2 идет обратная очередность mutex2.lock(), mutex1.lock(). Получается thread1 захватывает mutex1, thread2 захватывает mutex2 и возникает взаимная блокировка.
             Решение:
             1. Захватывать (lock) несколько мьютексов всегда в одинаковом порядке
             2. Отпускать (unlock) захваченные (lock) mutex в порядке LIFO («последним пришёл — первым ушёл»)
             3. Можно использовать алгоритм предотвращения взаимоблокировок
             */
            {
                deadlock::start();
            }
            
            /*
             std::condition_variable (условная переменная) - механизм синхронизации между потоками, который работает ТОЛЬКО в паре mutex + std::unique_lock. Используется для блокировки одного или нескольких потоков с помощью wait/wait_for/wait_until куда помещается mutex с lock, внутри wait происходит unlock для текущего потока, но его же блокирует с помощью while цикла до тех пор, пока другой поток не изменит общую переменную (условие) и не уведомит об этом condition_variable с помощью notify_one/notify_any.
             */
            {
                cv::start();
            }
            /*
             std::atomic - атомарная операция. Операция называется атомарной, если операция выполнена целиком, либо не выполнена полностью, поэтому нет промежуточного состояние операции.
             */
            {
                ATOMIC::Start();
            }
            /*
             Семафор (semaphore) - механизм синхронизации работы потоков, который может управлять доступом к общему ресурсу. В основе семафора лежит счётчик, над которым можно производить две атомарные операции: увеличение и уменьшение кол-во потоков на единицу, при этом операция уменьшения для нулевого значения счётчика является блокирующей. Служит для более сложных механизмов синхронизации параллельно работающих задач. В качестве шаблонного параметра  указывается максимальное допустимое кол-во потоков. В конструкторе инициализируется счетчик - текущее допустимое кол-во потоков.
             */
            {
                semaphore::start();
            }
            /*
             Барьер - механизм синхронизации работы потоков, который может управлять доступом к общему ресурсу и позволяет блокировать любое количество потоков до тех пор, пока ожидаемое количество потоков не достигнет барьера.
             Защелки нельзя использовать повторно, барьеры можно использовать повторно.
             
             Виды:
             1. Защелка (std::latch) - механизм синхронизации работы потоков, который может управлять доступом к общему ресурсу. В основе лежит уменьшающийся счетчик, значение счетчика инициализируется при создании. Потоки уменьшают значение счётчика и блокируются на защёлке до тех пор, пока счетчик не уменьшится до нуля. Нет возможности увеличить или сбросить счетчик, что делает защелку одноразовым барьером.
             2. Барьер (std::barrier) - механизм синхронизации работы потоков, который может управлять доступом к общему ресурсу. В основе лежит уменьшающийся счетчик, значение счетчика инициализируется при создании. Барьер блокирует потоки до тех пор, пока все потоки не уменьшат значение счётчика до нуля, как только ожидающие потоки разблокируются, значение счётчика устанавливается в начальное состояние и барьер может быть использован повторно.
             
             Отличия std::latch от std::barrier:
             - std::latch может быть уменьшен одним потоком более одного раза.
             - std::latch - можно использовать один раз, std::barrier является многоразовым: как только ожидающие потоки разблокируются, значение счётчика устанавливается в начальное состояние и барьер может быть использован повторно.
             */
            {
                Latch_Barrier::Start();
            }
            /*
             Потокобезопасная очередь - позволяет безопасно нескольким потоков получать доступ к элементам очереди без необходимости синхронизации (синхронизация внутри структуры).
             */
            {
                std::cout << "ThreadSafeQueue" << std::endl;
                
                // mutex
                {
                    using namespace MUTEX;
                    std::cout << "mutex" << std::endl;
                    
                    ThreadSafeQueue<std::string> messages;
                    messages.Push("1");
                    messages.Push("2");
                    messages.Push("3");
                    
                    while (!messages.Empty())
                    {
                        auto result = messages.Pop();
                    }
                    
                    std::cout << std::endl;
                }
                // shared_mutex
                {
                    using namespace SHARED_MUTEX;
                    std::cout << "shared_mutex" << std::endl;
                    
                    ThreadSafeQueue<std::string> messages;
                    messages.Push("1");
                    messages.Push("2");
                    messages.Push("3");
                    
                    while (!messages.Empty())
                    {
                        auto result = messages.Front();
                        messages.Pop();
                    }
                    
                    std::cout << std::endl;
                }
            }
        }
        // Паралеллизм
        {
            // Threadpool
            {
                std::cout << "Threadpool" << std::endl;
                // 1 Способ: обычный
                {
                    int sum = 0;
                    std::vector<std::thread> threads;
                    threads.reserve(size);

                    timer.start();
                    for (const auto& number : numbers)
                    {
                        threads.emplace_back([&]()
                            {
                                sum += number;
                            });
                    }
                    for (auto& thread : threads)
                    {
                        thread.join();
                    }
                    timer.stop();
                    std::cout << "1 Способ: обычный, с указанием размера массива, Сумма: " << sum << " Время: " << timer.elapsedMilliseconds() << " мс" << std::endl;
                }
                // 2 Способ: std::this_thread::yield - приостановливает текущий поток, отдав преимущество другим потокам
                {
                    std::atomic_bool ready = false;
                    int sum = 0;
                    std::vector<std::thread> threads;
                    threads.reserve(size);

                    timer.start();
                    for (const auto& number : numbers)
                    {
                        threads.emplace_back([&]()
                            {
                                while (!ready)
                                    std::this_thread::yield(); // приостановливает текущий поток, отдав преимущество другим потокам

                                sum += number;
                            });
                    }
                    ready = true;
                    for (auto& thread : threads)
                    {
                        thread.join();
                    }
                    timer.stop();
                    std::cout << "2 Способ: std::this_thread::yield, с указанием размера массива, Сумма: " << sum << " Время: " << timer.elapsedMilliseconds() << " мс" << std::endl;
                }
            }
            
            /*
             Параллельные алгоритмы, C++17
             - использовать при n > 10000 при ОЧЕНЬ ПРОСТЫХ операциях. Чем сложнее операции, тем быстрее выполняется параллельность.
             - OpenMP все равно быстрее, поэтому лучше его использовать. Но TBB быстрее OpenMP.
             */
            {
                // std::execution::seq (обычная сортировка)
                {
                    std::cout << "std::execution::seq" << std::endl;
                    std::vector<int> numbers = { 3, 2, 4, 5, 1 };
                    // std::ranges::sort(std::execution::seq, numbers); // в xcode не работает
                }
                // std::execution::par (параллельная сортировка)
                {
                    std::cout << "std::execution::par" << std::endl;
                    std::vector<int> numbers = { 3, 2, 4, 5, 1 };
                    // std::ranges::sort(std::execution::par, numbers); // в xcode не работает
                }
                // std::execution::unseq (parallel + vectorized (SIMD))
                {
                    std::cout << "std::execution::unseq" << std::endl;
                    // TODO
                }
                // std::execution::par_unseq (vectorized, C++20)
                {
                    std::cout << "std::execution::unseq" << std::endl;
                    // TODO
                }
            }
#if defined(_MSC_VER) || defined(_MSC_FULL_VER) || defined(_WIN32) || defined(_WIN64)
            /*
              OpenMP(Open Multi-Processing) — это библиотека, используемая для многопоточности на уровне цикла.
              Использование параллельной версии STL (все алгоритмы внутри поддерживают OpenMP и будут выполняться параллельно), для этого нужно передать libstdc++ parallel в компилятор GCC.
              До цикла for будет определено кол-во ядер в системе. Код внутри for будет выделен в отдельную функцию, которая запустится в отдельном потоке (НЕ сам For!!! try catch бессмысленен - нельзя отловить исключение в отдельном потоке, break, continue, return - невозможны).
              В конце области видимости для каждого потока будет вызван join().
              Например для 10 ядерной системы будет запущено 10 потоков. При 10.000 итераций. Каждый поток обработает 1.000 / 10 = 1.000 элементов в контейнере.
              Не подходит:
              - для рекурсии (может кончиться стек). Есть исключение с использованием очереди задач #pragma omp task. При условии, что размер очереди < 255.
              Можно на определенном уровне стека запоминать состояния (значения переменных) и кидать в очередь.
              После окончания функции в рамках того же потока или другого продолжаем вызывать ту же функцию с такой же логикой. Таким развязываем рекурсию по потокам через очередь.
              - не всегда обеспечивает хорошую производительность
              Подробнее: https://learn.microsoft.com/ru-ru/cpp/parallel/openmp/2-directives?view=msvc-170
            */
            {
                openmp::start();
            }

            /*
              Использовать C++17
              TBB (Threading Building Blocks) - библиотека Intel, является высокоуровневой библиотекой, чем OpenMP.
              В TBB есть планировщих задач, который помогает лучше оптимизировать работу.
              Это достигается с помощью алгоритма work stealing, который реализует динамическую балансировку нагрузки. Есть функция разной сложности, какой-то поток очень быстро обработал свою очередь задач, то он возьмет часть свободных задач другого потока. В TBB самому создать поток нельзя, поэтому в каких потоках идет выполнение знать не нужно. Можно только создать задачу и отдать ее на исполнение планировщику.
              Подробнее: https://oneapi-src.github.io/oneTBB/
            */
            {
                tbb::start();
            }
#endif
            std::cout << std::endl;
        }
        // Асинхронность
        {
            std::cout << "Асинхронность" << std::endl;
            
            {
                Promise_Future::Start();
            }
            
            /*
             Корутина (coroutine) - механизм асинхронной работы потоков, функция с несколькими точками входа и выхода, из нее можно выйти середине, а затем вернуться в нее и продолжить исполнение. По сути это объект, который может останавливаться и возобновляться. C++20: stackless, userserver (yandex): stackfull.
              Gример — программы, выполняющие много операций ввода-вывода. Типичный пример — веб-сервер. Он вынужден общаться со многими клиентами одновременно, но при этом больше всего он занимается одним — ожиданием. Пока данные будут переданы по сети или получены, он ждёт. Если мы реализуем веб-сервер традиционным образом, то в нём на каждого клиента будет отдельный поток. В нагруженных серверах это будет означать тысячи потоков. Ладно бы все эти потоки занимались делом, но они по большей части приостанавливаются и ждут, нагружая операционную систему по самые помидоры переключением контекстов.
             Характериситки:
             - stackfull - держат свой стек в памяти на протяжении всего времени жизни корутины.
             - stackless - не сохраняет свой стек в памяти на протяжении всего времени жизни корутины, а только во время непосредственной работы. При этом стек аллоцируется в вызывающем корутину контексте.
             Методы:
             - co_await — для прерывания функции и последующего продолжения.
             - co_yield — для прерывания функции с одновременным возвратом результата. Это синтаксический сахар для конструкции с co_await.
             - co_return — для завершения работы функции.
             */
            {
                coroutine::start();
            }
        }
        // Singleton
        {
            std::cout << "Singleton" << std::endl;
            // До C++11
            {
                std::cout << "Непотокобезопасный Singleton до C++11" << std::endl;
                auto CreateSingleton = [&]()
                    {
                        antipattern::Singleton::Instance();
                    };
                
                std::thread thread1(CreateSingleton);
                std::thread thread2(CreateSingleton);
                
                [[maybe_unused]] auto instance = antipattern::Singleton::Instance();
                thread1.join();
                thread2.join();
            }
            // C++11
            {
                std::cout << "Потокобезопасный Singleton Майерса с C++11" << std::endl;
                auto CreateSingleton = [&]()
                    {
                        pattern::Singleton::Instance();
                    };
                
                std::thread thread1(CreateSingleton);
                std::thread thread2(CreateSingleton);
                
                [[maybe_unused]] auto instance = antipattern::Singleton::Instance();
                thread1.join();
                thread2.join();
            }
        }
    }
    std::cout << std::endl;

    return 0;
}
