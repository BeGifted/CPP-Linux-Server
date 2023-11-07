cmake_minimum_required(VERSION 3.0)
project(chat)

include (cmake/utils.cmake)

set(CMAKE_VERBOSE_MAKEFILE ON)
set(CMAKE_CXX_FLAGS "$ENV{CXXFLAGS} -rdynamic -O0 -ggdb -std=c++20 -Wall -Wno-deprecated -Werror -Wno-unused-function -Wno-builtin-macro-redefined -Wno-deprecated-declarations")

include_directories(.)
include_directories(/usr/local/include)
link_directories(/usr/local/lib)

# find_library(YAMLCPP libyaml-cpp.so)
# message("***", ${YAMLCPP})

set(LIB_SRC
    chat/log.cc
    chat/util.cc
    chat/config.cc
    chat/thread.cc
)

add_library(chat SHARED ${LIB_SRC})
force_redefine_file_macro_for_sources(chat) #__FILE__
#add_library(chat_static STATIC ${LIB_SRC})
#SET_TARGET_PROPERTIES (chat_static PROPERTIES OUTPUT_NAME "chat")

set(LIB_LIB
    chat
    pthread
    -L/usr/local/lib -lyaml-cpp
)

add_executable(test tests/test.cc)
add_dependencies(test chat)
force_redefine_file_macro_for_sources(test) #__FILE__
target_link_libraries(test chat ${LIB_LIB})

add_executable(test_config tests/test_config.cc)
add_dependencies(test_config chat)
force_redefine_file_macro_for_sources(test_config) #__FILE__
target_link_libraries(test_config ${LIB_LIB})

add_executable(test_thread tests/test_thread.cc)
add_dependencies(test_thread chat)
force_redefine_file_macro_for_sources(test_thread) #__FILE__
target_link_libraries(test_thread ${LIB_LIB})

SET(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)
SET(LIBRARY_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/lib)