#ifndef NMFS0_UTIL_HPP
#define NMFS0_UTIL_HPP

#include <string>
#include <string_view>
#include <memory>

#define HEXDUMP_COLS 16

constexpr char path_delimiter = '/';

std::unique_ptr<std::string> get_parent_dir_path(std::string_view path);
std::unique_ptr<std::string> get_filename_from_path(std::string_view path);
void hexdump(void *mem, unsigned int len);

#endif //NMFS0_UTIL_HPP
