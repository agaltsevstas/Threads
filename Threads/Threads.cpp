#include "OpenMP.h"
#include "TBB.h"
#include "Timer.h"


#include <iostream>
#include <future>
#include <mutex>
#include <numeric>
#include <thread>
#include <vector>


static int sum_static = 0;

int Sum(int number)
{
	sum_static += number;
	return sum_static;
}

bool MyThread(int something)
{
	std::cout << "This is a thread function\n" << std::endl;

	for (int i = 0; i < 10000; i++)
	{
		something++;
	}

	return true;
}


int main()
{
	Timer timer;
	setlocale(LC_ALL, "Russian");

	constexpr int size = 1000;
	std::vector<int> numbers(size);
	std::iota(numbers.begin(), numbers.end(), 1);

	// Без многопоточности
	{
		std::cout << "Без многопоточности" << std::endl;
		int sum = 0;
		timer.start();
		for (int i = 0; i < size; ++i)
		{
			sum += numbers[i];
		}
		timer.stop();
		std::cout << "Сумма: " << sum << " Время: " << timer.elapsedMilliseconds() << " мс" << std::endl;
		std::cout << std::endl;
	}

	// Параллелизм
	{
		std::cout << "Параллелизм" << std::endl;
		
		/*
		* OpenMP(Open Multi - Processing) — это библиотека, используемая для многопоточности на уровне цикла.
		* Использование параллельной версии STL (все алгоритмы внутри поддерживают OpenMP и будут выполняться параллельно), для этого нужно передать libstdc++ parallel в компилятор GCC
		* До цикла for будет определено кол-во ядер в системе. Код внутри for будет выделен в отдельную функцию (НЕ сам For!!! try catch бессмысленен - нельзя отловить исключение в отдельном потоке, break, continue, return - невозможны).
		  В конце области видимости для каждого потока будет вызван join()
		* Например для 10 ядерной системы будет запущено 10 потоков. При 10.000 итераций. Каждый поток обработает 1.000 / 10 = 1.000 элементов в контейнере
		* Не подходит:
		  1) для рекурсии (может кончиться стек). Есть исключение с использованием очереди задач #pragma omp task. При условии, что размер очереди < 255.
		  Можно на определенном уровне стека запоминать состояния (значения переменных) и кидать в очередь.
		  После окончания функции в рамках того же потока или другого продолжаем вызывать ту же функцию с такой же логикой. Таким развязываем рекурсию по потокам через очередь.
		  2) не всегда обеспечивает хорошую производительность
		* Подробнее: https://learn.microsoft.com/ru-ru/cpp/parallel/openmp/2-directives?view=msvc-170
		*/
		{
			openmp::start();
		}

		/*
		* Использовать C++17
		* TBB библиотека(Intel) - является высокоуровневой библиотекой, чем OpenMP
		* В TBB есть планировщих задач, который помогает лучше оптимизировать работу.
		  Это достигается с помощью алгоритма work stealing, который реализует динамическую балансировку нагрузки:
		  если есть функция разной сложности, то если какой-то поток обработал очень быстро свою очередь задач, то он возьмет часть задач тех потоков, которые еще это не сделали.
		* В TBB нельзя создать поток, поэтому в каких потоках идет выполнение знать не нужно. Можно только создать задачу и отдать ее на исполнение планировщику.
		*/
		{
			tbb::start();
		}

		// Thread
		{
			std::vector<std::thread> threads;
			threads.reserve(size);

			int sum = 0;

			// 1 способ
			{
				timer.start();
				for (const auto& number : numbers)
				{
					threads.emplace_back([&sum, &number]()
						{ sum += number; });
				}

				for (auto& thread : threads)
				{
					thread.join();
				}
				timer.stop();
				std::cout << "Способ Thread, с указанием размера массива, Сумма: " << sum << " Время: " << timer.elapsedMilliseconds() << " мс" << std::endl;
			}
		}
	}

	// Асинхронность
	{
		std::cout << "Асинхронность" << std::endl;
		std::vector<std::future<int>> futures;
		futures.reserve(size);
		int sum = 0;

		// 1 способ
		{
			timer.start();
			for (int i = 0; i < size; ++i)
				futures.emplace_back(std::async(std::launch::async, Sum, i));

			for (auto& future : futures)
				sum += future.get();

			timer.stop();
			std::cout << "1 cпособ Future, Сумма: " << sum << " Время: " << timer.elapsedMilliseconds() << " мс" << std::endl;
		}

		// 2 способ
		{
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
	}

	//std::vector<std::vector<int>> vec1, vec2;

	//func(10000, 10000, vec1);
	//func(500, 500, vec2);

	//std::thread th1 = std::thread([&vec1]() { func(100, 100, vec1); });
	//std::thread th2 = std::thread([&vec2]() { func(500, 500, vec2); });

	//th1.join();
	//th2.join();

	timer.stop();
	std::cout << "Суммурное время: " << timer.elapsedMilliseconds() << " мс" << std::endl;

	return 0;
}