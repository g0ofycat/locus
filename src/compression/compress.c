#include "compress.h"

#include <stdlib.h>
#include <string.h>

//--============
// -- PUBLIC
//--============

/// @brief Compress data using zstd
/// @param src: Source of input
/// @param srcSize: Size of source
/// @param dst: Destination buffer
/// @param dstCapacity: Size of destination
/// @return size_t: Size of compressed data, 0 on error
size_t compress_data(const void *src, size_t srcSize, void *dst, size_t dstCapacity)
{
	if (!src || !dst) return 0;
	size_t result = ZSTD_compress(dst, dstCapacity, src, srcSize, COMPRESSION_LEVEL);
	return ZSTD_isError(result) ? 0 : result;
}

/// @brief Decompress data using zstd
/// @param dst: Source of input
/// @param dstCapacity: Size of source
/// @param src: Destination buffer
/// @param srcSize: Size of destination
/// @return size_t: Size of decompressed data, 0 on error
size_t decompressed_data(void *dst, size_t dstCapacity, const void *src, size_t srcSize)
{
	if (!src || !dst) return 0;
	size_t result = ZSTD_decompress(dst, dstCapacity, src, srcSize);
	return ZSTD_isError(result) ? 0 : result;
}
