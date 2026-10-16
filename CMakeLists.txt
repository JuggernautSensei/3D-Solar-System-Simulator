cmake_minimum_required(VERSION 3.21)
project(SolarSystem LANGUAGES CXX)

find_package(OpenGL REQUIRED)
find_package(GLUT REQUIRED)
find_package(soil CONFIG REQUIRED)
set_property(TARGET soil APPEND PROPERTY IMPORTED_CONFIGURATIONS DEBUG)
set_target_properties(soil PROPERTIES
    IMPORTED_LOCATION_DEBUG "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/debug/lib/soil.lib"
)

add_executable(SolarSystem main.cpp)
target_compile_features(SolarSystem PRIVATE cxx_std_20)
target_link_libraries(SolarSystem PRIVATE OpenGL::GL OpenGL::GLU GLUT::GLUT soil)
target_compile_options(SolarSystem PRIVATE /W4 /utf-8 /permissive-)

set_target_properties(SolarSystem PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:SolarSystem>"
)

add_custom_command(TARGET SolarSystem POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/Resources"
        "$<TARGET_FILE_DIR:SolarSystem>/Resources"
)
