#include "Atomic.hpp"

#include <atomic>
#include <cassert>
#include <iostream>
#include <thread>
#include <vector>


/*
 Сайты: https://vk.com/@habr-kak-rabotat-s-atomarnymi-tipami-dannyh-v-c?ysclid=lyfjpgv4gn827137332
        https://habr.com/ru/articles/328362/
        https://habr.com/ru/articles/195948/
        https://stackoverflow.com/questions/50298358/where-is-the-lock-for-a-stdatomic
 */

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
                // LOAD (no) ↑ STORE (no)
                _flag.compare_exchange_strong(expected, true);
                // LOAD (no) ↓ STORE (no)
            }
            
            bool Try_lock() noexcept
            {
                bool expected = false;
                // LOAD (no) ↑ STORE (no)
                return _flag.compare_exchange_strong(expected, true);
                // LOAD (no) ↓ STORE (no)
            }
            
            void Unlock() noexcept
            {
                // LOAD (no) ↑ STORE (no)
                _flag = false;
                // LOAD (no) ↓ STORE (no)
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
                // LOAD (no) ↑ STORE (no)
                while (!_flag.compare_exchange_weak(expected, true, std::memory_order_acquire)) // может быть ложное срабатывания
                // LOAD (no) ↓ STORE (yes)
                    expected = false;
            }
            
            void Try_lock()
            {
                bool expected = false;
                // LOAD (no) ↑ STORE (no)
                _flag.compare_exchange_weak(expected, true, std::memory_order_acquire);
                // LOAD (no) ↓ STORE (yes)
            }
         
            void Unlock()
            {
                // LOAD (yes) ↑ STORE (no)
                _flag.store(false, std::memory_order_release);
                // LOAD (no) ↓ STORE (no)
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
                // LOAD (no) ↑ STORE (no)
                if (_flag.test_and_set(std::memory_order_acquire)) // read
                // LOAD (no) ↓ STORE (yes)
                    _flag.wait(true);
            }
            
            bool Try_lock() noexcept
            {
                // LOAD (no) ↑ STORE (no)
                return !_flag.test_and_set(std::memory_order_acquire);
                // LOAD (no) ↓ STORE (yes)
            }
            
            void Unlock() noexcept
            {
                // LOAD (yes) ↑ STORE (no)
                _flag.clear(std::memory_order_release); // write
                // LOAD (no) ↓ STORE (no)
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
            // LOAD (no) ↑ STORE (no)
            if (_flag.test_and_set(std::memory_order_acquire)) // read
            // LOAD (no) ↓ STORE (yes)
                _flag.wait(true);
        }
        
        bool Try_lock() noexcept
        {
            // LOAD (no) ↑ STORE (no)
            return !_flag.test_and_set(std::memory_order_acquire);
            // LOAD (no) ↓ STORE (yes)
        }
        
        void Unlock() noexcept
        {
            // LOAD (yes) ↑ STORE (no)
            _flag.clear(std::memory_order_release); // write
            // LOAD (no) ↓ STORE (no)
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
 
 spurious wakeup (ложные пробуждения) - пробуждение потока без веской причины. Это происходит потому что между моментом сигнала от (lock) std::atomic и моментом запуска ожидающего потока, другой поток запустился и изменил условие, вызвав состояние гонки. Если поток просыпается вторым, он проиграет гонку и произойдет ложное пробуждение. После wait рекомендуют всегда проверять истинность условие пробуждения в цикле или в предикате.
 
 ХАРАКТЕРИСТИКИ:
 - atomic может быть встроенный тип (int, double, char), либо это тривиальный класс/структура, которая дожна весить <= 4 или 8 байт из-за размера регистра в 4 байта для 32-битного процессора / 8 байт для 64-битного процессора. В процессоре есть только 2^n разрядные регистры, поэтому для 3/7 байтных структур не подойдет. Однако, это ограничение можно обойти с помощью выравнивания, изменив порядок типов и подложив (padding) неиспользуемые байты границам блоков памяти по 4/8 байт.
   Тривиальный класс/структура (std::is_trivial) - занимает непрерывную область памяти, поэтому компилятор может самостоятельно выбирать способ упорядочивания членов.
   Характеристики:
   — конструктор: явно отсутствующий или явно задан как default.
   — копирующий конструктор: явно отсутствующий или явно задан как default.
   — перемещающий конструктор: явно отсутствующий или явно задан как default.
   — копирующий оператор присваивания: явно отсутствующий или явно задан как default.
   — перемещающий оператор присваивания: явно отсутствующий или явно задан как default.
   — деструктор: явно отсутствующий или явно задан как default.
   - все поля и базовые классы — тоже тривиальные типы.
   - все поля класса - не инициализированны.
   - поля могут иметь разный модификатор доступа (public/protected/private).
   - не имеет виртуальных методов (включая деструктор).
   - не имеет виртуальных базовых типов (класс/структура).
 - с помощью метода is_lock_free - можно проверить возможность применения atomic к типу: true - можно использовать atomic к данному типу / false - нет, лучше использовать mutex (например, нетривиальный тип и больше > 8 байт - размер регистра)
 - ожидание wait блокировки предполагается недолгим, при длительной блокировке невыгоден - пустая трата процессорных ресурсов.
 - в atomic нет атомарного умножения: number2 *= 2; так можно, но это уже будет не атомарная операция: auto int = number * 2;
 
 Методы:
 - is_lock_free - проверяет возможность применения atomic к типу и возвращает значение: true - можно использовать atomic к данному типу / false - нет, лучше использовать mutex (например, нетривиальный тип и больше > 8 байт - размер регистра)
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
 
 ОТЛИЧИЕ от volatile:
 - atomic более дорогостоящая операция, чем volatile, т.к. блокирует шину.
 - volatile использует только одну модель памяти/барьер памяти - memory_order_seq_cst (самая дорогая операция), atomic может использовать 6 моделей памяти/барьеров памяти (строгие/нестрогие - relaxed/acquire/release).
 - volatile не является атомарной операцией, поэтому плохо работает с потоками. Например, если два потока выполняют операцию инкремента над атомарной переменной, то эта переменная гарантированно будет увеличена на 2, в отличии от volatile переменной, где между 2 потоками нет синхронизации и из-за этого они могут одновременно изменять эту переменную, поэтому переменная может быть увеличена как на 1, так и на 2.
 Однако, volatile с типом bool можно применять безопасно в многопоточности (статья Андрея Александреску).
 
 ОТЛИЧИЕ от mutex:
 - atomic блокирует шину адреса и тратит меньше времени, чем mutex блокирует поток с помощью планировщика задач (Scheduler) с переводом потока в заблокированное состояние через походы в ядро процессора.
 - метод is_lock_free в atomic проверяет возможность применения atomic к типу: true - можно использовать atomic к данному типу / false - нет, лучше использовать mutex (например, нетривиальный тип и больше > 8 байт - размер регистра)
 */

/*
 MEMORY ORDER:
 Для оптимизации работы с памятью у каждого ядра имеется его личный кэш памяти, над ним стоит общий кэш памяти процессора, далее оперативная память. Задача синхронизации памяти между ядрами - поддержка консистентного представления данных на каждом ядре (в каждом потоке). Если применить строгую упорядоченность изменений памяти, то операции на разных ядрах уже не будут выполнятся параллельно: остальные ядра будут ожидать, когда одно ядро выполнит изменения данных. Поэтому процессоры поддерживают работу с памятью с менее строгими гарантиями консистентности памяти. Разработчику предоставляется выбор: гарантии по доступу к памяти из разных потоков требуются для достижения максимальной корректности и производительности многопоточной программы.
 Модели памяти в std::atomic - это гарантии корректности доступа к памяти из разных потоков. По-умолчанию компилятор предполагает, что работа идет в одном потоке и код будет выполнен последовательно, но компилятор может переупорядочить команды программы с целью оптимизации. Поэтому в многопоточности требуется соблюдать правила упорядочивания доступа к памяти, что позволяет с синхронизировать потоки с определенной степенью синхронизации без использования дорогостоящего std::mutex.
 */

namespace ATOMIC
{
    // По возрастанию строгости
    enum memory_order
    {
        memory_order_relaxed, // Все можно делать: можно пускать вниз/вверх операцию LOAD/STORE.
        memory_order_consume, // Упрощенный memory_order_acquire, гарантирует, что операции будут выполняться в правильном порядке только те, которые зависят от одних данных.
        memory_order_acquire, // Можно пускать вниз операцию STORE, но нельзя операцию LOAD и никакую операцию вверх.
        memory_order_release, // Можно пускать вверх операцию LOAD, но нельзя операцию STORE и никакую операцию вниз.
        memory_order_acq_rel, // memory_order_acquire + memory_order_acq_rel. Можно пускать вниз операцию LOAD, но нельзя операцию STORE. Можно пускать вверх операцию STORE, но нельзя операцию LOAD. Дорогая операция, но дешевле mutex. Используется в x86/64.
        memory_order_seq_cst // Установлено по-умолчанию в atomic, компилятор не может делать переупорядочивание (reordining) - менять порядок в коде. Самая дорогая операция, но дает 100% корректности. Аналог volatile.
    };

    void Start()
    {
        std::cout << "std::atomic" << std::endl;
        /*
         Различие обычной переменной и std::atomic:
         - обычная переменная number1 имеет три операции: read-modify-write, поэтому нет гарантий, что другое ядро процессора не выполняет другой операции над number1.
         - atomic переменная number2 имеет одну операцию с lock сигналом на уровне процессора, которая блокирует шину адреса, которая обеспечивает обмен информацией между процессором и кэшами, и следовательно atomic эксклюзивно владеет этой переменной из кэша, т.е. если два потока выполняют операцию инкремента над переменной, то эта переменная гарантированно будет увеличена на 2, в отличии от обычной переменной, где между 2 потоками нет синхронизации и из-за этого они могут одновременно изменять эту переменную, поэтому переменная может быть увеличена как на 1, так и на 2.
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
                mov        eax, 1
                lock xadd  DWORD PTR _ZL2v2[rip], eax
            */
        }
        // is_lock_free - проверяет возможность применения atomic к типу и возвращает значение: true - можно использовать atomic к данному типу / false - нет, лучше использовать mutex (например, нетривиальный тип и больше > 8 байт - размер регистра)
        {
            class A { int x, y; }; //
            class B { int x, y, z; };
            class C { int x, y; char z; };
            class D { char a, b, c, d, e, f, j, h; };
            class E { char a, b, c, d, e, f, j, h, i; };
            class F { char mas[1]; };
            class J { char mas[3]; };
            class H { char mas[8]; };
            class I { char mas[9]; };
            
            [[maybe_unused]] auto A_size = sizeof(A); // 8 байтов
            [[maybe_unused]] auto B_size = sizeof(B); // 12 байтов
            [[maybe_unused]] auto C_size = sizeof(C); // 12 байтов из-за выравнивания (aligment)
            [[maybe_unused]] auto D_size = sizeof(D); // 8 байтов
            [[maybe_unused]] auto E_size = sizeof(E); // 9 байтов
            [[maybe_unused]] auto F_size = sizeof(F); // 1 байт
            [[maybe_unused]] auto J_size = sizeof(J); // 3 байта
            [[maybe_unused]] auto H_size = sizeof(H); // 8 байтов
            [[maybe_unused]] auto I_size = sizeof(I); // 9 байтов
            
            std::cout << std::boolalpha
                      << "std::atomic<int> is lock free? "
                      << std::atomic<int>{}.is_lock_free() << std::endl // true, 4 <= 8
                      << "std::atomic<A> is lock free? "
                      << std::atomic<A>{}.is_lock_free() << std::endl // true, 8 <= 8
                      << "std::atomic<B> is lock free? "
                      << std::atomic<B>{}.is_lock_free() << std::endl // false, 12 > 8
                      << "std::atomic<C> is lock free? "
                      << std::atomic<C>{}.is_lock_free() << std::endl // false, 12 > 8
                      << "std::atomic<D> is lock free? "
                      << std::atomic<D>{}.is_lock_free() << std::endl // true, 8 <= 8
                      << "std::atomic<E> is lock free? "
                      << std::atomic<E>{}.is_lock_free() << std::endl // false, 9 > 8
                      << "std::atomic<F> is lock free? "
                      << std::atomic<F>{}.is_lock_free() << std::endl // false, 1 <= 8
                      << "std::atomic<J> is lock free? "
                      << std::atomic<J>{}.is_lock_free() << std::endl // false, 3 <= 8 т.к. в процессоре есть только 2^n разрядные регистры
                      << "std::atomic<H> is lock free? "
                      << std::atomic<H>{}.is_lock_free() << std::endl // false, 8 <= 8
                      << "std::atomic<I> is lock free? "
                      << std::atomic<I>{}.is_lock_free() << std::endl; // false, 9 > 8
        }
        // exchange - замена значения
        {
            std::atomic<bool> flag = false;
            [[maybe_unused]] bool exchange1 = flag.exchange(true); // false
            [[maybe_unused]] bool exchange2 = flag.exchange(false); // true
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
                [[maybe_unused]] bool exchanged1 = flag.compare_exchange_strong(expected, true); // flag == true, expected == true, exchanged == true
                
                expected = false;
                [[maybe_unused]] bool exchanged2 = flag.compare_exchange_strong(expected, true); // flag == true, expected == true, exchanged == false
                [[maybe_unused]] bool exchanged3 = flag.compare_exchange_strong(expected, false); // flag == false, expected == true, exchanged == true
                [[maybe_unused]] bool exchanged4 = flag.compare_exchange_strong(expected, false); // flag == false, expected == false, exchanged == false
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
            /*
             volatile (аналог memory_order_seq_cst в memory_order atomic) - запрещает оптимизировать ту часть кода, которая взаимодействует с этой переменной, т.е. компилятор не может сразу же подставить в переменную значение или переставить на строку выше или ниже. По-умолчанию компилятор предполагает, что работа идет в одном потоке и код будет выполнен последовательно, но компилятор может переупорядочить команды программы с целью оптимизации, поэтому volatile помогает избежать ошибок при многопоточности, когда разные потоки делят общие данные. Однако, volatile плохо работает в многопоточности, но с типом bool его можно применять безопасно (статья Андрея Александреску). volatile не имеет ничего общего с atomic.
             Пример, один поток записывает переменную, другой поток читает эту переменную:
             компилятор может переставить местами result = GetResult() и flag = true в первом потоке, а во втором потоке выведется пустое значение при flag == true:
             */
            {
                std::cout << "volatile" << std::endl;
                std::string result;
                auto GetResult = []() noexcept -> std::string
                {
                    return "Result";
                };
                
                volatile bool flag = false;
                auto Function1 = [&]() // 1 поток
                {
                    result = GetResult(); // Получение результата в какой-то фукнции
                    // LOAD (no) ↑ STORE (no)
                    flag = true;
                    // LOAD (no) ↓ STORE (no)
                };
                
                auto Function2 = [&]() // 2 поток
                {
                    // LOAD (no) ↑ STORE (no)
                    if (flag)
                    // LOAD (no) ↓ STORE (no)
                        std::cout << result << std::endl;
                };
                
                std::thread thread1(Function1);
                std::this_thread::sleep_for(std::chrono::milliseconds(10)); // Чтобы первый поток успел записать результат
                std::thread thread2(Function2);
                
                thread1.join();
                thread2.join();
            }
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
                            // LOAD (yes) ↑ STORE (yes)
                            count.fetch_add(1, std::memory_order_relaxed); // запись
                            // LOAD (yes) ↓ STORE (yes)
                        };
                        
                        auto Read = [&]()
                        {
                            // LOAD (yes) ↑ STORE (yes)
                            std::cout << "Read count = " << count.load(std::memory_order_relaxed) << std::endl;
                            // LOAD (yes) ↓ STORE (yes)
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
            std::atomic_flag flag = ATOMIC_FLAG_INIT;
            [[maybe_unused]] auto test1 = flag.test(); // false
            [[maybe_unused]] auto test2 = flag.test_and_set(); // test == false, flag == true
            [[maybe_unused]] auto test3 = flag.test(); // true
            flag.clear(); // false
            [[maybe_unused]] auto test4 = flag.test(); // false
        }
        
        atomic_flag::Spinlock spinlock;
        std::thread thread1([&] {PrintSymbol('+', spinlock);});
        std::thread thread2([&] {PrintSymbol('-', spinlock);});
        
        thread1.join();
        thread2.join();
        
        std::cout << std::endl;
    }
}
