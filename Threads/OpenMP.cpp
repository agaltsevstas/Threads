#include "OpenMP.h"
#include "Timer.h"

#include <iostream>
#include <numeric>
#include <omp.h>
#include <vector>

namespace openmp
{
	void start()
	{
		Timer timer;

		constexpr int size = 1000;
		std::vector<int> numbers(size);
		std::iota(numbers.begin(), numbers.end(), 1);

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
		std::cout << "OpenMP" << std::endl;
		// 1 способ, без указания кол-во потоков
		{
			int sum = 0;
			timer.start();

			/*
			* j - Общая (разделяемая) переменная во всех потоках (будет передан указатель переменной в каждый поток, синхронизации не будет)
			* num - Своя локальная переменная в каждом потоке
			* sum - переменная для выполнения редуционной операции. reduction(Op:variables), где Op - список операторов (+,*,-&,^,|,&&,||), variables - список переменных, разделяемых запятыми
			*/
#pragma omp parallel for
			for (int i = 0; i < size; ++i)
			{
				sum += numbers[i];
			}
			timer.stop();
			std::cout << "1 способ OpenMP, без указания размера массива, Сумма: " << sum << " Время: " << timer.elapsedMilliseconds() << " мс" << std::endl;
		}

		// 2 способ, с указанием кол-во потоков
		{
			int sum = 0;

			timer.start();
#pragma omp parallel for num_threads(size) 
			for (int i = 0; i < size; ++i)
			{
				sum += numbers[i];
			}
			timer.stop();
			std::cout << "2 способ OpenMP, с указанием размера массива, Сумма: " << sum << " Время: " << timer.elapsedMilliseconds() << " мс" << std::endl;
		}

		// 3 способ, с дополнительными параметрами
		{
			int j, num, x = 0, sum = 0, difference = 0;
			timer.start();

			/*
			* j - Общая (разделяемая) переменная во всех потоках (будет передан указатель переменной в каждый поток, синхронизации не будет)
			* num - Своя локальная переменная в каждом потоке
			* sum - переменная для выполнения редуционной операции. reduction(Op:variables), где Op - список операторов (+,*,-&,^,|,&&,||), variables - список переменных, разделяемых запятыми
			*/
#pragma omp parallel for \
				shared(j) \
				private(num) \
				reduction(+:sum, x) \
				reduction(-:difference)
			for (int i = 0; i < size; ++i)
			{
				j = i;
				sum += numbers[i];
				difference -= numbers[i];
				num = omp_get_thread_num(); // Номер потока
				//std::cout << "Номер потока" << num << std::endl;
				//std::cout << "Переменная j = " << j << std::endl;


#pragma omp critical(name) // Сюда может заходить только один поток, name - название критической секции
				{
					++x;
				}
			}
		}

		// 4 способ, вручную распараллелить потоки на определенных блоках
		{
			int i, sum = 0, num_threads = 3;
#pragma omp parallel sections num_threads(num_threads) private(i) reduction(+:sum)
			{

#pragma omp section
				{
					for (i = 0; i < size / 2; ++i)
					{

					}
				}

#pragma omp section
				{
					for (i = size / 2; i < size; ++i)
					{

					}
				}

#pragma omp section
				{
					++sum;
				}
			}

#pragma omp barrier // Дождаться пока все потоки дойдут до этого места
		}

		// 5 способ, блок выполняется только одним потоком
#pragma omp single
		{

		}

		std::cout << std::endl;
	}
}
