#include "Condition_Variable.hpp"

#include <condition_variable>
#include <iostream>
#include <mutex>
#include <thread>
#include <ranges>
#include <shared_mutex>
#include <vector>

namespace cv
{
    class Spinlock
    {
        Spinlock(const Spinlock&) = delete;
        
    public:
        Spinlock() = default;
        ~Spinlock() = default;
        
        void Lock() noexcept
        {
            std::unique_lock lock(_mutex);
            _cv.wait(lock, [this] { return !_flag.exchange(true); });
        }
        
        bool Try_lock() noexcept
        {
            return !_flag.exchange(true);
        }
        
        void Unlock() noexcept
        {
            _cv.notify_one();
            _flag = false;
        }
        
    private:
        std::atomic<bool> _flag;
        std::mutex _mutex;
        std::condition_variable _cv;
    };
}

template <class TSpinlock>
void PrintSymbol(char c, TSpinlock& spinlock)
{
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    spinlock.Lock();
    for (int i = 0; i < 10; ++i)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        std::cout << c;
    }
    spinlock.Unlock();
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
}

/*
 std::condition_variable (условная переменная) - механизм синхронизации между потоками, который работает ТОЛЬКО в паре mutex + std::unique_lock. Используется для блокировки одного или нескольких потоков с помощью wait/wait_for/wait_until куда помещается mutex lock до тех пор, пока другой поток не изменит общую переменную (условие) и не уведомит об этом condition_variable с помощью notify_one/notify_any.

 spurious wakeup (ложные пробуждения) - пробуждение потока без веской причины. Это происходит потому что между моментом сигнала от condition_variable и моментом запуска ожидающего потока, другой поток запустился и изменил условие, вызвав состояние гонки. Если поток просыпается вторым, он проиграет гонку и произойдет ложное пробуждение. После wait рекомендуют всегда проверять истинность условие пробуждения в цикле или в предикате.
 
 Методы:
 - notify_one - снятие блокировки, ожидание 1 потока. Например, 1 писатель или много писателей.
 - notify_all - ожидание многих потоков. Например, много читаталей.
 - wait(mutex)/wait(mutex, predicate) - ставит поток в ожидание сигнала: атомарно освобождает (unlock) mutex для другого потока (чтобы тот смог выполнить условие) и приостанавливает выполнение текущего потока до тех пор, пока не будет уведомлена condition_variable или не произойдет ложное пробуждение, затем текущий поток пробуждается и mutex блокируется (lock) для другого потока. Для wait требуется функциональность lock/unlock, поэтому std::lock_guard не подойдет, только std::unique_lock. После wait рекомендуют всегда проверять истинность условие пробуждения в цикле или в предикате.
 - wait_for(mutex) - атомарно освобождает (unlock) mutex для другого потока и приостанавливает выполнение текущего потока на ОПРЕДЕЛЕННОЕ ВРЕМЯ или не произойдет ложное пробуждение, затем текущий поток пробуждается и mutex блокируется (lock) для другого потока.
 - wait_until(mutex) - атомарно освобождает (unlock) mutex для другого потока и приостанавливает выполнение текущего потока до НАСТУПЛЕНИЕ МОМЕНТА ВРЕМЕНИ (например, 11:15:00) или не произойдет ложное пробуждение, затем текущий поток пробуждается и mutex блокируется (lock) для другого потока.
 
 Отличие от std::atomic: std::atomic использует цикл активного ожидания, что приводит к трате процессорного времени на ожидание освобождения блокировки другим потоком, но тратит меньше времени на процедуру блокировки потока, т.к. не требуется задействование планировщика задач (Scheduler) с переводом потока в заблокированное состояние через походы в ядро процессора.
 */

