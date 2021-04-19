# logger
add_library(log SHARED logger/logger.cpp)

# rados_io
add_library(rio SHARED rados_io/rados_io.cpp)
find_library(rados librados.so)
target_link_libraries(rio rados)
