cmake_minimum_required(VERSION 3.6)
project(multisnake)

set(CMAKE_CXX_STANDARD 14)

set(DIR ${CMAKE_CURRENT_LIST_DIR})
set(SRC_DIR ${DIR}/src)

set(MAIN_SRC ${SRC_DIR}/multisnake.cpp)

add_executable(${PROJECT_NAME} ${MAIN_SRC})