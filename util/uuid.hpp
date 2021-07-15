#ifndef _UTIL_HPP_
#define _UTIL_HPP_

#include <string>
#include <sys/stat.h>

#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/uuid/uuid_generators.hpp>

#include "lib/logger/logger.hpp"

using namespace boost::uuids;

class uuid_controller {
private:
	random_generator generator;

public:
	uuid alloc_new_uuid();
	static uint64_t get_prefix_from_uuid(const uuid& id);
	static uint64_t get_postfix_from_uuid(const uuid& id);
	static uuid splice_prefix_and_postfix(const uint64_t prefix, const uint64_t postfix);
};

uuid get_root_ino();
std::string uuid_to_string(uuid id);

#endif /* _UTIL_HPP_ */
