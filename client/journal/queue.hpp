#ifndef _QUEUE_HPP_
#define _QUEUE_HPP_

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
	void issue(T value);
	T dispatch(void);
};

#endif /* _QUEUE_HPP_ */
