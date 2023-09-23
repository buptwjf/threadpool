cmake_minimum_required(VERSION 3.25)
project(my_threadpool)

set(CMAKE_CXX_STANDARD 20)

add_executable(my_threadpool Threadpool_test.cpp Threadpool.cpp  )
