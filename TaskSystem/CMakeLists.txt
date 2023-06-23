project(TaskSystem)

set (CMAKE_CXX_STANDARD 20)

set(SOURCES
    TaskSystem.cpp
    main.cpp
)

set(HEADERS
    Task.hpp
    Executor.hpp
    TaskSystem.hpp
)

add_executable(TaskSystem.exe "${SOURCES};${HEADERS}")

target_link_libraries(TaskSystem.exe
    rwlock
)

target_compile_definitions(TaskSystem.exe PRIVATE TS_EXECUTOR_PATH="${PLUGIN_INSTALL_PATH}")

install(TARGETS TaskSystem.exe DESTINATION ${PLUGIN_INSTALL_PATH})