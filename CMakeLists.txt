cmake_minimum_required(VERSION 3.16)

project(locus LANGUAGES C)
set(CMAKE_C_STANDARD 11)

add_executable(locus_server
	main/server/server.c
	src/io/msg_io.c
	src/server/server.c
)
target_include_directories(locus_server PRIVATE
	src/protocol
	src/io
	src/server
)

target_link_libraries(locus_server PRIVATE ws2_32)

add_executable(locus_client
	main/client/client.c
	src/io/msg_io.c
	src/client/client.c
	src/client/input/input.c
	src/client/rendering/render.c
)
target_include_directories(locus_client PRIVATE
	src/protocol
	src/io
	src/client
	src/client/input
	src/client/rendering
)

target_link_libraries(locus_client PRIVATE ws2_32)
