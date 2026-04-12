#pragma once

#include "../../dependencies/zstd/zstd.h"

#define COMPRESSION_LEVEL 11

//--================
// -- API
//--================

/// @brief Compress data using zstd
/// @param src: Source of input
/// @param srcSize: Size of source
/// @param dst: Destination buffer
/// @param dstCapacity: Size of destination
/// @return size_t: Size of compressed data
size_t compress_data(const void* src, size_t srcSize, void* dst, size_t dstCapacity);

/// @brief Decompress data using zstd
/// @param dst: Source of input
/// @param dstCapacity: Size of source
/// @param src: Destination buffer
/// @param srcSize: Size of destination
/// @reutrn size_t: Size of decompressed data
size_t decompressed_data(void* dst, size_t dstCapacity, const void* src, size_t srcSize);
