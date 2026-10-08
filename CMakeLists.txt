cmake_minimum_required(VERSION 3.16)
project(locus LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(WIN32)
	find_package(zstd CONFIG REQUIRED)
	set(LOCUS_ZSTD_TARGET zstd::libzstd_static)
	set(LOCUS_PLATFORM_LIBRARIES ws2_32 bcrypt)
else()
	find_package(OpenSSL REQUIRED COMPONENTS Crypto)
	find_package(Threads REQUIRED)
	find_package(PkgConfig REQUIRED)
	pkg_check_modules(ZSTD REQUIRED IMPORTED_TARGET libzstd)
	set(LOCUS_ZSTD_TARGET PkgConfig::ZSTD)
	set(LOCUS_PLATFORM_LIBRARIES OpenSSL::Crypto Threads::Threads)
endif()

add_executable(locus_server
	src/parser/message_parser.cpp

	src/keys/key_exchange.c

	src/compression/compress.c
	src/encryption/encrypt.c

	main/server/server.c
	src/io/msg_io.c
	src/server/server.c
	src/server/db/msg_db.cpp

	src/client/rendering/render.c
)

target_include_directories(locus_server PRIVATE
	src/parser
	src/keys
	src/utils
	src/compression
	src/encryption
	src/protocol
	src/io
	src/server
	src/server/db

	src/client/rendering
)

target_link_libraries(locus_server PRIVATE ${LOCUS_PLATFORM_LIBRARIES} ${LOCUS_ZSTD_TARGET})

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

target_link_libraries(locus_client PRIVATE ${LOCUS_PLATFORM_LIBRARIES} ${LOCUS_ZSTD_TARGET})
