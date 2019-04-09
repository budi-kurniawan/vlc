/*****************************************************************************
 * ZrleFrameMaker.cpp
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

#include "ZrleFrameMaker.hpp"
#include "FbsPixelFormat.hpp"
#include "utility.hpp"
#include <iostream>

namespace fbs {
using namespace fbs;

ZrleFrameMaker::ZrleFrameMaker(uint16_t fbWidth, uint16_t fbHeight,
		FbsPixelFormat *fbsPixelFormat, uint8_t *buffer) {
	frameBufferWidth = fbWidth;
	frameBufferHeight = fbHeight;
	bytesPerPixel = fbsPixelFormat->bitsPerPixel / 8;
	p_rfb_buffer = buffer;
	reset();
}

void ZrleFrameMaker::handleFrame(uint8_t *data) {
    int rectCount = numerize2(data, 2);
    int pos = 4;
    for (int i = 0; i < rectCount; i++) {
		uint16_t rectX = numerize2(data, pos);
		pos += 2;
		uint16_t rectY = numerize2(data, pos);
		pos += 2;
		uint16_t rectWidth = numerize2(data, pos);
		pos += 2;
		uint16_t rectHeight = numerize2(data, pos);
		pos += 2;
		//int encoding = numerize4(data, pos);
		pos += 4; // skip encoding
		int rectDataLength = numerize4(data, pos);
		pos += 4;
		if (rectDataLength > 67108864) {
			// Something horrible happened
			printf("rectDataLength too long\n");
		}
		char rectData[rectDataLength];
		for (int j = 0; j < rectDataLength; j++) {
			rectData[j] = data[j+pos];
		}
		pos += rectDataLength;
		shared_ptr<vector<char>> decompressedData = inf(rectData, rectDataLength);
		char* decompressedDataData = (char*) decompressedData->data();

		string s(decompressedDataData, decompressedData->size());
		std::stringstream rectDataStream;
		rectDataStream << s;
		for (int i = rectY; i < rectY + rectHeight; i += 64) {
			// last row tiles may have height that is < 64
			int tileHeight = min(64, rectY + rectHeight - i);
			for (int j = rectX; j < rectX + rectWidth; j += 64) {
				// last column tiles may have width that is < 64
				int tileWidth = min(64, rectX + rectWidth - j);
				// the first byte of the data contains subencoding
				uint8_t subencoding = readU8(rectDataStream);
				bool runLength = (subencoding & 0x80) != 0;
				int paletteSize = subencoding & 0x7F;

				int colorArray[128];
				populateColorArray(rectDataStream, colorArray, paletteSize);
				// read palette colors
				if (paletteSize == 1) {
					int colorCode = colorArray[0];
					for (int d = i; d < i + tileHeight; d++) {
						for (int e = j; e < j + tileWidth; e++) {
							int index = (d * frameBufferWidth + e) * 3;
							p_rfb_buffer[index + 2] = colorCode & 255;        //red;
							p_rfb_buffer[index + 1] = (colorCode >> 8) & 255; //green;
							p_rfb_buffer[index]     = (colorCode >> 16) & 255;//blue;
						}
					}
					continue;
				}
				if (!runLength) {
					if (paletteSize == 0) {
						handleRawPixels(rectDataStream, tileWidth, tileHeight);
					} else {
						handlePackedPixels(rectDataStream, tileWidth, tileHeight, colorArray, paletteSize);
					}
				} else {
					if (paletteSize == 0) {
						handlePlainRLEPixels(rectDataStream, tileWidth, tileHeight);
					} else {
						handlePackedRLEPixels(rectDataStream, tileWidth, tileHeight, colorArray);
					}
				}
				// copy tile content to pixels24
				handleUpdatedZrleTile(j, i, tileWidth, tileHeight);
			}
		}
    }
}

void ZrleFrameMaker::reset() {
    inflated = zStream.avail_in = 0;
    zStream.zalloc = Z_NULL;
    zStream.zfree = Z_NULL;
    zStream.opaque = Z_NULL;
    zStream.next_in = Z_NULL;
    inflateInit(&zStream);
}

void ZrleFrameMaker::populateColorArray(std::stringstream &rectDataStream, int colorArray[], int paletteSize) {
	switch (bytesPerPixel) {
	case 1: // not supported
		break;
	case 2: // not supported
		break;
	case 4:
		int totalSize = paletteSize * 3;
		uint8_t byteArray[totalSize];
		for (int i = 0; i < totalSize; i++) {
			byteArray[i] = readU8(rectDataStream);
		}
		for (int i = 0; i < paletteSize; ++i) {
			colorArray[i] = (byteArray[i * 3 + 2] & 255) << 16
					| (byteArray[i * 3 + 1] & 255) << 8
					| (byteArray[i * 3] & 255);
		}
		break;
	}
}

void ZrleFrameMaker::handleRawPixels(std::stringstream &rectDataStream, int tileWidth, int tileHeight) {
	populateColorArray(rectDataStream, zrleTilePixels24, tileWidth * tileHeight);
}

void ZrleFrameMaker::handlePackedPixels(std::stringstream &rectDataStream,
		int tileWidth, int tileHeight, int colorArray[], int paletteSize) {
    int shift = paletteSize > 16 ? 8 : (paletteSize > 4 ? 4 : (paletteSize > 2 ? 2 : 1));
    int pointer = 0;
    for (int i = 0; i < tileHeight; ++i) {
        int end = pointer + tileWidth;
        uint8_t counter1;
        int counter2 = 0;
        while (pointer < end) {
            if (counter2 == 0) {
            	counter1 = readU8(rectDataStream);
                counter2 = 8;
            }
            counter2 -= shift;
            uint8_t colorIndex = (counter1 >> counter2) & ((1 << shift) - 1);
            zrleTilePixels24[pointer++] = colorArray[colorIndex];
        }
    }
}

void ZrleFrameMaker::handlePlainRLEPixels(std::stringstream &rectDataStream, int tileWidth, int tileHeight) {
	uint16_t ptr = 0;
	uint16_t end = tileWidth * tileHeight;
    while (ptr < end) {
        uint8_t length;
        int pixel = readPixel(rectDataStream);
        uint16_t totalLength = 1;
        do {
        	length = readU8(rectDataStream);
            totalLength += length;
        } while (length == 255);
        if (totalLength > end - ptr) {
            printf("ZRLE decoder: assertion failed (len <= end-ptr). length: %d, end:%d, ptr:%d\n", totalLength, end, ptr);
        }
        while (totalLength-- > 0) {
            zrleTilePixels24[ptr++] = pixel;
        }
    }
}

void ZrleFrameMaker::handlePackedRLEPixels(std::stringstream &rectDataStream,
		int tileWidth, int tileHeight, int colorArray[]) {
	uint32_t pointer = 0;
    uint32_t end = tileWidth * tileHeight;
    while (pointer < end) {
        uint8_t flag = readU8(rectDataStream);
        uint32_t totalLength = 1;
        if ((flag & 128) != 0) {
            uint8_t length;
            do {
                length = readU8(rectDataStream);
                totalLength += length;
            } while (length == 255);
            if (totalLength > end - pointer) {
                printf("ZRLE decoder: assertion failed (len > end-ptr). length: %d, end:%d, ptr:%d\n", totalLength, end, pointer);
            }
        }
        int pixel = colorArray[flag &= 127];
        while (totalLength-- != 0) {
            zrleTilePixels24[pointer++] = pixel;
        }
    }
}

void ZrleFrameMaker::handleUpdatedZrleTile(int x, int y, int tileWidth, int tileHeight) {
    switch (bytesPerPixel) {
    case 1: // not supported
    	break;
    case 2: // not supported
    	break;
    case 4:
		for (int i = 0; i < tileHeight; i++) {
			for (int j = 0; j < tileWidth; j++) {
				int pixel = zrleTilePixels24[i * tileWidth + j];
				int position = ((y + i) * frameBufferWidth + (x + j)) * 3;
				p_rfb_buffer[position + 2] = pixel & 255;        //red;
				p_rfb_buffer[position + 1] = (pixel >> 8) & 255; //green;
				p_rfb_buffer[position] = (pixel >> 16) & 255;    //blue;
			}
		}
    }
}

int ZrleFrameMaker::readPixel(std::stringstream &rectDataStream) {
    int pixel = 0;
    switch (bytesPerPixel) {
    case 1: // not supported
    	break;
    case 2: // not supported
    	break;
    case 4:
    	uint8_t red = readU8(rectDataStream);
    	uint8_t green = readU8(rectDataStream);
    	uint8_t blue = readU8(rectDataStream);
        pixel = (blue & 255) << 16 | (green & 255) << 8 | (red & 255);
    	break;
    }
    return pixel;
}

std::shared_ptr<std::vector<char>> ZrleFrameMaker::inf(char* input, long inputLength) {
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
