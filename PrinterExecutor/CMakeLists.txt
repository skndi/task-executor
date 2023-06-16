project(PrinterExecutor)

set (CMAKE_CXX_STANDARD 20)

set(SOURCES
    Printer.cpp
)

add_library(${PROJECT_NAME} SHARED ${SOURCES})

target_include_directories(${PROJECT_NAME} PUBLIC ../TaskSystem)

install(TARGETS ${PROJECT_NAME} DESTINATION ${PLUGIN_INSTALL_PATH})