namespace cv
{
    void start()
    {
        // std::condition_variable
        {
            std::cout << "std::condition_variable" << std::endl;
            std::mutex mutex;
            std::condition_variable cv;
            
            // notify_one
            {
                std::cout << "notify_one" << std::endl;
                std::string data;
                
                auto SetSymbol = [&](const std::string& iSymbols)
                {
                    {
                        std::lock_guard lock(mutex);
                        std::this_thread::sleep_for(std::chrono::seconds(1));
                        data = iSymbols;
                    }
                    
                    cv.notify_one(); // разрешаем одному потоку
                };
                
                // 1 Способ: while и без предиката
                {
                    std::cout << "1 Способ: while и без предиката" << std::endl;
                    
                    auto PrintSymbol = [&]()
                    {
                        std::unique_lock lock(mutex);
                        while (data.empty()) // обработка ложных пробуждений (spurious wakeup)
                            cv.wait(lock); // unlock mutex, чтобы в SetSymbol записались данные
                        
                        std::cout << data << std::endl;
                        data.clear();
                    };
                    
                    std::string symbols = {"----------"};
                    std::thread thread1(PrintSymbol);
                    std::thread thread2(SetSymbol, symbols);
                    
                    thread1.join();
                    thread2.join();
                }
                // 2 Способ: с предикатом
                {
                    std::cout << "2 Способ: с предикатом" << std::endl;
                    
                    auto PrintSymbol = [&]()
                    {
                        std::unique_lock lock(mutex);
                        cv.wait(lock, [&data](){ return !data.empty();}); // обработка ложных пробуждений (spurious wakeup)
                        
                        std::cout << data << std::endl;
                        data.clear();
                    };
                    
                    std::string symbols = {"----------"};
                    std::thread thread1(PrintSymbol);
                    std::thread thread2(SetSymbol, symbols);
                    
                    thread1.join();
                    thread2.join();
                }
            }
            // notify_all
            {
                std::cout << "notify_all" << std::endl;
                // 1 Способ: обычный
                {
                    std::cout << "1 Способ: Обычный" << std::endl;
                    std::string data;
                    
                    auto SetSymbol = [&](const std::string& iSymbols)
                    {
                        {
                            std::lock_guard lock(mutex);
                            std::this_thread::sleep_for(std::chrono::seconds(1));
                            data = iSymbols;
                        }
                        
                        cv.notify_all(); // разрешаем всем потокам
                    };
                    
                    auto PrintSymbol = [&](int indexThread)
                    {
                        std::unique_lock lock(mutex);
                        cv.wait(lock, [&data](){ return !data.empty();}); // обработка ложных пробуждений (spurious wakeup)
                        
                        std::cout << "Индекс потока: " << indexThread << ", данные:" << data << std::endl;
                        // data.clear();
                    };
                    
                    std::thread threads[10];
                    for (const auto i : std::views::iota(0, 10))
                        threads[i] = std::thread(PrintSymbol, i);
                    std::string symbols = {"++++++++++"};
                    std::thread(SetSymbol, symbols).join();
                    for (auto& thread : threads)
                        thread.join();
                    
                    std::cout << std::endl;
                }
                // 2 Способ: реализация семафора - обработка потоков по 2 штуки
                {
                    std::cout << "2 Способ: реализация семафора - обработка потоков по 2 штуки" << std::endl;
                    int countThread = 0;
                    
                    auto PrintSymbol = [&](const std::string& data)
                    {
                        static std::mutex m;
                        auto lock = std::unique_lock<std::mutex>(m);
                        std::cout << data << std::endl;
                    };
                    
                    auto SetSymbol = [&](int i)
                    {
                        auto lock = std::unique_lock(mutex);
                        cv.wait(lock, [&countThread] { return countThread < 2; }); // обработка до 2 потоков
                        
                        ++countThread; // увеличение потоков
                        lock.unlock();
                        PrintSymbol("Поток: " + std::to_string(i));
                        std::this_thread::sleep_for(std::chrono::milliseconds(500));
                        
                        lock.lock();
                        --countThread; // уменьшение числа потоков
                        lock.unlock();
                        
                        cv.notify_all(); // разрешаем всем потокам
                    };
                    
                    std::thread threads[10];
                    for (const auto i : std::views::iota(0, 10))
                        threads[i] = std::thread(SetSymbol, i);
                    for (auto& thread : threads)
                    {
                        if (thread.joinable())
                            thread.join();
                    }
                    
                    std::cout << std::endl;
                }
                /*
                 3 Способ: std::notify_all_at_thread_exit - принимает в качестве аргументов std::condition_variable и std::unique_lock. При завершении потока и выхода из стека, когда все деструкторы локальных (по отношению к потоку) объектов отработали, выполняется notify_all в захваченном condition_variable. Поток, вызвавший std::notify_all_at_thread_exit, будет обладать mutex до самого завершения, поэтому необходимо позаботиться о том, чтобы не произошёл deadlock где-нибудь в другом месте. std::notify_all_at_thread_exit - использовать в редких случаях, когда необходимо гарантировать к определенному моменту уничтожение локальных объектов + нельзя использовать join у потока по какой-то причине.
                 */
                {
                    std::cout << "3 Способ: использование std::notify_all_at_thread_exit" << std::endl;
                    
                    std::string data;
                    
                    auto SetSymbol = [&](const std::string& iSymbols)
                    {
                        std::unique_lock lock(mutex);
                        std::this_thread::sleep_for(std::chrono::milliseconds(500));
                        data = iSymbols;
                        std::notify_all_at_thread_exit(cv, std::move(lock));
                    };
                    
                    auto PrintSymbol = [&](int indexThread)
                    {
                        std::unique_lock lock(mutex);
                        cv.wait(lock, [&data](){ return !data.empty();}); // обработка ложных пробуждений (spurious wakeup)
                        
                        std::cout << "Индекс потока: " << indexThread << ", данные:" << data << std::endl;
                        // data.clear();
                    };
                    
                    std::thread threads[100];
                    for (const auto i : std::views::iota(0, 100))
                        threads[i] = std::thread(PrintSymbol, i);
                    std::string symbols = {"++++++++++"};
                    std::thread(SetSymbol, symbols).detach();
                    for (auto& thread : threads)
                        thread.detach();
                    
                    std::this_thread::sleep_for(std::chrono::milliseconds(500)); // Чтобы PrintSymbol успел вывести данные, но не все
                    std::cout << std::endl;
                }
            }
            // Spinlock
            {
                std::cout << "Spinlock" << std::endl;
                
                cv::Spinlock spinlock;
                std::thread thread1([&] {PrintSymbol('+', spinlock);});
                std::thread thread2([&] {PrintSymbol('-', spinlock);});
                
                thread1.join();
                thread2.join();
                
                std::cout << std::endl;
            }
        }
        
        /* 
         std::condition_variable_any - принимает любой объект (mutex, lock), который может выполнить lock/unlock. Работает медленее, чем std::condition_variable.
         */
        {
            std::cout << "std::condition_variable_any" << std::endl;
            std::condition_variable_any cv;
            std::shared_mutex mutex;
            std::string data;
            
            auto SetSymbol = [&](const std::string& iSymbols)
            {
                {
                    std::unique_lock lock(mutex);
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                    data = iSymbols;
                }
                
                cv.notify_all(); // разрешаем одному потоку
            };
            
            // std::mutex;
            {
                std::cout << "std::mutex" << std::endl;
                
                auto PrintSymbol = [&]()
                {
                    mutex.lock();
                    while (data.empty()) // обработка ложных пробуждений (spurious wakeup)
                        cv.wait(mutex); // unlock mutex, чтобы в SetSymbol записались данные
                    
                    std::cout << data << std::endl;
                    mutex.unlock();
                };
                
                std::string symbols = {"----------"};
                std::vector<std::thread> threads(10);
                for (auto& thread: threads)
                    thread = std::thread(PrintSymbol);
                
                std::thread(SetSymbol, symbols).join();
                for (auto& thread : threads)
                {
                    if (thread.joinable())
                        thread.join();
                }
                
                data.clear();
                std::cout << std::endl;
            }
            // std::shared_mutex;
            {
                std::cout << "std::shared_mutex" << std::endl;
                
                auto PrintSymbol = [&]()
                {
                    std::shared_lock lock(mutex);
                    cv.wait(lock, [&data](){ return !data.empty();}); // обработка ложных пробуждений (spurious wakeup)
                    
                    std::cout << data << std::endl;
                };
                
                std::string symbols = {"----------"};
                std::vector<std::thread> threads(10);
                for (auto& thread: threads)
                    thread = std::thread(PrintSymbol);
                
                std::thread(SetSymbol, symbols).join();
                for (auto& thread : threads)
                {
                    if (thread.joinable())
                        thread.join();
                }
                
                data.clear();
                std::cout << std::endl;
            }
        }
    }
}
