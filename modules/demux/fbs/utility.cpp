/*****************************************************************************
 * utility.cpp
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

#include "utility.hpp"

namespace fbs {
	uint16_t numerize2(const uint8_t *data, int startIndex) {
		return data[startIndex]*256 + data[startIndex+1];
	}
	uint32_t numerize4(const uint8_t *data, int startIndex) {
		return data[startIndex]*256*256*256 + data[startIndex+1]*256*256
				+ data[startIndex+2]*256 + data[startIndex+3];
	}
	uint8_t readU8(stringstream &stringstream) {
		char c_array[1];
		stringstream.readsome(c_array, 1);
		return static_cast<uint8_t>(c_array[0]);
	}
} // namespace
