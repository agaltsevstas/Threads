# Многопоточность
Возможности многопоточности

## std::thread
std::thread - это RAII обертка вокруг операционной системы, POSIX - API Linuxa, WindowsRest - API Windows <br>
Методы:
- detach - разрывает связь между объектом thread и потоком, который начал выполняться. detach - не блокирует основной поток, не заставляя дожидаться окончания выполнения потока.
- join - блокирует основной поток, заставляя дожидаться окончания выполнения потока.
- joinble - проверяет ассоциирован std::thread с потоком, если нет (не было detach или join) - возвращает true.

Замечание: нельзя копировать.

## std::jthread с C++20
std::jthread - по сравнению с std::thread преимущества:
- в деструкторе вызывается join.
- безопасный и предотвращает утечку ресурсов: исключение будет перехвачено и обработано.

## std::exception
Исключения (std::exception) в разных потоках (std::thread) - не пересекаются. Чтобы пробросить исключение в главный поток можно использовать:
- ```std::current_exception``` - получение текущего exception.
- ```std::exception_ptr``` - обертка для исключения (std::exception), из нее ничего нельзя получить, только пробросить дальше с помощью std::rethrow_exception.
- ```std::rethrow_exception``` - пробрасывает исключение в другой try/catch.
- ```std::stack<std::exception_ptr>``` - сохраняет исключение из другого потока.
```
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
```

### Безопасность исключений (exception safety)
При захвате std::mutex при std::exception: лучше использовать RAII оберкти std::lock_guard/std::unique_lock, которые при выходе из стека в деструкторе вызывается unlock, вместо обычного std::mutex, который при возникновении исключения не вызовет unlock.
```
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
```

## Cпособы управления текущим потоком
- std::call_once - вызов объекта ровно один раз, даже если он вызывается одновременно из нескольких потоков.
- std::this_thread::get_id() - id текущего потока.
- std::thread::hardware_concurrency() - кол-во возможных потоков на компьютере.
- std::this_thread::yield() - приостановливает текущий поток, отдав преимущество другим потокам.
- std::this_thread::sleep_for(sleep_duration) - блокирует выполнение текущего потока на время sleep_duration.
- std::this_thread::sleep_until(sleep_time) - блокирует выполнение текущего потока до наступления момента времени, например 11:15:00.

## Синхронизация
Проблема: Race condition/data race (состояние гонки) - обращение к общим данным в разных потоках одновременно. <br>
Решение: использование std::mutex.

### Виды mutex:

#### std::mutex
Механизм синхронизации и потокобезопасности (thread safety), который предназначен для контроля доступа к общим данным для нескольких потоков с использованием барьеров памяти. <br>
Методы, внутри используются синхронизации уровня операционной системы (ОС):
- lock - происходит захват mutex текущим потоком и блокирует доступ к данным другим потокам; или поток блокируется, если мьютекс уже захвачен другим потоком.
- unlock - освобождение mutex, разблокирует данные другим потокам. <br>
Замечание: при повторном вызове lock - вылезет std::exception или приведет к состоянию бесконечного ожидания, при повторном вызове unlock - ничего не будет или вылезет std::exception.
- try_lock - происходит захват mutex текущим потоком, НО НЕ блокирует доступ к данным другим потокам, а возвращает значение: true - можно захватить mutex / false - нельзя; НО МОЖЕТ возвращать ложное значение, потому что в момент вызова try_lock mutex может быть уже lock/unlock. Использовать try_lock в редких случаях, когда поток мог что-то сделать полезное, пока ожидает unlock.

#### std::recursive_mutex
Использовать при рекурсии. Правило: кол-во lock = кол-во unlock. Если использовать обычный std::mutex, то при повторном вызове lock, либо будет deadlock, либо выскачит ошибка.

#### std::timed_mutex
По сравнению с std::mutex имеет дополнительные методы:
- try_lock_for - блокирует доступ к данным другим потокам на ОПРЕДЕЛЕННОЕ ВРЕМЯ и возвращает true - произошел захват mutex текущим потоком/false - нет; НО МОЖЕТ возвращать ложное значение, потому что в момент вызова try_lock_for std::timed_mutex может быть уже lock/unlock.
- try_lock_until - блокирует выполнение текущего потока до НАСТУПЛЕНИЕ МОМЕНТА ВРЕМЕНИ (например, 11:15:00) и возвращает true - произошел захват mutex текущим потоком/false - нет; НО МОЖЕТ возвращать ложное значение, потому что в момент вызова try_lock_until std::timed_mutex может быть уже lock/unlock.

