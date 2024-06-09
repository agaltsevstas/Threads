#include "Latch_Barrier.hpp"

#include <barrier>
#include <iostream>
#include <functional>
#include <latch>
#include <ranges>
#include <thread>
#include <vector>

/*
 Сайты: https://www.geeksforgeeks.org/cpp-20-latch-header/?ysclid=lx69ub3m43171648255
        https://www.geeksforgeeks.org/std-barrier-in-cpp-20/?ysclid=lx6fhggljs67772429
 */

/*
 Защелка (std::latch) - механизм синхронизации работы потоков, который может управлять доступом к общему ресурсу. В основе лежит уменьшающийся счетчик, значение счетчика инициализируется при создании. Защелка блокирует текущие потоки до тех пор, пока другие потоки не уменьшат значение счётчика до нуля. Нет возможности увеличить или сбросить счетчик, что делает защелку одноразовым барьером.
 Методы:
 - count_down - атомарно уменьшает значение счётчика на определенное значение (по умолчанию 1) без блокировки потока. Когда счетчик достигает нуля, все потоки, ожидающие фиксации, разблокируются. Если значение счётчика становится отрицательным, то поведение неопределено.
 - wait - блокирует текущий поток до тех пор, пока счётчик не достигнет нулевого значения. Если значение счётчика уже равно 0, то управление возвращается немедленно.
 - try_wait - проверяет, достигло ли значение счетчика нуля. Возвращает значение: true - счетчик равен нулю / false - нет, НО МОЖЕТ возвращать ложное значение, потому что в момент вызова try_wait защелка может быть уже count_down.
 - arrive_and_wait - атомарно уменьшает значение счётчика на определенное значение (по умолчанию 1) и блокирует текущий поток до тех пор, пока счётчик не достигнет нулевого значения. Если значение счётчика становится отрицательным, то поведение неопределено.
 - max - возвращает максимально возможное значение счетчика. В конструкторе инициализируется счетчик - текущее допустимое кол-во потоков.
 
 Барьер (std::barrier) - механизм синхронизации работы потоков, который может управлять доступом к общему ресурсу. В основе лежит уменьшающийся счетчик, значение счетчика инициализируется при создании. Барьер блокирует потоки до тех пор, пока все потоки не уменьшат значение счётчика до нуля, как только ожидающие потоки разблокируются, значение счётчика устанавливается в начальное состояние и барьер может быть использован повторно.
 Методы:
 - arrive - атомарно уменьшает значение счётчика на определенное значение (по умолчанию 1). Если значение счётчика становится отрицательным, то поведение не определено. Метод возвращает идентификатор фазы, который имеет тип std::arrival_token.
 - wait - блокирует текущий поток до тех пор, пока указанная фаза std::arrival_token не завершится. Принимает идентификатор фазы в качестве аргумента.
 - arrive_and_wait - атомарно уменьшает значение счётчика на 1 и блокирует текущий поток до тех пор, пока счётчик не обнулится. Внутри функции вызов wait(arrive()). Если значение счётчика становится отрицательным, то поведение неопределено.
 - arrive_and_drop - атомарно уменьшает на 1 значение счётчика и счётчика для следующих фаз. Если значение счётчика становится отрицательным, то поведение неопределено.
 - max - возвращает максимально возможное значение счетчика. В конструкторе инициализируется счетчик - текущее допустимое кол-во потоков.
 
 Отличия std::latch от std::barrier:
 - std::latch может быть уменьшен одним потоком более одного раза.
 - std::latch - можно использовать один раз, std::barrier является многоразовым: как только ожидающие потоки разблокируются, значение счётчика устанавливается в начальное состояние и барьер может быть использован повторно.
 */

