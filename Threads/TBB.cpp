#include "TBB.h"
#include "Timer.h"

#include <iostream>
#include <numeric>
#include <sstream>
#include <tbb/tbb.h>

namespace tbb 
{
	void start()
	{
		Timer timer;

		constexpr int size = 1000;
		std::vector<int> numbers(size);
		std::iota(numbers.begin(), numbers.end(), 1);

        /*
          Использовать C++17
          TBB (Threading Building Blocks) - библиотека Intel, является высокоуровневой библиотекой, чем OpenMP.
          В TBB есть планировщих задач, который помогает лучше оптимизировать работу.
          Это достигается с помощью алгоритма work stealing, который реализует динамическую балансировку нагрузки. Есть функция разной сложности, какой-то поток очень быстро обработал свою очередь задач, то он возьмет часть свободных задач другого потока. В TBB самому создать поток нельзя, поэтому в каких потоках идет выполнение знать не нужно. Можно только создать задачу и отдать ее на исполнение планировщику.
          Подробнее: https://oneapi-src.github.io/oneTBB/
        */
		std::cout << "TBB" << std::endl;
		// parallel_for - Параллельное выполнение операции над каждым элементом контейнера
		{
			// Линейное число потоков
			{
				// 1 способ parallel_for
				{
					int sum = 0;
					timer.start();
					tbb::parallel_for(0, (int)numbers.size(), [&sum, &numbers](int index)
						{
							sum += numbers[index];
							//std::cout << "thread id: " << std::this_thread::get_id() << " - Index = " << index << std::endl; // Если закоментить, может не считать в отдельном потоке
						});
					timer.stop();
					std::cout << "1 способ parallel_for, TBB сам определяет нужное число потоков линейно, Сумма: " << sum << " Время: " << timer.elapsedMilliseconds() << " мс" << std::endl;
				}

				// 2 способ parallel_for, фиксированный шаг диапазона (Index first, Index last)
				{
					int sum = 0;
					constexpr int step = 20; // шаг
					timer.start();
					tbb::parallel_for<int>(0, (int)numbers.size(), step, [&sum, &numbers](int index)
						{
							sum += numbers[index];
							//std::cout << "thread id: " << std::this_thread::get_id() << " - Index = " << index << std::endl; // Если закоментить, может не считать в отдельном потоке
						});
					timer.stop();
					std::cout << "2 способ parallel_for, Сумма: " << sum << " Время: " << timer.elapsedMilliseconds() << " мс" << std::endl;
				}

				// parallel_for_each
				{
					int sum = 0;
					tbb::spin_rw_mutex mutex; // Аналог в STL - std::mutex
					timer.start();
					tbb::parallel_for_each(numbers, [&sum, &mutex](int number)
						{
							sum += number;
							if (sum % 2 == 0)
							{
								tbb::spin_rw_mutex::scoped_lock lock(mutex); // Аналог в STL - std::lock_guard или в OpenNM - barrier, critical
							}
							//std::cout << "thread id: " << std::this_thread::get_id() << " - Index = " << index << std::endl; // Если закоментить, может не считать в отдельном потоке
						});
					timer.stop();
					std::cout << "1 способ parallel_for, Сумма: " << sum << " Время: " << timer.elapsedMilliseconds() << " мс" << std::endl;
				}
			}

			// Число потоков определяется массивом
			{
				// 1 способ, blocked_range: потоки выделяются с помощью двумерного фиксированного массива
				{
					int sum = 0;
					constexpr int step = 20;
					timer.start();
					tbb::parallel_for(tbb::blocked_range<int>(0, step), [&sum](const tbb::blocked_range<int>& r)
						{
							for (int i = r.begin(); i < r.end(); ++i)
							{
								sum += i;
							}
							//std::cout << "thread id: " << std::this_thread::get_id() << " - Index = " << r.begin() << std::endl; // Если закоментить, может не считать в отдельном потоке
						});
					timer.stop();
					std::cout << "1 способ blocked_range, Сумма: " << sum << " Время: " << timer.elapsedMilliseconds() << " мс" << std::endl;
				}

				// 2 способ, blocked_range: потоки выделяются с помощью трехмерного фиксированного массива
				{
					int sum = 0;
					constexpr int step = 20;
					timer.start();
					tbb::parallel_for(tbb::blocked_range<int>(0, step, step), [&sum](const tbb::blocked_range<int>& r)
						{
							for (int i = r.begin(); i < r.end(); ++i)
							{
								sum += i;
							}
							//std::cout << "thread id: " << std::this_thread::get_id() << " - Index = " << r.begin() << std::endl; // Если закоментить, может не считать в отдельном потоке
						});
					timer.stop();
					std::cout << "2 способ blocked_range, Сумма: " << sum << " Время: " << timer.elapsedMilliseconds() << " мс" << std::endl;
				}
			}
		}

		// parellel_reduce - аналог в OpenNM - reduction. Помогает распараллелить не только задачи, но и функции слияния(может быть отдельной задачей)
		{
#if __cplusplus == 201703L // C++17
			// 1 способ, parellel_reduce: Class
			{
				struct Sum
				{
					float value;
					Sum() : value(0) {}
					Sum(Sum& s, tbb::split) { value = 0; }
					void operator()(tbb::blocked_range<std::vector<int>::iterator>& r)
					{
						float temp = value;
						for (const auto number : r)
						{
							temp += number;
						}
						value = temp;
					}

					void join(Sum& rhs)
					{
						value += rhs.value;
					}
				};

				Sum sum;
				timer.start();
				tbb::parallel_reduce(tbb::blocked_range<std::vector<int>::iterator>(numbers.begin(), numbers.end()), sum); // Не работает с С++20
				timer.stop();
				std::cout << "1 способ, parellel_reduce: Class, Сумма: " << sum.value << " Время: " << timer.elapsedMilliseconds() << " мс" << std::endl;
			}
#endif

			// 2 способ, parellel_reduce: Lambda
			{
				using range_type = tbb::blocked_range<std::vector<int>::iterator>;
				timer.start();
				auto sum = tbb::parallel_reduce(range_type(numbers.begin(), numbers.end()), 0,
					[](const range_type& r, int init)
					{
						return std::accumulate(r.begin(), r.end(), init);
					},
					std::plus<int>());
				timer.stop();
				std::cout << "2 способ, parellel_reduce: Lambda, Сумма: " << sum << " Время: " << timer.elapsedMilliseconds() << " мс" << std::endl;
			}

			// 3 способ, parellel_reduce: Lambda
			{
				using range_type = tbb::blocked_range<std::vector<int>::iterator>;
				range_type(numbers.begin(), numbers.end());
				timer.start();

				auto sum = tbb::parallel_reduce(range_type(numbers.begin(), numbers.end()),
					0.0,
					[](const range_type& r, int init)->int
					{
						for (auto it = r.begin(); it != r.end(); ++it)
						{
							init += *it;
						}
						return init;
					},

					[](int x, int y)->int {
						return x + y;
					}
				);

				timer.stop();
				std::cout << "2 способ, parellel_reduce: Lambda, Сумма: " << sum << " Время: " << timer.elapsedMilliseconds() << " мс" << std::endl;
			}
		}

		// parellel_invoke - параллельное выполнение функции с помощью фиксированного числа
		{
			std::function<int(int)> Fib;
			Fib = [&Fib](int n)->int
				{
					if (n < 2) // 2 is minimum
					{
						return n;
					}
					else {
						int x, y;
						tbb::parallel_invoke([&] {x = Fib(n - 1); },  // 1 функтор
							[&] {y = Fib(n - 2); }); // 2 функтор
						return x + y;
					}
				};

			auto result = Fib(5);
		}

		// task_group: отложенное выполнение задач
		{
			// Здесь tasks запускаются сразу
			std::function<int(int)> Fib;
			Fib = [&Fib](int n)->int
				{
					if (n < 2)
					{
						return n;
					}
					else {
						int x, y;
						tbb::task_group g;
						g.run([&] {x = Fib(n - 1); }); // Запуск первой task
						g.run([&] {y = Fib(n - 2); }); // Запуск второй task
						g.wait();                // Подождать, пока обе задачи не завершаться
						return x + y;
					}
				};
		}

		// parellel_do - почти тоже самое, что parallel_for, только еще можно добавлять итерирование в оннлайн режиме и сделать фактически бессконечный конвеер.
		{

		}

		/*
		* parellel_pipeline - распараллеливание на определенном участке
		*            _____
		*           |_____|
		*    _____ / _____ \ _____    _____
		* - |_____|-|_____|-|_____| -|_____|
		*          \ _____ /
		*			|_____|
		*/

		{
			/*
			* 3 последовательных фильтра:
			* 1 последовательный фильтр читает строку из ФАЙЛА!!!
			* 2 параллельный фильтр берет строку, которую прочитал 1 фильтр, сортирует параллельно строку
			* 3 последовательный фильтр берет отсортированную строку из 2 фильтра и пишет в файл
			*/

			std::istringstream input; // Строка, не ФАЙЛ! Поэтому дальше не работает
			std::ostringstream out;
			input.str("1\n2\n3\n4\n5\n6\n7\n\0");

			/*
			tbb::parallel_pipeline(2, //only two files at once
				tbb::make_filter<void, std::string*>(
					tbb::filter_mode::serial_in_order,
					[&input](tbb::flow_control& fc)-> std::string*
					{
						auto line = new std::string();
						std::getline(input, *line);
						if (!input.eof() && line->length() == 0)
						{
							fc.stop();
							delete line;
							line = nullptr;
						}

						return line;
					}
				)
				&
				tbb::make_filter<std::string*, std::string*>(
					tbb::filter_mode::parallel,
					[](std::string* line)
					{
						tbb::parallel_sort(line->begin(), line->end());
						return line;
					}
				)
				&
				tbb::make_filter<std::string*, void>(
					tbb::filter_mode::serial_in_order,
					[&out](std::string* line)
					{
						out << *line << std::endl;
						delete line;
					}
				)
			);
			*/
		}

		// graph - Построение графа. Относится к сервисным архитектурам
		{
			tbb::flow::graph g;

			// Построение ребра
			tbb::flow::continue_node<tbb::flow::continue_msg> node1(g, [](const tbb::flow::continue_msg&)
				{
					std::cout << "node 1 with";
				});

			tbb::flow::continue_node<tbb::flow::continue_msg> node2(g, [](const tbb::flow::continue_msg&)
				{
					std::cout << " node 2" << std::endl;
				});

			tbb::flow::make_edge(node1, node2);
			node1.try_put(tbb::flow::continue_msg());
			g.wait_for_all(); // Исполнение
		}

		std::cout << std::endl;
	}
}
