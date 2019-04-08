/*****************************************************************************
 * ZliInStream.hpp
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

#ifndef VLC_FBS_ZLIBINSTREAM_H_
#define VLC_FBS_ZLIBINSTREAM_H_

#include "zlib.h"
#include <vector>
#include <memory>

namespace fbs {
#define MAX_INFLATE_SIZE_ZLIB 128000

class ZlibStream {
    z_stream zStream;
    unsigned long inflated;

public:
    ZlibStream();
    std::shared_ptr<std::vector<char>> inf(char*, long);
    void reset();
};
} // namespace

#endif /* ZLIBINSTREAM_H_ */
