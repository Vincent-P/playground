cmake_minimum_required(VERSION 3.19)

set(CMAKE_CXX_STANDARD 17)

project(test-vulkan LANGUAGES CXX)

find_package(Vulkan REQUIRED)
find_package(Threads)

add_subdirectory(third_party)
add_subdirectory(engine)
