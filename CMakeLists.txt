cmake_minimum_required(VERSION 3.16)

if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
  if(DEFINED ENV{VITASDK})
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VITASDK}/share/vita.toolchain.cmake" CACHE PATH "toolchain file")
  else()
    message(FATAL_ERROR "Please define VITASDK to point to your SDK path!")
  endif()
endif()

project(vitagprof C ASM)
include("${VITASDK}/share/vita.cmake" REQUIRED)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=gnu11")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

add_library(${PROJECT_NAME}
    src/prof.c
    src/mcount.s
)

set_target_properties(${PROJECT_NAME} PROPERTIES PUBLIC_HEADER src/vitagprof.h)

install(TARGETS ${PROJECT_NAME}
  LIBRARY DESTINATION $ENV{VITASDK}/arm-vita-eabi/lib/
  PUBLIC_HEADER DESTINATION $ENV{VITASDK}/arm-vita-eabi/include/
)

add_subdirectory(test EXCLUDE_FROM_ALL)
