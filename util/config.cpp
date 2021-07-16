#include "config.hpp"

void read_config(Config &config, const char *path)
{
	try {
		config.readFile(path);
	} catch (const FileIOException &e) {
		std::cerr << "I/O error while reading file." << std::endl;
		exit(1);
	}
}
