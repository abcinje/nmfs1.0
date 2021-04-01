#include "rados_io.hpp"

#include <iostream>
#include <stdexcept>
#include <string>

int main(void)
{
  rados_io::conn_info ci = {
    .user = "client.admin",
    .cluster = "ceph",
    .flags = 0
  };

  rados_io rio(ci, POOL_NAME);

  std::string str = "value";

  librados::bufferlist write_bl = librados::bufferlist::static_from_string(str);
  if (rio.ioctx.write("key", write_bl, 5, 0) < 0)
    throw std::runtime_error("write() failed");

  char buf[30];
  librados::bufferlist read_bl = librados::bufferlist::static_from_mem(buf, 30);

  return rio.ioctx.read("key", read_bl, 10, 2);
  
}
