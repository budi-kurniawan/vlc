/*****************************************************************************
 * ZrleFrameMaker.hpp
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

#ifndef VLC_FBS_ZRLEFRAMEMAKER_H_
#define VLC_FBS_ZRLEFRAMEMAKER_H_

#include "FbsPixelFormat.hpp"
#include <sstream>
#include "zlib.h"
#include <vector>
#include <memory>

namespace fbs {
#define MAX_INFLATE_SIZE_ZLIB 128000

class ZrleFrameMaker {
public:
	uint16_t frameBufferWidth;
	uint16_t frameBufferHeight;
	uint8_t bytesPerPixel;
	uint8_t *p_rfb_buffer;
	int zrleTilePixels24[64 * 64]; // storage for tiles of 64 x 64 pixels, each pixel is of the form 00BBGGRR
	ZrleFrameMaker(uint16_t fbWidth, uint16_t fbHeight, FbsPixelFormat* fbsPixelFormat, uint8_t *p_rfb_buffer);
    void handleFrame(uint8_t *data);
    void reset();
    z_stream zStream;
    unsigned long inflated;

private:
    std::shared_ptr<std::vector<char>> inf(char*, long);
    void populateColorArray(std::stringstream &rectDataStream, int colorArray[], int paletteSize);
    void handleRawPixels(std::stringstream &rectDataStream, int tileWidth, int tileHeight);
    void handlePackedPixels(std::stringstream &rectDataStream, int tileWidth, int tileHeight, int colorArray[], int paletteSize);
    void handlePlainRLEPixels(std::stringstream &rectDataStream, int tileWidth, int tileHeight);
    void handlePackedRLEPixels(std::stringstream &rectDataStream, int tileWidth, int tileHeight, int colorArray[]);
    void handleUpdatedZrleTile(int j, int i, int tileWidth, int tileHeight);
    int readPixel(std::stringstream &rectDataStream);
};
} // namespace
#endif /* VLC_FBS_ZRLEFRAMEMAKER_H_ */
