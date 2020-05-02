cmake_minimum_required(VERSION 2.8.0)
project(vimaj)

include_directories(.)
include_directories(${CMAKE_CURRENT_BINARY_DIR})

find_package(OpenCV)

add_executable(${PROJECT_NAME}
  main.cpp
)

find_package(Threads REQUIRED)
if(THREADS_HAVE_PTHREAD_ARG)
  target_compile_options(${PROJECT_NAME} PUBLIC "-pthread")
endif()
if(CMAKE_THREAD_LIBS_INIT)
  target_link_libraries(${PROJECT_NAME} "${CMAKE_THREAD_LIBS_INIT}")
endif()

target_link_libraries(${PROJECT_NAME}
  ${OpenCV_LIBS}
  glog
  gflags
  boost_thread
  boost_filesystem
  boost_system
)

