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
		* ������������ C++17
		* TBB ����������(Intel) - �������� ��������������� �����������, ��� OpenMP
		* � TBB ���� ����������� �����, ������� �������� ����� �������������� ������.
		  ��� ����������� � ������� ��������� work stealing, ������� ��������� ������������ ������������ ��������:
		  ���� ���� ������� ������ ���������, �� ���� �����-�� ����� ��������� ����� ������ ���� ������� �����, �� �� ������� ����� ����� ��� �������, ������� ��� ��� �� �������.
		* � TBB ������ ������� �����, ������� � ����� ������� ���� ���������� ����� �� �����. ����� ������ ������� ������ � ������ �� �� ���������� ������������.
		*/
		std::cout << "TBB" << std::endl;
		// parallel_for - ������������ ���������� �������� ��� ������ ��������� ����������
		{
			// �������� ����� �������
			{
				// 1 ������ parallel_for
				{
					int sum = 0;
					timer.start();
					tbb::parallel_for(0, (int)numbers.size(), [&sum, &numbers](int index)
						{
							sum += numbers[index];
							//std::cout << "thread id: " << std::this_thread::get_id() << " - Index = " << index << std::endl; // ���� �����������, ����� �� ������� � ��������� ������
						});
					timer.stop();
					std::cout << "1 ������ parallel_for, TBB ��� ���������� ������ ����� ������� �������, �����: " << sum << " �����: " << timer.elapsedMilliseconds() << " ��" << std::endl;
				}

				// 2 ������ parallel_for, ������������� ��� ��������� (Index first, Index last)
				{
					int sum = 0;
					constexpr int step = 20; // ���
					timer.start();
					tbb::parallel_for<int>(0, (int)numbers.size(), step, [&sum, &numbers](int index)
						{
							sum += numbers[index];
							//std::cout << "thread id: " << std::this_thread::get_id() << " - Index = " << index << std::endl; // ���� �����������, ����� �� ������� � ��������� ������
						});
					timer.stop();
					std::cout << "2 ������ parallel_for, �����: " << sum << " �����: " << timer.elapsedMilliseconds() << " ��" << std::endl;
				}

				// parallel_for_each
				{
					int sum = 0;
					tbb::spin_rw_mutex mutex; // ������ � STL - std::mutex
					timer.start();
					tbb::parallel_for_each(numbers, [&sum, &mutex](int number)
						{
							sum += number;
							if (sum % 2 == 0)
							{
								tbb::spin_rw_mutex::scoped_lock lock(mutex); // ������ � STL - std::lock_guard ��� � OpenNM - barrier, critical
							}
							//std::cout << "thread id: " << std::this_thread::get_id() << " - Index = " << index << std::endl; // ���� �����������, ����� �� ������� � ��������� ������
						});
					timer.stop();
					std::cout << "1 ������ parallel_for, �����: " << sum << " �����: " << timer.elapsedMilliseconds() << " ��" << std::endl;
				}
			}

			// ����� ������� ������������ ��������
			{
				// 1 ������, blocked_range: ������ ���������� � ������� ���������� �������������� �������
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
							//std::cout << "thread id: " << std::this_thread::get_id() << " - Index = " << r.begin() << std::endl; // ���� �����������, ����� �� ������� � ��������� ������
						});
					timer.stop();
					std::cout << "1 ������ blocked_range, �����: " << sum << " �����: " << timer.elapsedMilliseconds() << " ��" << std::endl;
				}

				// 2 ������, blocked_range: ������ ���������� � ������� ����������� �������������� �������
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
							//std::cout << "thread id: " << std::this_thread::get_id() << " - Index = " << r.begin() << std::endl; // ���� �����������, ����� �� ������� � ��������� ������
						});
					timer.stop();
					std::cout << "2 ������ blocked_range, �����: " << sum << " �����: " << timer.elapsedMilliseconds() << " ��" << std::endl;
				}
			}
		}

		// parellel_reduce - ������ � OpenNM - reduction. �������� �������������� �� ������ ������, �� � ������� �������(����� ���� ��������� �������)
		{
			// 1 ������, parellel_reduce: Class
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
				tbb::parallel_reduce(tbb::blocked_range<std::vector<int>::iterator>(numbers.begin(), numbers.end()), sum); // �� �������� � �++20
				timer.stop();
				std::cout << "1 ������, parellel_reduce: Class, �����: " << sum.value << " �����: " << timer.elapsedMilliseconds() << " ��" << std::endl;
			}

			// 2 ������, parellel_reduce: Lambda
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
				std::cout << "2 ������, parellel_reduce: Lambda, �����: " << sum << " �����: " << timer.elapsedMilliseconds() << " ��" << std::endl;
			}

			// 3 ������, parellel_reduce: Lambda
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
				std::cout << "2 ������, parellel_reduce: Lambda, �����: " << sum << " �����: " << timer.elapsedMilliseconds() << " ��" << std::endl;
			}
		}

		// parellel_invoke - ������������ ���������� ������� � ������� �������������� �����
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
						tbb::parallel_invoke([&] {x = Fib(n - 1); },  // 1 �������
							[&] {y = Fib(n - 2); }); // 2 �������
						return x + y;
					}
				};

			auto result = Fib(5);
		}

		// task_group: ���������� ���������� �����
		{
			// ����� tasks ����������� �����
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
						g.run([&] {x = Fib(n - 1); }); // ������ ������ task
						g.run([&] {y = Fib(n - 2); }); // ������ ������ task
						g.wait();                // ���������, ���� ��� ������ �� �����������
						return x + y;
					}
				};
		}

		// parellel_do - ����� ���� �����, ��� parallel_for, ������ ��� ����� ��������� ������������ � ������� ������ � ������� ���������� ������������ �������.
		{

		}

		/*
		* parellel_pipeline - ����������������� �� ������������ �������
		*            _____
		*           |_____|
		*    _____ / _____ \ _____    _____
		* - |_____|-|_____|-|_____| -|_____|
		*          \ _____ /
		*			|_____|
		*/

		{
			/*
			* 3 ���������������� �������:
			* 1 ���������������� ������ ������ ������ �� �����!!!
			* 2 ������������ ������ ����� ������, ������� �������� 1 ������, ��������� ����������� ������
			* 3 ���������������� ������ ����� ��������������� ������ �� 2 ������� � ����� � ����
			*/

			std::istringstream input; // ������, �� ����! ������� ������ �� ��������
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

		// graph - ���������� �����. ��������� � ��������� ������������
		{
			tbb::flow::graph g;

			// ���������� �����
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
			g.wait_for_all(); // ����������
		}

		std::cout << std::endl;
	}
}