#pragma once

#include <chrono>

class Timer
{
public:
	void start()
	{
		_startTime = std::chrono::system_clock::now();
		_running = true;
	}

	void stop()
	{
		_endTime = std::chrono::system_clock::now();
		_running = false;
	}

	double elapsedMilliseconds() const noexcept
	{
        std::chrono::time_point<std::chrono::system_clock> endTime;

        if (_running)
        {
            endTime = std::chrono::system_clock::now();
        }
        else
        {
            endTime = _endTime;
        }

        return (double)std::chrono::duration_cast<std::chrono::milliseconds>(endTime - _startTime).count();
	}

	double elapsedSeconds() const noexcept
	{
		return elapsedMilliseconds() / 1000.0;
	}

private:
	std::chrono::time_point<std::chrono::system_clock> _startTime;
	std::chrono::time_point<std::chrono::system_clock> _endTime;
	bool _running = false;
};
