#ifndef _DUMP_HPP_
#define _DUMP_HPP_

#include <cctype>
#include <cstdio>

#define HEXDUMP_COLS 16

void hexdump(void *mem, unsigned int len);

#endif /* _DUMP_HPP_ */
