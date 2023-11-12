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
    chat/config.cc
    chat/fiber.cc
    chat/iomanager.cc
    chat/log.cc
    chat/thread.cc
    chat/util.cc
    chat/scheduler.cc
    chat/timer.cc
    chat/mutex.cc
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

add_executable(test_util tests/test_util.cc)
add_dependencies(test_util chat)
force_redefine_file_macro_for_sources(test_util) #__FILE__
target_link_libraries(test_util ${LIB_LIB})

add_executable(test_fiber tests/test_fiber.cc)
add_dependencies(test_fiber chat)
force_redefine_file_macro_for_sources(test_fiber) #__FILE__
target_link_libraries(test_fiber ${LIB_LIB})

add_executable(test_scheduler tests/test_scheduler.cc)
add_dependencies(test_scheduler chat)
force_redefine_file_macro_for_sources(test_scheduler) #__FILE__
target_link_libraries(test_scheduler ${LIB_LIB})

add_executable(test_iomanager tests/test_iomanager.cc)
add_dependencies(test_iomanager chat)
force_redefine_file_macro_for_sources(test_iomanager) #__FILE__
target_link_libraries(test_iomanager ${LIB_LIB})

SET(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)
SET(LIBRARY_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/lib)