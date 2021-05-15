#include "mqueue.hpp"

template <typename T>
void mqueue<T>::issue(T value)
{
	std::scoped_lock lock(m);

	q.push(value);
	cv.notify_one();
}

template <typename T>
T mqueue<T>::dispatch(void)
{
	T value;

	std::scoped_lock lock(m);

	while (q.empty())
		cv.wait(lock);
	value = q.front();
	q.pop();

	return value;
}
