cmake_minimum_required(VERSION 3.16)
project(GooseVF LANGUAGES CXX)
option(BUILD_SHARED_LIBS "Build shared libraries" OFF)

file(GLOB_RECURSE source_files
   "src/*.cpp"
)

add_library(GooseVF STATIC ${source_files})
target_compile_features(GooseVF PUBLIC cxx_std_17)

target_include_directories(GooseVF PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)