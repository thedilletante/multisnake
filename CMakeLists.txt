cmake_minimum_required(VERSION 3.6)
project(multisnake)

set(CMAKE_CXX_STANDARD 14)

set(DIR ${CMAKE_CURRENT_LIST_DIR})
set(SRC_DIR ${DIR}/src)
set(LIBS_DIR ${DIR}/libs)

set(MAIN_SRC
  ${SRC_DIR}/multisnake.cpp
  ${SRC_DIR}/Lobby.hpp
  ${SRC_DIR}/Lobby.cpp
  ${SRC_DIR}/util.hpp
)

add_executable(${PROJECT_NAME} ${MAIN_SRC})

target_include_directories(${PROJECT_NAME} PRIVATE ${LIBS_DIR}/websocketpp)
target_compile_definitions(${PROJECT_NAME} PRIVATE ASIO_STANDALONE=1)

target_include_directories(${PROJECT_NAME} PRIVATE ${LIBS_DIR}/executors/include)


enable_testing()