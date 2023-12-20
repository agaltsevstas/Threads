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
		* OpenMP(Open Multi - Processing) � ��� ����������, ������������ ��� ��������������� �� ������ �����.
		* ������������� ������������ ������ STL (��� ��������� ������ ������������ OpenMP � ����� ����������� �����������), ��� ����� ����� �������� libstdc++ parallel � ���������� GCC
		* �� ����� for ����� ���������� ���-�� ���� � �������. ��� ������ for ����� ������� � ��������� ������� (�� ��� For!!! try catch ������������ - ������ �������� ���������� � ��������� ������, break, continue, return - ����������).
		  � ����� ������� ��������� ��� ������� ������ ����� ������ join()
		* �������� ��� 10 ������� ������� ����� �������� 10 �������. ��� 10.000 ��������. ������ ����� ���������� 1.000 / 10 = 1.000 ��������� � ����������
		* �� ��������:
		  1) ��� �������� (����� ��������� ����). ���� ���������� � �������������� ������� ����� #pragma omp task. ��� �������, ��� ������ ������� < 255.
		  ����� �� ������������ ������ ����� ���������� ��������� (�������� ����������) � ������ � �������.
		  ����� ��������� ������� � ������ ���� �� ������ ��� ������� ���������� �������� �� �� ������� � ����� �� �������. ����� ����������� �������� �� ������� ����� �������.
		  2) �� ������ ������������ ������� ������������������
		* ���������: https://learn.microsoft.com/ru-ru/cpp/parallel/openmp/2-directives?view=msvc-170
		*/
		std::cout << "OpenMP" << std::endl;
		// 1 ������, ��� �������� ���-�� �������
		{
			int sum = 0;
			timer.start();

			/*
			* j - ����� (�����������) ���������� �� ���� ������� (����� ������� ��������� ���������� � ������ �����, ������������� �� �����)
			* num - ���� ��������� ���������� � ������ ������
			* sum - ���������� ��� ���������� ����������� ��������. reduction(Op:variables), ��� Op - ������ ���������� (+,*,-&,^,|,&&,||), variables - ������ ����������, ����������� ��������
			*/
#pragma omp parallel for
			for (int i = 0; i < size; ++i)
			{
				sum += numbers[i];
			}
			timer.stop();
			std::cout << "1 ������ OpenMP, ��� �������� ������� �������, �����: " << sum << " �����: " << timer.elapsedMilliseconds() << " ��" << std::endl;
		}

		// 2 ������, � ��������� ���-�� �������
		{
			int sum = 0;

			timer.start();
#pragma omp parallel for num_threads(size) 
			for (int i = 0; i < size; ++i)
			{
				sum += numbers[i];
			}
			timer.stop();
			std::cout << "2 ������ OpenMP, � ��������� ������� �������, �����: " << sum << " �����: " << timer.elapsedMilliseconds() << " ��" << std::endl;
		}

		// 3 ������, � ��������������� �����������
		{
			int j, num, x = 0, sum = 0, difference = 0;
			timer.start();

			/*
			* j - ����� (�����������) ���������� �� ���� ������� (����� ������� ��������� ���������� � ������ �����, ������������� �� �����)
			* num - ���� ��������� ���������� � ������ ������
			* sum - ���������� ��� ���������� ����������� ��������. reduction(Op:variables), ��� Op - ������ ���������� (+,*,-&,^,|,&&,||), variables - ������ ����������, ����������� ��������
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
				num = omp_get_thread_num(); // ����� ������
				//std::cout << "����� ������" << num << std::endl;
				//std::cout << "���������� j = " << j << std::endl;


#pragma omp critical(name) // ���� ����� �������� ������ ���� �����, name - �������� ����������� ������
				{
					++x;
				}
			}
		}

		// 4 ������, ������� �������������� ������ �� ������������ ������
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

#pragma omp barrier // ��������� ���� ��� ������ ������ �� ����� �����
		}

		// 5 ������, ���� ����������� ������ ����� �������
#pragma omp single
		{

		}

		std::cout << std::endl;
	}
}