#### std::recursive_timed_mutex
Обладает свойствами std::recursive_mutex + std::timed_mutex.

#### std::shared_mutex
Имеет 2 разных блокировки:
- эксклюзивная блокировка (exclusive lock) - запись данных только для одного потока одновременно. Если один поток получил эксклюзивный доступ (через lock, try_lock), то никакие другие потоки не могут получить блокировку (включая общую).
  Работает по аналогии std::mutex. Методы:
  - lock
  - unlock
  - try_lock
- общая блокировка (shared lock) - чтение данных из нескольких потоков одновременно. Если один поток получил общую блокировку (через lock_shared, try_lock_shared), ни один другой поток не может получить эксклюзивную блокировку, но может получить общую блокировку. <br>
  Методы:
  - shared_lock
  - unlock_shared
  - try_lock_shared
Замечание: в пределах одного потока одновременно может быть получена только одна блокировка (общая или эксклюзивная).

Плюсы:
- можно выделить участок кода, где будет происходить чтение из нескольких потоков.
- синхронизирует данные при записи/чтении, не вызывая Race condition/data race (состояние гонки). При записи данных одним потоком, чтение/запись данных блокируется для всех потоков. При чтении данных одним потоком, чтение данных - доступно для всех потоков, запись данных блокируется для всех потоков.
- параллелизм чтения и общая блокировка для чтения, которые недоступны при обычном std::mutex.
- отсутствие Livelock (голодание потоков) - ситуация, в которой поток не может получить доступ к общим ресурсам, потому что на эти ресурсы всегда претендуют какие-то другие потоки. <br>
Минусы:
- больше весит, чем std::mutex.

#### std::shared_timed_mutex
Обладает свойствами std::shared_mutex + std::timed_mutex.

## Lock management
RAII обертки, захват mutex.lock() ресурса происходит на стеке в конструкторе и высвобождение unlock при выходе из стека в деструкторе.

### Виды lock:

#### std::lock_guard
Делает захват mutex.lock() ресурса происходит на стеке в конструкторе и высвобождение unlock при выходе из стека в деструкторе. <br>
Дополнительные тэги:
- std::adopt_lock - считается lock сделан до этого.

Замечание: нельзя копировать.

#### std::unique_lock
Имеет возможности std::lock_guard + std::mutex. Приоритетнее использовать std::unique_lock, чем std::lock_guard. <br>
Методы:
- owns_lock - проверка на блокировку, возвращает значение: true - mutex заблокирован / false - нет.

Дополнительные тэги:
- std::try_to_lock - происходит попытка захвата mutex текущим потоком, НО НЕ блокирует доступ к данным другим потокам.
- std::defer_lock - lock не выполнется.
- std::adopt_lock - считается lock сделан до этого.

Замечание: можно копировать.

#### std::shared_lock
Имеет возможности std::unique_lock, но для std::shared_timed_mutex. Для обычного std::mutex не подойдет. <br>
Делает общую блокировку (shared lock) - чтение данных из нескольких потоков одновременно. Если один поток получил общую блокировку, ни один другой поток не может получить эксклюзивную блокировку (exclusive lock) - запись данных только для одного потока одновременно, но может получить общую блокировку.

## Виды блокировок
1. Livelock - голодание потоков.
2. Starvation - нехватка ресурсов для потока.
3. Deadlock - взаимная блокировка mutex.

Отличия: <br>
В Livelock потоки находятся в состоянии ожидания и выполняются одновременно. Livelock — это частный случай Starvation - процесс в потоке не прогрессирует.
В Deadlock потоки находятся в состоянии ожидания.

### Livelock
Голодание потоков - ситуация, в которой поток не может получить доступ к общим ресурсам, потому что на эти ресурсы всегда претендуют какие-то другие потоки, которым отдаётся предпочтение. Потоки не блокируются — они выполняют не столь полезные действия.
Решение: Расставлять приоритеты потоков и правильно использовать mutex.

