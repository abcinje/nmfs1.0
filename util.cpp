#include <stdexcept>
#include "util.hpp"

std::unique_ptr<std::string> get_parent_dir_path(std::string_view path) {
	if (path.empty()) {
		throw std::runtime_error("Invalid path: path is empty string");
	} else {
		for (size_t i = path.length() - 1; i > 0; i--) {
			if (path[i] == path_delimiter) {
				return std::move(std::make_unique<std::string>(path.substr(0, i)));
			}
		}
		// root directory
		if (path[0] == path_delimiter) {
			return std::move(std::make_unique<std::string>(path.substr(0, 1)));
		}

		throw std::runtime_error("Invalid path: No delimiter on path");
	}
}

std::unique_ptr<std::string> get_filename_from_path(std::string_view path) {
	if (path.empty()) {
		throw std::runtime_error("Invalid path: path is empty string");
	} else {
		// root directory
		if ((path.length() == 1) && (path[0] == path_delimiter)) {
			return std::move(std::make_unique<std::string>(path.substr(0, 1))); // root directory
		}

		for (size_t i = path.length() - 1; i >= 0; i--) {
			if (path[i] == path_delimiter) {
				return std::move(std::make_unique<std::string>(path.substr(i+1)));
			}
		}
		throw std::runtime_error("Invalid path: No delimiter on path");
	}
}