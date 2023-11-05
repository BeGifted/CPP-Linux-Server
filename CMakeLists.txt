cmake_minimum_required(VERSION 3.0)
project(chat)

set(CMAKE_VERBOSE_MAKEFILE ON)
set(CMAKE_CXX_FLAGS "$ENV{CXXFLAGS} -rdynamic -O0 -ggdb -std=c++20 -Wall -Wno-deprecated -Werror -Wno-unused-function -Wno-builtin-macro-redefined -Wno-deprecated-declarations")

<<<<<<< HEAD
include_directories(.)
include_directories(/usr/local/include)
link_directories(/usr/local/lib)

find_library(YAMLCPP libyaml-cpp.so)

set(LIB_SRC
    chat/log.cc
    chat/util.cc
    chat/config.cc
=======
set(LIB_SRC
    chat/log.cc
    chat/util.cc
>>>>>>> 4254dc14c2b00a548c7237d0b64660f7ab626445
)

add_library(chat SHARED ${LIB_SRC})
#add_library(chat_static STATIC ${LIB_SRC})
#SET_TARGET_PROPERTIES (chat_static PROPERTIES OUTPUT_NAME "chat")

<<<<<<< HEAD
add_executable(test tests/test.cc)
add_dependencies(test chat)
target_link_libraries(test chat -L/usr/local/lib -lyaml-cpp)

add_executable(test_config tests/test_config.cc)
add_dependencies(test_config chat)
target_link_libraries(test_config chat -L/usr/local/lib -lyaml-cpp)
=======
add_executable(test "tests/test.cc")
add_dependencies(test chat)
target_link_libraries(test chat)
>>>>>>> 4254dc14c2b00a548c7237d0b64660f7ab626445

SET(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)
SET(LIBRARY_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/lib)