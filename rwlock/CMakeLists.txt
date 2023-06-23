project(ReadWriteLock)

set (CMAKE_CXX_STANDARD 20)

add_library(rwlock STATIC
    ReadWriteLock.cpp
)
target_include_directories(rwlock PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)