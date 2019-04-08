/*****************************************************************************
 * ZlibInStream.cpp
 *****************************************************************************
 * Copyright (C) 2019 OrecX LLC
 *
 * Author: Budi Kurniawan <bkurniawan@orecx.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "ZlibStream.hpp"

namespace fbs {
ZlibStream::ZlibStream() {
    reset();
}

void ZlibStream::reset() {
    inflated = zStream.avail_in = 0;
    zStream.zalloc = Z_NULL;
    zStream.zfree = Z_NULL;
    zStream.opaque = Z_NULL;
    zStream.next_in = Z_NULL;
    inflateInit(&zStream);
}

std::shared_ptr<std::vector<char>> ZlibStream::inf(char* input, long inputLength) {
	std::shared_ptr<std::vector<char>> decompressedData = std::shared_ptr<std::vector<char>>(new std::vector<char>());
	zStream.avail_in = inputLength;
    zStream.next_in = (Bytef*) input;
    char chunkData[MAX_INFLATE_SIZE_ZLIB];
    while(1) {
        zStream.avail_out = MAX_INFLATE_SIZE_ZLIB;
        zStream.next_out = (Bytef*) chunkData;
        int inflateResult = inflate(&zStream, Z_NO_FLUSH);
        unsigned int numBytesLeft = zStream.total_out - inflated;
        for (unsigned int i = 0; i < numBytesLeft; i++) {
            decompressedData->push_back(chunkData[i]);
        }
        inflated = zStream.total_out;
        if (inflateResult != Z_OK || zStream.avail_in == 0
        		|| (zStream.avail_out != 0 && inflateResult == Z_STREAM_END)) {
            break;
        }
    }
    return decompressedData;
}
} // namespace
