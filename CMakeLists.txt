cmake_minimum_required(VERSION 3.16)
project(colorex VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_definitions(-D_MBCS)

add_definitions(-DUNICODE=0)
add_definitions(-D_UNICODE=0)

include_directories(${PROJECT_SOURCE_DIR}/include)

add_library(colorex_lib STATIC src/colorex.cpp)
target_include_directories(colorex_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

set(RC_FILE "${CMAKE_CURRENT_SOURCE_DIR}/app.rc")

add_executable(colorex WIN32 src/main.cpp ${RC_FILE})
target_link_libraries(colorex colorex_lib)

if(MSVC)
    set_target_properties(colorex PROPERTIES 
        LINK_FLAGS "/MANIFEST:NO")
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT colorex)