### Starvation
Голодание - поток не может получить все ресурсы, необходимые для выполнения его работы.

### Deadlock
Ситуация, в которой есть 2 mutex и 2 thread и 2 функции. В function1 идет mutex1.lock(), mutex2.lock(). В функции function2 идет обратная очередность mutex2.lock(), mutex1.lock(). Получается thread1 захватывает mutex1, thread2 захватывает mutex2 и возникает взаимная блокировка:
```
std::mutex mutex1;
std::mutex mutex2;
auto function1 = [&]()
{
    std::lock_guard guard1(mutex1);
    std::this_thread::sleep_for(std::chrono::milliseconds(10)); // задержка, чтобы thread2 успел сделать lock в mutex2 в function2
    std::lock_guard guard2(mutex2);
};
auto function2 = [&]()
{
    std::lock_guard guard1(mutex2);
    std::this_thread::sleep_for(std::chrono::milliseconds(10)); // задержка, чтобы thread1 успел сделать lock в mutex1
    std::lock_guard guard2(mutex1);
};
std::thread thread1(function1);
std::thread thread2(function2);                    
thread1.join();
thread2.join();
```

#### Решение До С++17
1. Захватывать (lock) несколько мьютексов всегда в одинаковом порядке.
2. Отпускать (unlock) захваченные (lock) mutex в порядке LIFO («последним пришёл — первым ушёл»).
3. Можно использовать алгоритм предотвращения взаимоблокировок std::lock, порядок std::mutexов неважен: <br>
  3.1:
  ```
  std::lock(mutex1, mutex2);
  std::lock_guard lock1(mutex1, std::adopt_lock); // считается lock сделан до этого
  std::lock_guard lock2(mutex2, std::adopt_lock); // считается lock сделан до этого
  ```
  
  3.2:
  ```
  std::unique_lock lock1(mutex1, std::defer_lock); // lock не выполнется
  std::unique_lock lock2(mutex2, std::defer_lock); // lock не выполнется
  std::lock(lock1, lock2);
  ```
4. std::try_lock - пытается захватить (lock) в очередном порядке каждый std::mutex.
При успешном захвате всех переданных std::mutex возвращает -1.
При неуспешном захвате всех std::mutexoв, все std::mutexы освобождаются и возвращается индекс (0,1,2..) первого std::mutex, которого не удалось заблокировать. <br>
Замечание: при повторном захвате все std::mutex вызовется unlock. <br>
При исключении в std::try_lock вызывается unlock для всех std::mutex. <br>
Порядок std::mutexов неважен:
```
if (std::try_lock(mutex1, mutex2) == -1)
{
   std::lock_guard lock1(mutex1, std::adopt_lock); // считается lock сделан до этого
   std::lock_guard lock2(mutex2, std::adopt_lock); // считается lock сделан до этого
}
```

#### Решение C++17
std::scoped_lock - улучшенная версия std::lock_guard, конструктор которого делает захват (lock) произвольного кол-во мьютексов в очередном порядке и высвобождает (unlock) при выходе из стека в деструкторе, использование идиомы RAII. <br>
Замечание: отсутствует конструктор копироввания.
```
std::scoped_lock scoped_lock(mutex1, mutex2);
```

## Параллелизм

### Параллельные алгоритмы STL с C++17
- использовать при n > 10000 при ОЧЕНЬ ПРОСТЫХ операциях. Чем сложнее операции, тем быстрее выполняется параллельность.
- OpenMP все равно быстрее, поэтому лучше его использовать. Но TBB быстрее OpenMP.

Примеры:
- std::execution::seq (обычная сортировка)
```
{
    std::vector<int> numbers = { 3, 2, 4, 5, 1 };
    std::ranges::sort(std::execution::seq, numbers); // в xcode не работает
}
```
- std::execution::par (параллельная сортировка)
```
{
    std::vector<int> numbers = { 3, 2, 4, 5, 1 };
    std::ranges::sort(std::execution::par, numbers); // в xcode не работает
}
```
- std::execution::unseq (parallel + vectorized (SIMD))
```
{
    // TODO
}
```
- std::execution::par_unseq (vectorized, C++20)
```
{
    // TODO
}
```

