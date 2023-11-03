cmake_minimum_required(VERSION 3.0)
project(chat)

set(CMAKE_VERBOSE_MAKEFILE ON)
set(CMAKE_CXX_FLAGS "$ENV{CXXFLAGS} -rdynamic -O0 -ggdb -std=c++20 -Wall -Wno-deprecated -Werror -Wno-unused-function -Wno-builtin-macro-redefined -Wno-deprecated-declarations")

set(LIB_SRC
    chat/log.cc
    chat/util.cc
)

add_library(chat SHARED ${LIB_SRC})
#add_library(chat_static STATIC ${LIB_SRC})
#SET_TARGET_PROPERTIES (chat_static PROPERTIES OUTPUT_NAME "chat")

add_executable(test "tests/test.cc")
add_dependencies(test chat)
target_link_libraries(test chat)

SET(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)
SET(LIBRARY_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/lib)