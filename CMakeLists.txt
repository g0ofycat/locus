# build/ cmake .. -DCMAKE_TOOLCHAIN_FILE=C:/Users/clint/vcpkg/scripts/buildsystems/vcpkg.cmake -DVCPKG_TARGET_TRIPLET=x64-mingw-static

cmake_minimum_required(VERSION 3.16)
project(locus LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(zstd CONFIG REQUIRED)

add_executable(locus_server
	src/keys/key_exchange.c

	src/compression/compress.c
	src/encryption/encrypt.c

	main/server/server.c
	src/io/msg_io.c
	src/server/server.c
)

target_include_directories(locus_server PRIVATE
	src/keys
	src/utils
	src/compression
	src/encryption
	src/protocol
	src/io
	src/server
)

target_link_libraries(locus_server PRIVATE ws2_32 bcrypt zstd::libzstd_static)

add_executable(locus_client
	src/parser/message_parser.cpp
	src/keys/key_exchange.c

	src/compression/compress.c
	src/encryption/encrypt.c

	main/client/client.c
	src/io/msg_io.c
	src/client/client.c
	src/client/input/input.c
	src/client/rendering/render.c
)

target_include_directories(locus_client PRIVATE
	src/parser
	src/keys
	src/utils
	src/compression
	src/encryption
	src/protocol
	src/io
	src/client
	src/client/input
	src/client/rendering
)

target_link_libraries(locus_client PRIVATE ws2_32 bcrypt zstd::libzstd_static)
