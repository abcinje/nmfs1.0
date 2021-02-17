#include "logger.hpp"

void logger::log(const std::string &msg)
{
	std::cout << msg << std::endl;
}

logger global_logger;
