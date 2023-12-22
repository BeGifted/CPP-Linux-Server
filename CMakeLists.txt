cmake_minimum_required(VERSION 3.0)
project(chat)

include (cmake/utils.cmake)

set(CMAKE_VERBOSE_MAKEFILE ON)
set(CMAKE_CXX_FLAGS "$ENV{CXXFLAGS} -rdynamic -O0 -ggdb -std=c++20 -Wall -Wno-deprecated -Werror -Wno-unused-function -Wno-builtin-macro-redefined -Wno-deprecated-declarations")

include_directories(.)
include_directories(/usr/local/include)
link_directories(/usr/local/lib64)

# find_library(YAMLCPP libyaml-cpp.so)
# message("***", ${YAMLCPP})

find_package(OpenSSL REQUIRED)
include_directories(${OPENSSL_INCLUDE_DIR})


set(LIB_SRC
    chat/address.cc
    chat/application.cc
    chat/bytearray.cc
    chat/config.cc
    chat/daemon.cc
    chat/env.cc
    chat/fiber.cc
    chat/fd_manager.cc
    chat/hook.cc
    chat/http/http.cc
    chat/http/http_connection.cc
    chat/http/httpclient_parser.cc  # rl
    chat/http/http11_parser.cc  # rl
    chat/http/http_parser.cc
    chat/http/http_session.cc
    chat/http/http_server.cc
    chat/http/servlet.cc
    chat/http/ws_connection.cc
    chat/http/ws_server.cc
    chat/http/ws_servlet.cc
    chat/http/ws_session.cc
    chat/iomanager.cc
    chat/log.cc
    chat/mutex.cc
    chat/protocol.cc
    chat/scheduler.cc
    chat/socket.cc
    chat/stream.cc
    chat/streams/socket_stream.cc
    chat/timer.cc
    chat/thread.cc
    chat/tcp_server.cc
    chat/util.cc
    chat/uri.cc  # rl
    chat/worker.cc
)

add_library(chat SHARED ${LIB_SRC})
force_redefine_file_macro_for_sources(chat) #__FILE__
#add_library(chat_static STATIC ${LIB_SRC})
#SET_TARGET_PROPERTIES (chat_static PROPERTIES OUTPUT_NAME "chat")

set(LIB_LIB
    chat
    jsoncpp
    dl
    pthread
    -L/usr/local/lib -lyaml-cpp
    ${OPENSSL_LIBRARIES}
)


# ragelmaker(chat/http/http11_parser.rl LIB_SRC ${CMAKE_CURRENT_SOURCE_DIR}/chat/http)
# ragelmaker(chat/http/httpclient_parser.rl LIB_SRC ${CMAKE_CURRENT_SOURCE_DIR}/chat/http)
# ragelmaker(chat/uri.rl LIB_SRC ${CMAKE_CURRENT_SOURCE_DIR}/chat)

# add_executable(test_log tests/test_log.cc)
# add_dependencies(test_log chat)
# force_redefine_file_macro_for_sources(test_log) #__FILE__
# target_link_libraries(test_log chat ${LIB_LIB})

# add_executable(test_config tests/test_config.cc)
# add_dependencies(test_config chat)
# force_redefine_file_macro_for_sources(test_config) #__FILE__
# target_link_libraries(test_config ${LIB_LIB})

# add_executable(test_thread tests/test_thread.cc)
# add_dependencies(test_thread chat)
# force_redefine_file_macro_for_sources(test_thread) #__FILE__
# target_link_libraries(test_thread ${LIB_LIB})

# add_executable(test_util tests/test_util.cc)
# add_dependencies(test_util chat)
# force_redefine_file_macro_for_sources(test_util) #__FILE__
# target_link_libraries(test_util ${LIB_LIB})

# add_executable(test_fiber tests/test_fiber.cc)
# add_dependencies(test_fiber chat)
# force_redefine_file_macro_for_sources(test_fiber) #__FILE__
# target_link_libraries(test_fiber ${LIB_LIB})

# add_executable(test_scheduler tests/test_scheduler.cc)
# add_dependencies(test_scheduler chat)
# force_redefine_file_macro_for_sources(test_scheduler) #__FILE__
# target_link_libraries(test_scheduler ${LIB_LIB})

