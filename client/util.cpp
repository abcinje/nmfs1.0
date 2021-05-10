#include <stdexcept>
#include "util.hpp"

std::unique_ptr<std::string> get_parent_dir_path(std::string_view path) {
	if (path.empty())
		throw std::runtime_error("Invalid path: path is empty string");

	for (size_t i = path.length() - 1; i > 0; i--)
		if (path[i] == path_delimiter)
			return std::move(std::make_unique<std::string>(path.substr(0, i)));

	// root directory
	if (path[0] == path_delimiter)
		return std::move(std::make_unique<std::string>(path.substr(0, 1)));

	throw std::runtime_error("Invalid path: No delimiter on path");
}

std::unique_ptr<std::string> get_filename_from_path(std::string_view path) {
	if (path.empty())
		throw std::runtime_error("Invalid path: path is empty string");

	// root directory
	if ((path.length() == 1) && (path[0] == path_delimiter))
		return std::move(std::make_unique<std::string>(path.substr(0, 1)));

	for (size_t i = path.length() - 1; i >= 0; i--)
		if (path[i] == path_delimiter)
			return std::move(std::make_unique<std::string>(path.substr(i + 1)));

	throw std::runtime_error("Invalid path: No delimiter on path");
}

void hexdump(void *mem, unsigned int len) {
	unsigned int i, j;

	for (i = 0; i < len + ((len % HEXDUMP_COLS) ? (HEXDUMP_COLS - len % HEXDUMP_COLS) : 0); i++) {
		/* print offset */
		if (i % HEXDUMP_COLS == 0) {
			printf("0x%06x: ", i);
		}

		/* print hex data */
		if (i < len) {
			printf("%02x ", 0xFF & ((char *) mem)[i]);
		} else /* end of block, just aligning for ASCII dump */
		{
			printf("   ");
		}

		/* print ASCII dump */
		if (i % HEXDUMP_COLS == (HEXDUMP_COLS - 1)) {
			for (j = i - (HEXDUMP_COLS - 1); j <= i; j++) {
				if (j >= len) /* end of block, not really printing */
				{
					putchar(' ');
				} else if (isprint(((char *) mem)[j])) /* printable char */
				{
					putchar(0xFF & ((char *) mem)[j]);
				} else /* other char */
				{
					putchar('.');
				}
			}
			putchar('\n');
		}
	}
}