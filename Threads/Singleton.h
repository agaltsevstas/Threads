#ifndef Singleton_h
#define Singleton_h

#include <mutex>
#include <iostream>
#include <type_traits>

/*
 Singleton до C++11
 Double checked-locking (anti-pattern)
 Проблема: 1 поток выделил память для объекта _instance1 но его еще не создал, 2 поток пытается взять объект _instance1.
 Решение: сделать _instance2 - atomic.
 */
namespace antipattern
{
    struct Singleton
    {
#if 0
    /*
     Непотокобезопасный Singleton
     Проблема: 1 поток выделил память для объекта но его еще не создал, 2 поток пытается взять объект
     Решение: сделать _instance atomic
     */
    static Singleton* Instance()
    {
        // mutex - слишком затратный, поэтому делаем 2 проверку (double checked-locking)
        if (!_instance1) // Операция не атомарна: чтение + сравнение.
        {
            std::lock_guard lock(_mutex); // Делаем thread safety, чтобы не было data race
            if (!_instance1)
            {
                _instance1 = new Singleton();  // Операция не атомарна: создание объекта (выделить память + вызвать конструктор) + присваивания указателя
                {
                    /*
                     // Работает как malloc - выделяем память
                     _instance2 = (Singleton*)aligned_alloc(alignof(Singleton), sizeof(Singleton)); // 1 поток выделил память, но не создал объект
                     // Создаем объект
                     _instance2 = new (_instance2) Singleton;
                     */
                }
            }
        }
        
        return _instance1; // 2 поток берет объект, пока 1 поток выделил под него память, но еще его не создал
    }
#endif
        // Потокобезопасный Singleton: std::atomic
        static Singleton* Instance()
        {
            // mutex - слишком затратный, поэтому делаем 2 проверку (double checked-locking)
            if (!_instance2.load()) // Операция не атомарна: чтение + сравнение.
            {
                std::lock_guard lock(_mutex); // Делаем thread safety, чтобы не было data race
                if (!_instance2)
                    _instance2.store(new Singleton());  // Операция атомарна
            }
            
            return _instance2;
        }
        
    private:
        Singleton() = default;
        ~Singleton() // Singleton - static, поэтому в деструктор не вызовется
        {
            if (_instance1)
            {
                delete _instance1; // Нет смысла удалять static переменные,они живут столько же сколько и программа, поэтому после завершения программы память будет освобождена системой.
                _instance1 = nullptr;
            }
            if (_instance2)
            {
                delete _instance2; // Нет смысла удалять static переменные,они живут столько же сколько и программа, поэтому после завершения программы память будет освобождена системой.
                _instance2 = nullptr;
            }
        }
        Singleton(const Singleton&) = delete;
        Singleton(Singleton&&) = delete;
        Singleton& operator = (const Singleton&) = delete;
        Singleton& operator = (Singleton&&) = delete;
        
    private:
        static inline Singleton* _instance1 = nullptr;
        static inline std::atomic<Singleton*> _instance2 = nullptr;
        static inline std::mutex _mutex;
    };
}

/*
 Потокобезопасный Singleton Майерса с C++11
 Решение: использовать static переменную.
 */
namespace pattern
{
    struct Singleton
    {
        static Singleton& Instance()
        {
            static Singleton instance;
            return instance;
        }
    };
}

#endif /* Singleton_h */
