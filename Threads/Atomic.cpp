#include "Atomic.hpp"

#include <atomic>
#include <cassert>
#include <iostream>
#include <thread>
#include <vector>

namespace atomic
{
    // compare_exchange_weak
    namespace compare_exchange_strong
    {
        class Spinlock
        {
            Spinlock(const Spinlock&) = delete;
            
        public:
            Spinlock() = default;
            ~Spinlock() = default;
            
            void Lock() noexcept
            {
                while(_flag);
                bool expected = false;
                _flag.compare_exchange_strong(expected, true);
            }
            
            bool Try_lock() noexcept
            {
                bool expected = false;
                return _flag.compare_exchange_strong(expected, true);
            }
            
            void Unlock() noexcept
            {
                _flag = false;
            }
            
        private:
            std::atomic<bool> _flag = ATOMIC_FLAG_INIT; // false
        };
    }
    
    // compare_exchange_strong
    namespace compare_exchange_weak
    {
        class Spinlock
        {
        public:
            void Lock()
            {
                bool expected = false;
                while (!_flag.compare_exchange_weak(expected, true, std::memory_order_acquire)) // может быть ложное срабатывания
                    expected = false;
            }
            
            void Try_lock()
            {
                bool expected = false;
                _flag.compare_exchange_weak(expected, true, std::memory_order_acquire);
            }
         
            void Unlock()
            {
                _flag.store(false, std::memory_order_release);
            }
         
        private:
            std::atomic<bool> _flag;
        };
    }

    namespace wait
    {
        class Spinlock
        {
            Spinlock(const Spinlock&) = delete;
            
        public:
            Spinlock() = default;
            ~Spinlock() = default;
            
            void Lock() noexcept
            {
                if (_flag.test_and_set(std::memory_order_acquire)) // read
                    _flag.wait(true);
            }
            
            bool Try_lock() noexcept
            {
                return !_flag.test_and_set(std::memory_order_acquire);
            }
            
            void Unlock() noexcept
            {
                _flag.clear(std::memory_order_release); // write
                _flag.notify_one();
            }
            
        private:
            std::atomic_flag _flag = ATOMIC_FLAG_INIT; // false
        };
    }
}

namespace atomic_flag
{
    class Spinlock
    {
        Spinlock(const Spinlock&) = delete;
        
    public:
        Spinlock() = default;
        ~Spinlock() = default;
        
        void Lock() noexcept
        {
            if (_flag.test_and_set(std::memory_order_acquire)) // read
                _flag.wait(true);
        }
        
        bool Try_lock() noexcept
        {
            return !_flag.test_and_set(std::memory_order_acquire);
        }
        
        void Unlock() noexcept
        {
            _flag.clear(std::memory_order_release); // write
            _flag.notify_one();
        }
        
    private:
        std::atomic_flag _flag = ATOMIC_FLAG_INIT; // false
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
 std::atomic - атомарная операция. Операция называется атомарной, если операция выполнена целиком, либо не выполнена полностью, поэтому нет промежуточного состояние операции.
 
 spurious wakeup (ложные пробуждения) - пробуждение потока без веской причины. Это происходит потому что между моментом сигнала от std::atomic и моментом запуска ожидающего потока, другой поток запустился и изменил условие, вызвав состояние гонки. Если поток просыпается вторым, он проиграет гонку и произойдет ложное пробуждение. После wait рекомендуют всегда проверять истинность условие пробуждения в цикле или в предикате.
 
 Методы:
 - is_lock_free - TODO
 - store - запись значения.
 - load - возвращение значения.
 - exchange - замена значения и возвращение старого значения.
 - compare_exchange_weak(expected, desired) - сравнивает значение atomic с аргументом expected. Если значения совпадают, то desired записывается в atomic и возвращается true. Если значения НЕ совпадают, то в аргумент expected записывается значение atomic и возвращается false. Возможны ложные пробуждения (spurious wakeup).
 - compare_exchange_strong(expected, desired) - в отличии от compare_exchange_weak включает обработку ложных срабатываний и реализован внутри как вызов compare_exchange_weak в цикле.
 - wait - блокирует поток до тех пор, пока не будет уведомления notify_one/notify_all и не изменится значение. Например, wait(true) блокируется при значении true, разблокируется при значении false. wait(false) блокируется при значении false, разблокируется при значении true. Ложные пробуждения базовой реализация wait скрыты в стандартной библиотеки STL, поэтому std::wait не имеет ложных пробуждений. Внутри std::wait повторно выполняются действия по порядку:
     1. сравнивает значение со старым значение -> пункт 1.
     2. если они != то возврат из функции или -> пункт 3.
     3. блокируется до тех пор, пока он не будет разблокирован notify_one/notify_all или не будет разблокирован ложно -> пункт 1.
 - notify_one - снятие блокировки, ожидание 1 потока. Например, 1 писатель или много писателей.
 - notify_all - ожидание многих потоков. Например, много читаталей.
 - fetch_add - сложение значения c atomic и возвращение старого значения.
 - fetch_sub - вычитание значения из atomic и возвращение старого значения.
 - fetch_and - побитовое И между значением и atomic и возвращение старого значения.
 - fetch_or - побитовое ИЛИ между значением и atomic и возвращение старого значения.
 - fetch_xor - побитовое исключающее ИЛИ между значением и atomic и возвращение старого значения.
 
