#include "Coroutine.hpp"

#include <coroutine>
#include <iostream>
#include <optional>
#include <thread>

/*
 Лекции: https://www.youtube.com/watch?v=seDJT66BJJo&ab_channel=C%2B%2BUserGroup
         https://www.youtube.com/watch?v=ITLe4FIrrTg&t=442s
         https://en.cppreference.com/w/cpp/language/coroutines
         https://ru.stackoverflow.com/questions/496002/%D0%A1%D0%BE%D0%BF%D1%80%D0%BE%D0%B3%D1%80%D0%B0%D0%BC%D0%BC%D1%8B-%D0%BA%D0%BE%D1%80%D1%83%D1%82%D0%B8%D0%BD%D1%8B-coroutine-%D1%87%D1%82%D0%BE-%D1%8D%D1%82%D0%BE?ysclid=lxx121p70162700412
 */

/*
 Сайты: https://habr.com/ru/articles/519464/
        https://habr.com/ru/companies/yandex_praktikum/articles/559642/
 */

/*
 Корутина/корутины (std::coroutine) - функция с несколькими точками входа и выхода, из нее можно выйти середине, а затем вернуться в нее и продолжить исполнение. По сути это объект, который может останавливаться и возобновляться. Является более простым аналогом future.then, где then осуществляет запуск цепочки выполнения в будущем последовательных асинхронных операций для вычисления промежуточных результатов.
 Пример — программы, выполняющие много операций ввода-вывода. Пример, веб-сервер, который пока данные не будут переданы по сети или получены, он ждёт. Если реализовать веб-сервер обычным способом, то на каждого клиента будет отдельный поток. В нагруженных серверах это будет означать тысячи потоков. Эти потоки по большей части приостанавливаются и ждут, нагружая операционную систему переключением контекстов. При использовании корутины поток приостанавливает выполнение задачи, сохранив текущее состояние, и начинает выполнять другие задачи, а затем может вернуться в предыдущую задачу продолжить ее исполнение.
 Характериситки:
 - stackfull - держат свой стек в памяти на протяжении всего времени жизни корутины. В STL стандартах 20/23 корутины не работают.
 - stackless - не сохраняет свой стек в памяти на протяжении всего времени жизни корутины, а только во время непосредственной работы. При этом стек аллоцируется в вызывающем корутину контексте.
 C++20 stl: stackless, userserver (yandex): stackfull.
 Методы:
 - co_await — для прерывания функции и последующего продолжения.
 - co_yield — для прерывания функции с одновременным возвратом результата. Это синтаксический сахар для конструкции с co_await.
 - co_return — для завершения работы функции.
 */


namespace coroutine
{
    namespace CO_RETURN
    {
        struct promise;
         
        struct coroutine : std::coroutine_handle<promise>
        {
            using promise_type = ::coroutine::CO_RETURN::promise;
        };
         
        struct promise
        {
            coroutine get_return_object() { return {coroutine::from_promise(*this)}; }
            std::suspend_always initial_suspend() noexcept { return {}; }
            std::suspend_always final_suspend() noexcept { return {}; }
            void return_void() {}
            void unhandled_exception() {}
        };
         
        struct S
        {
            int i;
            coroutine f()
            {
                std::cout << i;
                co_return;
            }
        };
    }

    namespace CO_YIELD
    {
        template<std::movable T>
        class Generator
        {
        public:
            struct promise_type
            {
                Generator<T> get_return_object()
                {
                    return Generator{Handle::from_promise(*this)};
                }
                static std::suspend_always initial_suspend() noexcept
                {
                    return {};
                }
                static std::suspend_always final_suspend() noexcept
                {
                    return {};
                }
                std::suspend_always yield_value(T value) noexcept
                {
                    current_value = std::move(value);
                    return {};
                }
                // Disallow co_await in generator coroutines.
                void await_transform() = delete;
                [[noreturn]]
                static void unhandled_exception() { throw; }
         
                std::optional<T> current_value;
            };
         
