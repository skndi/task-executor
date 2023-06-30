project(RaytracerExecutor)

if (NOT EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/raytracer/CMakeLists.txt)
    message(WARNING "Raytracer not initialized, skipping")
    return()
endif()

set (CMAKE_CXX_STANDARD 20)

set(SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/raytracer/src/Material.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/raytracer/src/Material.cpp

    ${CMAKE_CURRENT_SOURCE_DIR}/raytracer/src/Image.hpp

    ${CMAKE_CURRENT_SOURCE_DIR}/raytracer/src/Primitive.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/raytracer/src/Primitive.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/raytracer/src/Accelerators.cpp

    ${CMAKE_CURRENT_SOURCE_DIR}/raytracer/src/Utils.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/raytracer/src/Threading.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/raytracer/src/Mesh.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/raytracer/src/Mesh.cpp

    ${CMAKE_CURRENT_SOURCE_DIR}/raytracer/src/Mesh.cpp

    ${CMAKE_CURRENT_SOURCE_DIR}/raytracer/src/third_party/stb_image_write.h
    ${CMAKE_CURRENT_SOURCE_DIR}/raytracer/src/third_party/tiny_obj_loader.h
    Raytracer.cpp
)

add_library(${PROJECT_NAME} SHARED ${SOURCES})

target_compile_definitions(${PROJECT_NAME} PRIVATE
    MESH_FOLDER="${CMAKE_CURRENT_SOURCE_DIR}/raytracer/mesh"
)

target_include_directories(${PROJECT_NAME} PUBLIC
    ../TaskSystem
    ../rwlock
    raytracer/src
)

install(TARGETS ${PROJECT_NAME} DESTINATION ${PLUGIN_INSTALL_PATH})