 Отличие от std::condition_variable: std::atomic использует цикл активного ожидания, что приводит к трате процессорного времени на ожидание освобождения блокировки другим потоком, но тратит меньше времени на процедуру блокировки потока, т.к. не требуется задействование планировщика задач (Scheduler) с переводом потока в заблокированное состояние через походы в ядро процессора.
 
 */

/*
 Для оптимизации работы с памятью у каждого ядра имеется его личный кэш памяти, над ним стоит общий кэш памяти процессора, далее оперативная память. Задача синхронизации памяти между ядрами - поддержка консистентного представления данных на каждом ядре (в каждом потоке). Если применить строгую упорядоченность изменений памяти, то операции на разных ядрах уже не будут выполнятся параллельно: остальные ядра будут ожидать, когда одно ядро выполнит изменения данных. Поэтому процессоры поддерживают работу с памятью с менее строгими гарантиями консистентности памяти. Разработчику предоставляется выбор: гарантии по доступу к памяти из разных потоков требуются для достижения максимальной корректности и производительности многопоточной программы.
 Модели памяти в std::atomic - это гарантии корректности доступа к памяти из разных потоков. По-умолчанию компилятор предполагает, что работа идет в одном потоке и код будет выполнен последовательно, но компилятор может переупорядочить команды программы с целью оптимизации. Поэтому в многопоточности требуется соблюдать правила упорядочивания доступа к памяти, что позволяет с синхронизировать потоки с определенной степенью синхронизации без использования дорогостоящего std::mutex.
 Преимущества: более эффективные и легкие и не требуют std::mutex.
 */

namespace ATOMIC
{
    // По возрастанию строгости
    enum memory_order
    {
        memory_order_relaxed, // Все можно делать
        memory_order_consume, // Упрощенный memory_order_acquire, гарантирует, что операции будут выполняться в правильном порядке только те, которые зависят от одних данных.
        memory_order_acquire, // Можно пускать вниз операцию STORE, но нельзя операцию LOAD и никакую операцию вверх.
        memory_order_release, // Можно пускать вверх операцию LOAD, но нельзя операцию STORE и никакую операцию вниз.
        memory_order_acq_rel, // memory_order_acquire + memory_order_acq_rel. Можно пускать вниз операцию LOAD, но нельзя операцию STORE. Можно пускать вверх операцию STORE, но нельзя операцию LOAD. Дорогая операция, но дешевле mutex. Используется в x86/64.
        memory_order_seq_cst // Установлено по-умолчанию в atomic, компилятор не может делать переупорядочивание (reordining) - менять порядок в коде. Самая дорогая операция, но дает 100% корректности.
    };