            using Handle = std::coroutine_handle<promise_type>;
         
            explicit Generator(const Handle coroutine) :
                m_coroutine{coroutine}
            {}
         
            Generator() = default;
            ~Generator()
            {
                if (m_coroutine)
                    m_coroutine.destroy();
            }
         
            Generator(const Generator&) = delete;
            Generator& operator=(const Generator&) = delete;
         
            Generator(Generator&& other) noexcept :
                m_coroutine{other.m_coroutine}
            {
                other.m_coroutine = {};
            }
            Generator& operator=(Generator&& other) noexcept
            {
                if (this != &other)
                {
                    if (m_coroutine)
                        m_coroutine.destroy();
                    m_coroutine = other.m_coroutine;
                    other.m_coroutine = {};
                }
                return *this;
            }
         
            // Range-based for loop support.
            class Iter
            {
            public:
                void operator++()
                {
                    m_coroutine.resume();
                }
                const T& operator*() const
                {
                    return *m_coroutine.promise().current_value;
                }
                bool operator==(std::default_sentinel_t) const
                {
                    return !m_coroutine || m_coroutine.done();
                }
         
                explicit Iter(const Handle coroutine) :
                    m_coroutine{coroutine}
                {}
         
            private:
                Handle m_coroutine;
            };
         
            Iter begin()
            {
                if (m_coroutine)
                    m_coroutine.resume();
                return Iter{m_coroutine};
            }
         
            std::default_sentinel_t end() { return {}; }
         
        private:
            Handle m_coroutine;
        };
         
        template<std::integral T>
        Generator<T> range(T first, const T last)
        {
            while (first < last)
                co_yield first++;
        }
    }

    namespace CO_AWAIT
    {
        auto switch_to_new_thread(std::thread& out)
        {
            struct awaitable
            {
                std::thread* p_out;
                bool await_ready() { return false; }
                void await_suspend(std::coroutine_handle<> h)
                {
                    std::thread& out = *p_out;
                    if (out.joinable())
                        throw std::runtime_error("Output jthread parameter not empty");
                    out = std::thread([h] { h.resume(); });
                    // Potential undefined behavior: accessing potentially destroyed *this
                    // std::cout << "New thread ID: " << p_out->get_id() << '\n';
                    std::cout << "New thread ID: " << out.get_id() << '\n'; // this is OK
                }
                void await_resume() {}
            };
            return awaitable{&out};
        }
         
        struct task
        {
            struct promise_type
            {
                task get_return_object() { return {}; }
                std::suspend_never initial_suspend() { return {}; }
                std::suspend_never final_suspend() noexcept { return {}; }
                void return_void() {}
                void unhandled_exception() {}
            };
        };

        task resuming_on_new_thread(std::thread& out)
        {
            std::cout << "Coroutine started on thread: " << std::this_thread::get_id() << '\n';
            co_await switch_to_new_thread(out);
            // awaiter destroyed here
            std::cout << "Coroutine resumed on thread: " << std::this_thread::get_id() << '\n';
        }
    }


    void start()
    {
        // co_yield
        {
#if defined (__APPLE__) || defined(__APPLE_CC__) || defined(__OSX__)
            using namespace CO_YIELD;
            std::cout << "co_yield" << std::endl;
            
            for (const char i : range(65, 91))
                    std::cout << i << ' ';
                std::cout << '\n';
#endif
        }
        // co_wait
        {
            using namespace CO_AWAIT;
            std::cout << "co_wait" << std::endl;
            
            std::thread out;
            resuming_on_new_thread(out);
            out.join();
        }
        // co_return
        {
            using namespace CO_RETURN;
            std::cout << "co_return" << std::endl;
            
            coroutine h = [i = 0]() -> coroutine // a lambda that's also a coroutine
            {
                std::cout << i;
                co_return;
            }(); // immediately invoked
            // lambda destroyed
            h.resume(); // uses (anonymous lambda type)::i after free
            h.destroy();
        }
    }
}