### OpenMP
Open Multi-Processing — это библиотека, используемая для многопоточности на уровне цикла.
Использование параллельной версии STL (все алгоритмы внутри поддерживают OpenMP и будут выполняться параллельно), для этого нужно передать libstdc++ parallel в компилятор GCC.
До цикла for будет определено кол-во ядер в системе. Код внутри for будет выделен в отдельную функцию (НЕ сам For!!! try catch бессмысленен - нельзя отловить исключение в отдельном потоке, break, continue, return - невозможны). В конце области видимости для каждого потока будет вызван join(). Например, для 10 ядерной системы будет запущено 10 потоков. При 10.000 итераций. Каждый поток обработает 1.000 / 10 = 1.000 элементов в контейнере.

Не подходит:
- для рекурсии (может кончиться стек). Есть исключение с использованием очереди задач #pragma omp task. При условии, что размер очереди < 255. Можно на определенном уровне стека запоминать состояния (значения переменных) и кидать в очередь. После окончания функции в рамках того же потока или другого продолжаем вызывать ту же функцию с такой же логикой. Таким развязываем рекурсию по потокам через очередь.
- не всегда обеспечивает хорошую производительность

Подробнее: [OpenMP](https://learn.microsoft.com/ru-ru/cpp/parallel/openmp/2-directives?view=msvc-170)

### TBB 
Threading Building Blocks - библиотека Intel, является высокоуровневой библиотекой, чем OpenMP. В TBB есть планировщих задач, который помогает лучше оптимизировать работу.
Это достигается с помощью алгоритма work stealing, который реализует динамическую балансировку нагрузки. Есть функция разной сложности, какой-то поток очень быстро обработал свою очередь задач, то он возьмет часть свободных задач другого потока. В TBB самому создать поток нельзя, поэтому в каких потоках идет выполнение знать не нужно. Можно только создать задачу и отдать ее на исполнение планировщику.

Подробнее: [TBB](https://oneapi-src.github.io/oneTBB/)

## Асинхронность
TODO

# Лекции:
[Лекция 5. Multithreading in C++ (потоки, блокировки, задачи, атомарные операции, очереди сообщений)](https://www.youtube.com/watch?v=z6M5YCWm4Go&ab_channel=ComputerScience%D0%BA%D0%BB%D1%83%D0%B1%D0%BF%D1%80%D0%B8%D0%9D%D0%93%D0%A3) <br/>
[Лекция 9. OpenMP и Intel TBB](https://www.youtube.com/watch?v=_MKbLk6K_Tk&t=2627s&ab_channel=ComputerScienceCenter) <br/>
[Модель памяти C++ - Андрей Янковский](https://www.youtube.com/watch?v=SIZmLPtcZiE&ab_channel=YandexforDevelopers) <br/>

# Сайты: 
[Multithreading](https://habr.com/ru/companies/otus/articles/549814/) <br/>
[shared_mutex](https://en.cppreference.com/w/cpp/thread/shared_mutex) <br/>
[difference between std::mutex and std::shared_mutex](https://stackoverflow.com/questions/46452973/difference-between-stdmutex-and-stdshared-mutex) <br/>
[Does std::shared_mutex favor writers over readers?](https://stackoverflow.com/questions/57706952/does-stdshared-mutex-favor-writers-over-readers) <br/>
[why std::lock() supports deallock avoidence but std::try_lock() does not?](https://stackoverflow.com/questions/50286056/why-stdlock-supports-deallock-avoidence-but-stdtry-lock-does-not) <br/>
[std::atomic. Модель памяти C++ в примерах](https://habr.com/ru/articles/517918/) <br/>
[std::conditional_variable и std::atomic_flag в С++20](https://habr.com/ru/articles/708918/) <br/>
[std::condition_variable::notify_all() - I need an example](https://stackoverflow.com/questions/43759609/stdcondition-variablenotify-all-i-need-an-example) <br/>
[How std::atomic wait operation works?](https://stackoverflow.com/questions/70812376/how-stdatomic-wait-operation-works) <br/>
[Spurious wakeup with atomics and condition_variables](https://stackoverflow.com/questions/72194964/spurious-wakeup-with-atomics-and-condition-variables) <br/>
