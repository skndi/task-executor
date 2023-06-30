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

set(TASK_SYSTEM_NAME TaskSystem)

add_executable(${TASK_SYSTEM_NAME} "${SOURCES};${HEADERS}")

set_property(TARGET ${TASK_SYSTEM_NAME} PROPERTY INSTALL_RPATH "${PLUGIN_INSTALL_PATH}")

install(TARGETS ${TASK_SYSTEM_NAME} DESTINATION ${PLUGIN_INSTALL_PATH})