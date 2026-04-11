cmake_minimum_required(VERSION 3.16)
project(locus LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)

add_executable(main
	src/io/msg_io.c
	src/client/client.c
	src/client/input/input.c
	src/client/rendering/render.c
	src/server/server.c
)

target_include_directories(main
	src/protocol
	src/io
	src/client
	src/client/input
	src/client/rendering
	src/server
)

target_link_options(main PRIVATE
	-lws2_32
)