# add_executable(test_iomanager tests/test_iomanager.cc)
# add_dependencies(test_iomanager chat)
# force_redefine_file_macro_for_sources(test_iomanager) #__FILE__
# target_link_libraries(test_iomanager ${LIB_LIB})

# add_executable(test_hook tests/test_hook.cc)
# add_dependencies(test_hook chat)
# force_redefine_file_macro_for_sources(test_hook) #__FILE__
# target_link_libraries(test_hook ${LIB_LIB})

# add_executable(test_address tests/test_address.cc)
# add_dependencies(test_address chat)
# force_redefine_file_macro_for_sources(test_address) #__FILE__
# target_link_libraries(test_address ${LIB_LIB})

# add_executable(test_socket tests/test_socket.cc)
# add_dependencies(test_socket chat)
# force_redefine_file_macro_for_sources(test_socket) #__FILE__
# target_link_libraries(test_socket ${LIB_LIB})

# add_executable(test_bytearray tests/test_bytearray.cc)
# add_dependencies(test_bytearray chat)
# force_redefine_file_macro_for_sources(test_bytearray) #__FILE__
# target_link_libraries(test_bytearray ${LIB_LIB})

# add_executable(test_http tests/test_http.cc)
# add_dependencies(test_http chat)
# force_redefine_file_macro_for_sources(test_http) #__FILE__
# target_link_libraries(test_http ${LIB_LIB})

# add_executable(test_http_parser tests/test_http_parser.cc)
# add_dependencies(test_http_parser chat)
# force_redefine_file_macro_for_sources(test_http_parser) #__FILE__
# target_link_libraries(test_http_parser ${LIB_LIB})

# add_executable(test_tcp_server tests/test_tcp_server.cc)
# add_dependencies(test_tcp_server chat)
# force_redefine_file_macro_for_sources(test_tcp_server) #__FILE__
# target_link_libraries(test_tcp_server ${LIB_LIB})

add_executable(echo_server examples/echo_server.cc)
add_dependencies(echo_server chat)
force_redefine_file_macro_for_sources(echo_server) #__FILE__
target_link_libraries(echo_server ${LIB_LIB})

# add_executable(test_http_server tests/test_http_server.cc)
# add_dependencies(test_http_server chat)
# force_redefine_file_macro_for_sources(test_http_server) #__FILE__
# target_link_libraries(test_http_server ${LIB_LIB})

# add_executable(test_http_connection tests/test_http_connection.cc)
# add_dependencies(test_http_connection chat)
# force_redefine_file_macro_for_sources(test_http_connection) #__FILE__
# target_link_libraries(test_http_connection ${LIB_LIB})

# add_executable(test_uri tests/test_uri.cc)
# add_dependencies(test_uri chat)
# force_redefine_file_macro_for_sources(test_uri) #__FILE__
# target_link_libraries(test_uri ${LIB_LIB})

# add_executable(http_server examples/http_server.cc)
# add_dependencies(http_server chat)
# force_redefine_file_macro_for_sources(http_server) #__FILE__
# target_link_libraries(http_server ${LIB_LIB})

# add_executable(test_daemon tests/test_daemon.cc)
# add_dependencies(test_daemon chat)
# force_redefine_file_macro_for_sources(test_daemon) #__FILE__
# target_link_libraries(test_daemon ${LIB_LIB})

# add_executable(test_env tests/test_env.cc)
# add_dependencies(test_env chat)
# force_redefine_file_macro_for_sources(test_env) #__FILE__
# target_link_libraries(test_env ${LIB_LIB})

# add_executable(test_application tests/test_application.cc)
# add_dependencies(test_application chat)
# force_redefine_file_macro_for_sources(test_application) #__FILE__
# target_link_libraries(test_application ${LIB_LIB})

add_executable(main chat/main.cc)
add_dependencies(main chat)
force_redefine_file_macro_for_sources(main) #__FILE__
target_link_libraries(main ${LIB_LIB})

add_executable(echo_server_udp examples/echo_server_udp.cc)
add_dependencies(echo_server_udp chat)
force_redefine_file_macro_for_sources(echo_server_udp) #__FILE__
target_link_libraries(echo_server_udp ${LIB_LIB})

SET(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)
SET(LIBRARY_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/lib)