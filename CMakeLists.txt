cmake_minimum_required(VERSION 3.16)
project(locus LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)

add_executable(main
	src/io/msg_io.c
	src/client/client.c
	src/server/server.c
)

target_include_directories(main
	src/protocol
	src/io
	src/client
	src/server
)

target_link_options(main PRIVATE
	-lws2_32
)
