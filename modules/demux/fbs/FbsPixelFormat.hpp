/*****************************************************************************
 * FbsPixelFormat.hpp
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

#ifndef FBS_PIXELS_FORMAT_H_
#define FBS_PIXELS_FORMAT_H_
namespace fbs {
struct FbsPixelFormat {
	char bitsPerPixel;
	char depth;
	char bigEndianFlag;
	char trueColorFlag;
	short redMax;
	short greenMax;
	short blueMax;
	char redShift;
	char greenShift;
	char blueShift;
};
} // namespace
#endif /* FBS_PIXELS_FORMAT_H_ */
