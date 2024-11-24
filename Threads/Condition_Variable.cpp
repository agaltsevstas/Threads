#include "Condition_Variable.hpp"

#include <condition_variable>
#include <iostream>
#include <mutex>
#include <thread>
#include <ranges>
#include <shared_mutex>
#include <vector>

/*
 Лекция: https://www.youtube.com/watch?v=79d5WI5RTp8&ab_channel=ComputerScienceCenter
 
 Сайты: https://nuancesprog.ru/p/6546/?ysclid=lywwvgiuse463489148
        https://rsdn.org/forum/cpp.applied/5092670.hot
        https://ru.stackoverflow.com/questions/1166689/%D0%B7%D0%B0%D1%87%D0%B5%D0%BC-%D1%83%D1%81%D0%BB%D0%BE%D0%B2%D0%BD%D0%BE%D0%B9-%D0%BF%D0%B5%D1%80%D0%B5%D0%BC%D0%B5%D0%BD%D0%BD%D0%BE%D0%B9-%D0%BD%D1%83%D0%B6%D0%B5%D0%BD-%D0%BC%D1%8C%D1%8E%D1%82%D0%B5%D0%BA%D1%81
 */

namespace cv
{
    class Spinlock
    {
        Spinlock(const Spinlock&) = delete;
        Spinlock(Spinlock&&) noexcept = delete;
        Spinlock& operator=(const Spinlock&) = delete;
        Spinlock& operator=(Spinlock&&) noexcept = delete;
        
    public:
        Spinlock() = default;
        ~Spinlock() = default;
        
        void Lock()
        {
            std::unique_lock lock(_mutex);
            _cv.wait(lock, [this] { return !_flag.exchange(true); });
        }
        
        bool Try_lock()
        {
            return !_flag.exchange(true);
        }
        
        void Unlock()
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
 std::condition_variable (условная переменная) - механизм синхронизации между потоками, который работает ТОЛЬКО в паре mutex + unique_lock. Используется для блокировки одного или нескольких потоков с помощью wait до тех пор, пока другой поток не уведомит condition_variable о разблокировке одного (notify_one) или нескольких потоков (notify_any).

 spurious wakeup (ложные пробуждения) - пробуждение потока без веской причины. Это происходит потому что между моментом сигнала (notify) от condition_variable и моментом запуска ожидающего потока, другой поток запустился и изменил условие, вызвав состояние гонки. Если ожидающий поток просыпается вторым, он проигрывает гонку и происходит ложное пробуждение, поэтому после wait рекомендуют всегда проверять истинность условие пробуждения в цикле или в предикате.
 
 Методы:
 - notify_one - в wait удаляется ОДИН поток из очереди и он пробуждается. Например, 1 читатель.
 - notify_all - в wait удаляются ВСЕ потоки из очереди и они пробуждаются. Например, много читателей.
 - wait(mutex)/wait(mutex, predicate) - освобождает (unlock) mutex для текущего потока, чтобы другие потоки могли захватить mutex.
   При использовании предиката, проверяется условие в придикате:
   • условие НЕвыполнено. Идет засыпание потока и помещение его в очередь ожидающих потоков с помощью планировщика (Scheduler) через поход в ядро процессора - и так все потоки-читатели поочередно помещаются в очередь ожидания.
   • условие ВЫПОЛНЕНО или пришло уведомление notify_one (удаляется ОДИН поток из очереди и он пробуждается)/notify_all (удаляются ВСЕ потоки из очереди и они пробуждаются). Происходит попытка захвата mutex (lock) ОДНИМ потоком, продолжающаяся до тех пор, пока он не будет захвачен (бывает другой поток успел раньше захватить mutex и не отпустил его еще). После захвата (lock) mutexа ОДНИМ потоком, он выходит из wait, читает данные, выходит из области видимости unique_lock, освобождая mutex (unlock). При notify_all остальные пробудившиеся потоки поочередно выходят из wait, захватывая (lock) и освобождая (unlock) mutex.
 - wait_for(mutex) - приостанавливает выполнение потоков на ОПРЕДЕЛЕННОЕ ВРЕМЯ.
 - wait_until(mutex) - приостанавливает выполнение потоков до НАСТУПЛЕНИЕ МОМЕНТА ВРЕМЕНИ (например, 11:15:00).
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
            
            // notify_one - в wait удаляется ОДИН поток из очереди и он пробуждается. Например, 1 читатель.
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
                    
                    // Нужно уведомлять условную переменную после того, как отпустили mutex (unlock), чтобы wait сразу же захватил (lock) mutex и тратил время на ожидание разблокировки (unlock), поэтому он выносится за скобки
                    cv.notify_one(); // в wait удаляется ОДИН поток из очереди и он пробуждается
                };
                
