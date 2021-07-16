#ifndef _CONFIG_HPP_
#define _CONFIG_HPP_

#include <cstdlib>
#include <iostream>

#include <libconfig.h++>

using namespace libconfig;

void read_config(Config &config, const char *path);

template <typename T>
T lookup_config(const Config &config, const char *field)
{
	try {
		return config.lookup(field);
	} catch (const SettingNotFoundException &e) {
		std::cerr << "No \'" << field << "\' setting in configuration file." << std::endl;
		exit(1);
	}
}

#endif /* _CONFIG_HPP_ */
