#ifndef _MQUEUE_HPP_
#define _MQUEUE_HPP_

#include <condition_variable>
#include <memory>
#include <mutex>
#include <queue>

template <typename T>
class mqueue {
private:
	std::mutex m;
	std::condition_variable cv;
	std::queue<T> q;

public:
	void issue(T value)
	{
		std::scoped_lock lock(m);

		q.push(value);
		cv.notify_one();
	}

	T dispatch(void)
	{
		T value;

		std::scoped_lock lock(m);

		while (q.empty())
			cv.wait(lock);
		value = q.front();
		q.pop();

		return value;
	}
};

#endif /* _MQUEUE_HPP_ */