                /* 
                 1 Способ: с предикатом
                 Идет проверка условия в предикате для предотвращения ложных пробуждений::
                 1) в случае НЕвыполнения условия (например, поток-писатель еще не выполнил условие): идет засыпание потока и помещение его в очередь ожидающих потоков с помощью планировщика (Scheduler) через поход в ядро процессора - и так все потоки-читатели поочередно помещаются в очередь ожидания.
                 2) в случае ВЫПОЛНЕНИЯ условия очередь ожидающих потоков может быть ПУСТОЙ и НЕПУСТОЙ:
                    - при НЕПУСТОЙ очереди (например, потоки-читатели успели захватить mutex (lock) РАНЬШЕ, чем поток-писатель, невыполнивший еще условие), пришло уведомление:
                    • notify_one: удаляется ОДИН поток из очереди и он пробуждается.
                    • notify_all: удаляются ВСЕ потоки из очереди и они пробуждаются.
                    Происходит попытка захвата mutex (lock) ОДНИМ потоком, продолжающаяся до тех пор, пока он не будет захвачен (бывает другой поток успел раньше захватить mutex и не отпустил его еще). После захвата (lock) mutexа ОДНИМ потоком, он выходит из wait, читает данные, выходит из области видимости unique_lock, освобождая mutex (unlock). При notify_all остальные пробудившиеся потоки поочередно выходят из wait, захватывая (lock) и освобождая (unlock) mutex.
                    - при ПУСТОЙ очереди (например, потоки-читатели успели захватить mutex (lock) ПОЗЖЕ, чем поток-писатель, уже выполнивший условие): происходит тоже самое, что и при НЕПУСТОЙ ОЧЕРЕДИ, но с одним ОТЛИЧИЕМ - потоки не находятся в очереди и их от туда удалять не нужно и делать уведомление notify_one/notify_all так же не нужно, они поочередно выходят из wait, захватывая (lock) и освобождая (unlock) mutex.
                 */
                {
                    std::cout << "1 Способ: с предикатом" << std::endl;
                    
                    // Предикат в while
                    {
                        std::cout << "Предикат в while" << std::endl;
                        
                        auto PrintSymbol = [&]()
                        {
                            std::unique_lock lock(mutex);
                            while (data.empty()) // ждать пока данные пустые + обработка ложных пробуждений (spurious wakeup)
                            {
                                /*
                                 в случае НЕвыполнения условия (например, поток-писатель еще не выполнил условие): идет засыпание потока и помещение его в очередь ожидающих потоков с помощью планировщика (Scheduler) через поход в ядро процессора - и так все потоки-читатели поочередно помещаются в очередь ожидания.
                                 в случае ВЫПОЛНЕНИЯ условия: происходит попытка захвата mutex (lock) ОДНИМ потоком, продолжающаяся до тех пор, пока он не будет захвачен (бывает другой поток успел раньше захватить mutex и не отпустил его еще). После захвата (lock) mutexа ОДНИМ потоком, он выходит из wait, читает данные, выходит из области видимости unique_lock, освобождая mutex (unlock).
                                 */
                                
                                cv.wait(lock);
                            }
                            
                            std::cout << data << std::endl;
                            data.clear();
                        };
                        
                        std::string symbols = {"----------"};
                        std::thread thread1(PrintSymbol);
                        std::thread thread2(SetSymbol, symbols);
                        
                        thread1.join();
                        thread2.join();
                    }
                    // Предикат в wait
                    {
                        std::cout << "Предикат в wait" << std::endl;
                        
                         
                    }
                }
                /* 
                 2 Способ: без предиката
                 WAIT БЕЗ ПРЕДИКАТА. Потоки-читатели работают также как и с wait с ПРЕДИКАТОМ, но с 2 отличиями:
                 - не нужно проверять постоянно предикат.
                 - потоки всегда будут попадать в очередь ожидающих потоков и извлекать их из очереди нужно с помощью notify_one/notify_all.
                 */
                {
                    std::cout << "2 Способ: без предиката" << std::endl;
                    
                    auto PrintSymbol = [&]()
                    {
                        std::unique_lock lock(mutex);
                        cv.wait(lock); // потоки всегда будут попадать в очередь ожидающих потоков и извлекать их из очереди нужно с помощью notify_one/notify_all
                        
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
            // notify_all - в wait удаляются ВСЕ потоки из очереди и они пробуждаются. Например, много читателей.
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
                        
                        // Нужно уведомлять условную переменную после того, как отпустили mutex (unlock), чтобы wait сразу же захватил (lock) mutex и тратил время на ожидание разблокировки (unlock), поэтому он выносится за скобки
                        cv.notify_all(); // в wait удаляются ВСЕ потоки из очереди и они пробуждаются
                    };
                    
                    auto PrintSymbol = [&](int indexThread)
                    {
                        std::unique_lock lock(mutex);
                        cv.wait(lock, [&data](){ return !data.empty();}); // ждать пока данные пустые + обработка ложных пробуждений (spurious wakeup)
                        
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
                        
                        cv.notify_all(); // в wait удаляются ВСЕ потоки из очереди и они пробуждаются
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
                        cv.wait(lock, [&data](){ return !data.empty();}); // ждать пока данные пустые + обработка ложных пробуждений (spurious wakeup)
                        
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
         std::condition_variable_any - принимает любой объект (не только unique_lock, но может быть обычный mutex), который может выполнить lock/unlock. Работает медленее, чем condition_variable.
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
                
                // Нужно уведомлять условную переменную после того, как отпустили mutex (unlock), чтобы wait сразу же захватил (lock) mutex и тратил время на ожидание разблокировки (unlock), поэтому он выносится за скобки
                cv.notify_all(); // в wait удаляются ВСЕ потоки из очереди и они пробуждаются
            };
            
            // std::mutex;
            {
                std::cout << "std::mutex" << std::endl;
                
                auto PrintSymbol = [&]()
                {
                    mutex.lock();
                    while (data.empty()) // ждать пока данные пустые + обработка ложных пробуждений (spurious wakeup)
                        cv.wait(mutex);
                    
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
                    cv.wait(lock, [&data](){ return !data.empty();}); // ждать пока данные пустые + обработка ложных пробуждений (spurious wakeup)
                    
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