namespace Latch_Barrier
{
    void Start()
{
        // latch
        {
            std::cout << "latch" << std::endl;
            
            /// 1 Пример
            {
                std::cout << "1 Пример" << std::endl;
                auto Worker = [&](std::latch& latch, int indexThread)
                {
                    std::cout << "Индекс потока: " << indexThread << std::endl;
                    std::this_thread::sleep_for(std::chrono::milliseconds(50));
                    latch.count_down(); // уменьшает кол-во счетчика
                };
                
                std::latch latch(3); // макс кол-во потоков = 3
                [[maybe_unused]] auto max_count = latch.max();
                
                std::vector<std::thread> threads(3);
                for (const auto i : std::views::iota(0, 3))
                    threads[i] = std::thread(Worker, std::ref(latch), i);
                
                for (auto& thread : threads)
                {
                    if (thread.joinable())
                        thread.join();
                }
                
                std::cout << "Основной поток заблокирован" << std::endl;
                latch.wait(); // Если значение счетчика = 0, то разблокируем основной поток
                std::cout << "Основной поток разблокирован: значение счетчика = 0" << std::endl;
                std::cout << std::endl;
            }
            /// 2 Пример
            {
                std::cout << "2 Пример" << std::endl;
                std::string data;
                constexpr int size = 3;
                data.resize(size, '0');
                
                std::latch latch(size); // макс кол-во потоков = 3
                auto SetSymbol = [&](const char& iSymbol, int indexThread)
                {
                    data[indexThread] = iSymbol;
                    latch.count_down();
                    std::cout << "Индекс потока: " << indexThread << std::endl;
                };
                
                auto PrintSymbol = [&]()
                {
                    latch.wait();
                    std::cout << "Данные:" << data << std::endl;
                };
                
                std::thread threads[size];
                int i = 0;
                for (char c = 'a'; c < 'd'; ++c)
                {
                    threads[i] = std::thread(SetSymbol, c, i);
                    ++i;
                }
                
                for (auto& thread : threads)
                    thread.join();
                
                std::thread(PrintSymbol).join();
                std::cout << std::endl;
            }
        }
        /// barrier
        {
            std::cout << "barrier" << std::endl;
            
            /// 1 Пример: ожидаем завершение всех потоков
            {
                constexpr int size = 3;
                std::barrier barrier(3);
                
                std::cout << "1 Пример: ожидаем завершение всех потоков" << std::endl;
                auto Worker = [&](int indexThread)
                {
                    std::cout << "Индекс потока: " << indexThread << std::endl;
                    std::this_thread::sleep_for(std::chrono::milliseconds(50));
                    barrier.arrive_and_wait(); // уменьшает кол-во счетчика и блокирует текущий поток
                };
                
                std::vector<std::thread> threads(size);
                for (const auto i : std::views::iota(0, size))
                    threads[i] = std::thread(Worker, i);
                
                for (auto& thread : threads)
                {
                    if (thread.joinable())
                        thread.join();
                }
                std::cout << std::endl;
            }
            /// 2 Пример: вызов дополнительной функции при разблокировке потоков
            {
                constexpr int size = 3;
                
                int count = 1;
                auto Unlock = [&]() noexcept
                {
                    std::cout << "Потоки разблокирован: значение счетчика = " << count++ << std::endl;
                };
                
                std::barrier barrier(size, Unlock);
                
                std::cout << " 3 Пример: вызов дополнительной функции при разблокировке потоков" << std::endl;
                auto Worker = [&](int indexThread)
                {
                    std::cout << "1 Этап, Индекс потока: " << indexThread << std::endl;
                    barrier.arrive_and_wait(); // уменьшает кол-во счетчика и блокирует текущий поток
                    
                    std::this_thread::sleep_for(std::chrono::milliseconds(50));
                    std::cout << "2 Этап, Индекс потока: " << indexThread << std::endl;
                    barrier.arrive_and_wait(); // уменьшает кол-во счетчика и блокирует текущий поток
                };
                
                std::vector<std::thread> threads(size);
                for (const auto i : std::views::iota(0, size))
                    threads[i] = std::thread(Worker, i);
                
                for (auto& thread : threads)
                {
                    if (thread.joinable())
                        thread.join();
                }
                std::cout << std::endl;
            }
            /// 3 Пример: arrive
            {
                /// TODO
            }
            /// 4 Пример: wait
            {
                /// TODO
            }
            /// 5 Пример: arrive_and_drop
            {
                /// TODO
            }
        }
        
        std::cout << std::endl;
    }
}
