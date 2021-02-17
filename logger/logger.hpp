#ifndef _LOGGER_HPP_
#define _LOGGER_HPP_

#include <iostream>
#include <string>

class logger {
public:
	void log(const std::string &msg);
};

extern logger global_logger;

#endif /* _LOGGER_HPP_ */
