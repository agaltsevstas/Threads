#include "Lock.hpp"
#include "Deadlock.hpp"
#include "Mutex.hpp"

#include "shared_recursive_mutex.h"
#include "Timer.h"

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

constinit int sum_static = 0;
int Sum(int number)
{
    sum_static += number;
    return sum_static;
}

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
    Timer timer;
    constexpr int size = 100;
    std::vector<int> numbers(size);
    std::iota(numbers.begin(), numbers.end(), 1);
    setlocale(LC_ALL, "Russian");

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

        // Синхронизация
        {
            std::cout << "Синхронизация" << std::endl;
            /*
             std::thread - это RAII обертка вокруг операционной системы, POSIX - API Linuxa, WindowsRest - API Windows
             Методы:
             - detach - разрывает связь между объектом thread и потоком, который начал выполняться. detach - не блокирует основной поток, не заставляя дожидаться окончания выполнения потока.
             - join - блокирует основной поток, заставляя дожидаться окончания выполнения потока.
             - joinble - проверяет ассоциирован std::thread с потоком, если нет (не было detach или join) - возвращает true.
             Замечание:
             - нельзя копировать
             */
            {
                std::cout << "std::thread" << std::endl;
                // 1 Способ: Error: terminating, попытается вызваться деструктор и программа попытается закрыться из другого потока
                {
                    /*
                     int sum;
                     std::thread th(Work, 100); // Error: terminating
                     Work(10);
                     // th.~thread() // Error: terminating, попытается вызваться деструктор и программа попытается закрыться из другого потока
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
                        std::thread thread(SleepFor, 100);
                        if (thread.joinable()) // Проверяет ассоциирован std::thread с потоком, если нет (не было detach или join) - возвращает true
                            thread.join();
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
                    // Получение результата с помощью sts::promise
                    {
                        // TODO
                    }
                    // Получение результата через глобальную переменную std::atomic
                    {
                        // TODO
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
             - std::rethrow_exception - пробрасывает исключение в другой try/catch.
             - std::stack<std::exception_ptr> - сохраняет исключение из другого потока.
             */
            {
                // Перехват исключений (exception catching)
                {
                    std::cout << "Перехват исключений (exception catching)" << std::endl;
                    std::stack<std::exception_ptr> exception_queue;
                    auto Function = [&exception_queue]()
                    {
                        try
                        {
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
                    // Правильный захват std::mutex при exception: вместо использования lock/unlock в std::mutex, используются RAII оберкти std::lock_guard/std::unique_lock, при выходе из стека в деструкторе вызывается unlock
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
                // 1 Способ: mutex
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
                                    std::this_thread::yield();

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
              OpenMP(Open Multi - Processing) — это библиотека, используемая для многопоточности на уровне цикла.
              Использование параллельной версии STL (все алгоритмы внутри поддерживают OpenMP и будут выполняться параллельно), для этого нужно передать libstdc++ parallel в компилятор GCC.
              До цикла for будет определено кол-во ядер в системе. Код внутри for будет выделен в отдельную функцию (НЕ сам For!!! try catch бессмысленен - нельзя отловить исключение в отдельном потоке, break, continue, return - невозможны).
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
            std::vector<std::future<int>> futures;
            futures.reserve(size);
            int sum = 0;

            // 1 Способ
            {
                auto PrintSymbol = [](char c)
                    {
                        for (int i = 0; i < 10; ++i)
                            std::cout << c;
                        std::cout << std::endl;
                    };

                std::thread th1(PrintSymbol, '+');
                std::thread th2(PrintSymbol, '-');

                th1.join();
                th2.join();
            }

            // 2 способ: std::future
            {
                std::cout << "std::future" << std::endl;
                timer.start();
                for (int i = 0; i < size; ++i)
                    futures.emplace_back(std::async(std::launch::async, Sum, i));

                for (auto& future : futures)
                    sum += future.get();

                timer.stop();
                std::cout << "1 cпособ Future, Сумма: " << sum << " Время: " << timer.elapsedMilliseconds() << " мс" << std::endl;
            }

            // 3 способ: std::future + std::lambda
            {
                std::cout << "std::future + std::lambda" << std::endl;
                sum = 0;
                futures.clear();

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
                std::cout << "Способ Future, Сумма: " << sum << " Время: " << timer.elapsedMilliseconds() << " мс" << std::endl;
            }
            std::cout << std::endl;
        }
    }

    return 0;
}