    void Start()
    {
        std::cout << "std::atomic" << std::endl;
        /*
         Различие обычной переменной и std::atomic:
         - обычная переменная number1 имеет три операции: read-modify-write, поэтому нет гарантий, что другое ядро процессора не выполняет другой операции над number1.
         - std::atomic переменная number2 имеет одну операцию одну операция с lock сигналом на уровне процессора, гарантирующая, что к кэш линии, в которой лежит number2, эксклюзивно имеет доступ только ядро, выполняющее эту инструкцию.
         */
        {
            int number1 = 0;
            std::atomic<int> number2 = 0;
            
            ++number1;
            /* Generated x86-64 assembly:
                mov     eax, DWORD PTR v1[rip]
                add     eax, 1
                mov     DWORD PTR v1[rip], eax
            */
            
            ++number2;
            /* Generated x86-64 assembly:
                mov     eax, 1
                lock xadd       DWORD PTR _ZL2v2[rip], eax
            */
        }
        // exchange - замена значения
        {
            std::atomic<bool> flag = false;
            bool exchange1 = flag.exchange(true); // false
            bool exchange2 = flag.exchange(false); // true
        }
        /*
         - compare_exchange_weak(expected, desired) - сравнивает значение atomic с аргументом expected. Если значения совпадают, то desired записывается в atomic и возвращается true. Если значения НЕ совпадают, то в аргумент expected записывается значение atomic и возвращается false. Возможны ложные пробуждения (spurious wakeup).
         - compare_exchange_strong(expected, desired) - в отличии от compare_exchange_weak включает обработку ложных срабатываний и реализован внутри как вызов compare_exchange_weak в цикле.
         */
        {
            // Возможности compare_exchange_weak/compare_exchange_strong
            {
                std::cout << "Возможности compare_exchange_strong" << std::endl;
                std::atomic<bool> flag = false;
                bool expected = false;
                bool exchanged1 = flag.compare_exchange_strong(expected, true); // flag == true, expected == true, exchanged == true
                
                expected = false;
                bool exchanged2 = flag.compare_exchange_strong(expected, true); // flag == true, expected == true, exchanged == false
                bool exchanged3 = flag.compare_exchange_strong(expected, false); // flag == false, expected == true, exchanged == true
                bool exchanged4 = flag.compare_exchange_strong(expected, false); // flag == false, expected == false, exchanged == false
            }
            
            // Spinlock + compare_exchange_strong
            {
                std::cout << "Spinlock + compare_exchange_strong" << std::endl;
                
                atomic::compare_exchange_strong::Spinlock spinlock;
                std::thread thread1([&] {PrintSymbol('+', spinlock);});
                std::thread thread2([&] {PrintSymbol('-', spinlock);});
                
                thread1.join();
                thread2.join();
                
                std::cout << std::endl;
            }
            // Spinlock + compare_exchange_weak
            {
                std::cout << "Spinlock + compare_exchange_weak" << std::endl;
                
                atomic::compare_exchange_weak::Spinlock spinlock;
                std::thread thread1([&] {PrintSymbol('+', spinlock);});
                std::thread thread2([&] {PrintSymbol('-', spinlock);});
                
                thread1.join();
                thread2.join();
                
                std::cout << std::endl;
            }
        }
        /*
         - wait - блокирует поток до тех пор, пока не будет уведомления notify_one/notify_all и не изменится значение. Например, wait(true) блокируется при значении true, разблокируется при значении false. wait(false) блокируется при значении false, разблокируется при значении true. Ложные пробуждения базовой реализация wait скрыты в стандартной библиотеки STL, поэтому std::wait не имеет ложных пробуждений. Внутри std::wait повторно выполняются действия по порядку:
             1. сравнивает значение со старым значение -> пункт 1.
             2. если они != то возврат из функции или -> пункт 3.
             3. блокируется до тех пор, пока он не будет разблокирован notify_one/notify_all или не будет разблокирован ложно -> пункт 1.
         - notify_one - снятие блокировки, ожидание 1 потока. Например, 1 писатель или много писателей.
         - notify_all - ожидание многих потоков. Например, много читаталей.
         */
        {
            std::cout << "wait + notify_one" << std::endl;
            
            atomic::wait::Spinlock spinlock;
            std::thread thread1([&] {PrintSymbol('+', spinlock);});
            std::thread thread2([&] {PrintSymbol('-', spinlock);});
            
            thread1.join();
            thread2.join();
            
            std::cout << std::endl;
        }
        // Memory order
        {
            // Без std::atomic (без синхронизации), 2 потока могут одновременно менять одну переменную, увеличивая счетчик +1, вместо +1+1
            {
                std::cout << "Без std::atomic (без синхронизации), 2 потока могут одновременно менять одну переменную, увеличивая счетчик +1, вместо +1+1" << std::endl;
                int count = 0;
                
                auto Write = [&]()
                {
                    std::cout << "Write count = " << ++count << std::endl;
                };
                
                std::thread thread1(Write);
                std::thread thread2(Write);
                
                thread1.join();
                thread2.join();
                
                std::cout << std::endl;
            }
            // std::atomic
            {
                std::atomic<bool> flag = false;
                std::string data;
                std::string symbols = {"++++++++++"};
                
                /*
                 memory_order_relaxed - гарантирует только свойство атомарности операций, при этом не участвовует в процессе синхронизации данных между потоками.
                 Свойства:
                 - изменения переменной "появится" не сразу в другом потоке.
                 */
                {
                    std::cout << "relaxed" << std::endl;
                    
                    // Неправильное использование: нет гарантий, что поток thread2 увидит изменения data ранее, чем изменение флага ready, т.к. синхронизацию памяти флаг relaxed не обеспечивает.
                    {
                        std::cout << "Неправильное использование" << std::endl;
                        
                        auto Write = [&](const std::string& iSymbols)
                        {
                            data = iSymbols; // Может поменяться местами с flag.store
                            // LOAD (yes) ↑ STORE (yes)
                            flag.store(true, std::memory_order_relaxed); // запись, может поменяться местами с data = iSymbols
                            // LOAD (yes) ↓ STORE (yes)
                        };
                        
                        auto Read = [&]()
                        {
                            // LOAD (yes) ↑ STORE (yes)
                            if (flag.load(std::memory_order_relaxed)) // чтение, может поменяться местами с std::cout << data
                            {
                            // LOAD (yes) ↓ STORE (yes)
                                std::cout << data << std::endl; // Может поменяться местами с flag.load
                                data.clear();
                                flag.store(false); // запись
                            }
                        };
                        
                        std::thread thread1(Write, symbols);
                        std::thread thread2(Read);
                        
                        thread1.join();
                        thread2.join();
                    }
                    // Правильное использование
                    {
                        std::cout << "Правильное использование" << std::endl;
                        std::atomic<int> count = 0;
                        
                        auto Write = [&]()
                        {
                            count.fetch_add(1, std::memory_order_relaxed); // запись
                        };
                        
                        auto Read = [&]()
                        {
                            std::cout << "Read count = " << count.load(std::memory_order_relaxed) << std::endl;
                        };
                        
                        std::thread thread1(Write);
                        std::thread thread2(Read);
                        
                        thread1.join();
                        thread2.join();
                    }
                    
                    std::cout << std::endl;
                }
                /*
                 memory_order_acquire + memory_order_release
                 memory_order_acquire - можно пускать вниз операцию STORE, но нельзя операцию LOAD и никакую операцию наверх.
                 memory_order_release - можно пускать вверх операцию LOAD, но нельзя операцию STORE и никакую операцию вниз.
                 */
                {
                    // 1 Способ обычный: acquire + release
                    {
                        std::cout << "1 Способ обычный: acquire + release" << std::endl;
                        
                        auto Write = [&](const std::string& iSymbols)
                        {
                            data = iSymbols;
                            // LOAD (yes) ↑ STORE (no)
                            flag.store(true, std::memory_order_release); // запись
                            // LOAD (no) ↓ STORE (no)
                        };
                        
                        auto Read = [&]()
                        {
                            std::this_thread::sleep_for(std::chrono::milliseconds(10)); // задержка, чтобы thread1 успел записать
                            // LOAD (no) ↑ STORE (no)
                            if (flag.load(std::memory_order_acquire)) // чтение
                            {
                            // LOAD (no) ↓ STORE (yes)
                                std::cout << data << std::endl;
                                data.clear();
                                flag.store(false); // запись
                            }
                        };
                        
                        std::thread thread1(Write, symbols);
                        std::thread thread2(Read);
                        
                        thread1.join();
                        thread2.join();
                    }
                    // 2 Способ необычный: relaxed + acquire + release
                    {
                        std::cout << "2 Способ необычный: relaxed + acquire + release" << std::endl;
                        
                        auto Write = [&](const std::string& iSymbols)
                        {
                            data = iSymbols;
                            // LOAD (yes) ↑ STORE (no)
                            std::atomic_thread_fence(std::memory_order_release);
                            // LOAD (no) ↓ STORE (no)
                            flag.store(true, std::memory_order_relaxed); // запись
                        };
                        
                        auto Read = [&]()
                        {
                            std::this_thread::sleep_for(std::chrono::milliseconds(10)); // задержка, чтобы thread1 успел записать
                            if (flag.load(std::memory_order_relaxed)) // чтение
                            {
                                // LOAD (no) ↑ STORE (no)
                                std::atomic_thread_fence(std::memory_order_acquire);
                                // LOAD (no) ↓ STORE (yes)
                                std::cout << data << std::endl;
                                data.clear();
                                flag.store(false); // запись
                            }
                        };
                         
                        std::thread thread1(Write, symbols);
                        std::thread thread2(Read);
                        
                        thread1.join();
                        thread2.join();
                    }
                    std::cout << std::endl;
                }
                /*
                 memory_order_consume - упрощенный memory_order_acquire, гарантирует, что операции будут выполняться в правильном порядке только те, которые зависят от одних данных.
                 */
                {
                    std::cout << "consume" << std::endl;
                    std::atomic<std::string*> flag_data;
                    
                    int value = 0;
                    auto Write = [&](const std::string& iSymbols)
                    {
                        data = iSymbols;
                        value = 1;
                        // LOAD (yes) ↑ STORE (no) только для переменной data
                        flag_data.store(&data, std::memory_order_release); // чтение
                        // LOAD (no) ↓ STORE (no) только для переменной data
                    };
                    
                    auto Read = [&]()
                    {
                        std::this_thread::sleep_for(std::chrono::milliseconds(10)); // задержка, чтобы thread1 успел записать
                        
                        // LOAD (no) ↑ STORE (no) только для переменной data
                        if (auto data = flag_data.load(std::memory_order_consume)) // чтение
                        {
                        // LOAD (no) ↓ STORE (yes) только для переменной data
                            std::cout << *data << std::endl;
                            data->clear();
                            assert(value == 1); // может переставить выше строки flag_data.load
                        }
                    };
                    
                    std::thread thread1(Write, symbols);
                    std::thread thread2(Read);
                    
                    thread1.join();
                    thread2.join();
                    
                    std::cout << std::endl;
                }
                /*
                 memory_order_seq_cst (sequential consistency) - установлено по-умолчанию в atomic, компилятор не может делать переупорядочивание (reordining) - менять порядок в коде. Самая дорогая операция, но дает 100% корректности.
                 */
                {
                    std::cout << "sequential consistency" << std::endl;
                    std::atomic<bool> flag = false;
                    
                    auto Write = [&](const std::string& iSymbols)
                    {
                        data = iSymbols;
                        // LOAD (no) ↑ STORE (no)
                        flag = true; // запись
                        // LOAD (no) ↓ STORE (no)
                    };
                    
                    auto Read = [&]()
                    {
                        std::this_thread::sleep_for(std::chrono::milliseconds(10)); // задержка, чтобы thread1 успел записать
                        // LOAD (no) ↑ STORE (no)
                        if (flag) // чтение
                        {
                        // LOAD (no) ↓ STORE (no)
                            std::cout << data << std::endl;
                            data.clear();
                            flag.store(false); // запись
                        }
                    };
                    
                    std::thread thread1(Write, symbols);
                    std::thread thread2(Read);
                    
                    thread1.join();
                    thread2.join();
                    
                    std::cout << std::endl;
                }
            }
        }
    }
}

/*
 std::atomic_flag - атомарная булева операция. Операция называется атомарной, если операция выполнена целиком, либо не выполнена полностью, поэтому нет промежуточного состояние операции.
    Методы:
    - clear - сбрасывает значение в false.
    - test - возвращение значения.
    - test_and_set - устанавливает значение true и возвращает предыдущее значение.
    - wait - блокирует поток до тех пор, пока не будет уведомления notify_one/notify_all и не изменится значение. Например, wait(true) блокируется при значении true, разблокируется при значении false. wait(false) блокируется при значении false, разблокируется при значении true. Ложные пробуждения базовой реализация wait скрыты в стандартной библиотеки STL, поэтому std::wait не имеет ложных пробуждений. Внутри std::wait повторно выполняются действия по порядку:
        1. сравнивает значение со старым значение -> пункт 1.
        2. если они != то возврат из функции или -> пункт 3.
        3. блокируется до тех пор, пока он не будет разблокирован notify_one/notify_all или не будет разблокирован ложно -> пункт 1.
    - notify_one - снятие блокировки, ожидание 1 потока. Например, 1 писатель или много писателей.
    - notify_all - ожидание многих потоков. Например, много читаталей.
 */

namespace ATOMIС_FLAG
{
    void Start()
    {
        std::cout << "std::atomic_flag" << std::endl;
        
        // Базовые операции
        {
            std::atomic_flag flag = false;
            auto test1 = flag.test(); // false
            auto test2 = flag.test_and_set(); // test == false, flag == true
            auto test3 = flag.test(); // true
            flag.clear(); // false
            auto test4 = flag.test(); // false
        }
        
        atomic_flag::Spinlock spinlock;
        std::thread thread1([&] {PrintSymbol('+', spinlock);});
        std::thread thread2([&] {PrintSymbol('-', spinlock);});
        
        thread1.join();
        thread2.join();
        
        std::cout << std::endl;
    }
}
