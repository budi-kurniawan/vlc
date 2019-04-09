/*****************************************************************************
 * fbs.cpp : fbs demuxer
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

#include <iostream>
#include <new>
#include <inttypes.h>
#include <vlc_common.h>
#include <vlc_plugin.h>
#include <time.h>
#include <vlc_meta.h>
#include <vlc_charset.h>
#include <vlc_input.h>
#include <vlc_demux.h>
#include <vlc_aout.h> /* For reordering */
#include <iostream>
#include <cassert>
#include <typeinfo>
#include <string>
#include <vector>
#include <algorithm>
#include <map>
#include <stdexcept>
#include "FbsPixelFormat.hpp"
#include "utility.hpp"
#include "ZrleFrameMaker.hpp"
#include <chrono>
#include <ctime>
#include <fcntl.h>
#include <vlc_fs.h>
#include <vlc_url.h>

/** Initial memory alignment of data block.
 * @note This must be a multiple of sizeof(void*) and a power of two.
 * libavcodec AVX optimizations require at least 32-bytes. */
#define BLOCK_ALIGN        32

/** Initial reserved header and footer size. */
#define BLOCK_PADDING      32
/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
namespace fbs {
static int  Open(vlc_object_t *);
static void Close(vlc_object_t *);
static int Seek(demux_t *, vlc_tick_t);
} // namespace

using namespace fbs;
using namespace std;
vlc_module_begin ()
    set_shortname( "FBS" )
    set_description( N_("FBS stream demuxer" ) )
    set_capability( "demux", 212 )
    set_callbacks( Open, Close )
    set_category( CAT_INPUT )
    set_subcategory( SUBCAT_INPUT_DEMUX )
vlc_module_end ()

namespace fbs {
/*****************************************************************************
 * Definitions of structures used by this plugin
 *****************************************************************************/
typedef struct
{
    int    frame_size;
    es_out_id_t *p_es_video;
    es_format_t  fmt_video;
    date_t pcr;
} demux_sys_t;

static char rfbVersion;
static uint16_t frameBufferWidth;
static uint16_t frameBufferHeight;
static FbsPixelFormat *fbsPixelFormat;
static int framesPerSecond = 5;
static vlc_tick_t pos = 0;
static vlc_tick_t headerPos = 0;
static int dim = 0;
static vlc_tick_t soughtTimestamp = 0;
static ZrleFrameMaker *zrleFrameMaker;
static block_t     *p_block;
static uint8_t *p_rfb_buffer;
static vlc_tick_t timestamp = 0;
static vlc_tick_t lastTimestamp = 0;
static int Demux  ( demux_t * );
static int Control( demux_t *, int, va_list );
static int readLastTimestamp(char *);
static bool seeking = false;
/*****************************************************************************
 * Open: initializes FBS demux structures
 *****************************************************************************/
static int Open(vlc_object_t * p_this) {
    demux_t 			*p_demux = (demux_t*)p_this;
    demux_sys_t        	*p_sys;
    const vlc_fourcc_t i_chroma = VLC_CODEC_RGB24;
    const uint8_t i_sar_num = 1;
    const uint8_t i_sar_den = 1;
    const uint8_t *p_peek;

    if (lastTimestamp == 0) {
        lastTimestamp = readLastTimestamp(p_demux->psz_filepath);
    }
    p_demux->p_sys	= p_sys = (demux_sys_t*) malloc(sizeof(demux_sys_t));
    int readBytes = vlc_stream_Peek(p_demux->s, &p_peek, 2000);
    if (readBytes == -1) {
    	return VLC_EGENERIC;
    }
    if (strncmp((char*) &p_peek[0], "FBS 001.000", 11)) {
    	// file invalid
    	return VLC_EGENERIC;
    }
    string header((char*) &p_peek[0], 2000);

	fbsPixelFormat = new FbsPixelFormat();
    pos = 12;
    for (int frameNo = 1; frameNo < 6; frameNo++) {
    	int dataLength = numerize4(p_peek, pos);
    	string data = header.substr(pos + 4, dataLength);
    	int paddedDataLength = 4 * ((dataLength + 3) / 4);
    	if (frameNo == 1) {
    		rfbVersion = p_peek[pos+14] - 48; // rfbVersion is either 3 or 8
    	}
		if (frameNo == 3 && rfbVersion == 3) {
			// no security result for RFB 3.3, skip
			continue;
		}
    	if (frameNo == 4) {
    		frameBufferWidth = numerize2(p_peek, pos+4);
    		frameBufferHeight = numerize2(p_peek, pos+6);
    		fbsPixelFormat->bitsPerPixel = p_peek[pos+8];
    		fbsPixelFormat->depth = p_peek[pos+9];
    		fbsPixelFormat->bigEndianFlag = p_peek[pos+10];
    		fbsPixelFormat->trueColorFlag = p_peek[pos+11];
    		fbsPixelFormat->redMax = numerize2(p_peek, pos+12);
    		fbsPixelFormat->greenMax = numerize2(p_peek, pos+14);
    		fbsPixelFormat->blueMax = numerize2(p_peek, pos+16);
    		fbsPixelFormat->redShift = p_peek[pos+18];
    		fbsPixelFormat->greenShift = p_peek[pos+19];
    		fbsPixelFormat->blueShift = p_peek[pos+20];
    	}
    	pos += 4 + paddedDataLength + /* timestamp */ 4;
    	headerPos = pos; // used by SEEK
    }

	if (vlc_stream_Peek(p_demux->s, &p_peek, 11) == 11) {
		if (strncmp((char*) p_peek, "FBS 001.000", 11)) {
			cout << "File invalid\n";
		}
	}
    /* Set the demux function */
    es_format_Init( &p_sys->fmt_video, VIDEO_ES, i_chroma );
    video_format_Setup( &p_sys->fmt_video.video, i_chroma,
                        frameBufferWidth, frameBufferHeight,
						frameBufferWidth, frameBufferHeight,
                        i_sar_num, i_sar_den );

    date_Init( &p_sys->pcr, p_sys->fmt_video.video.i_frame_rate,
               p_sys->fmt_video.video.i_frame_rate_base);
    date_Set( &p_sys->pcr, VLC_TICK_0 );

    const vlc_chroma_description_t *dsc =
            vlc_fourcc_GetChromaDescription(p_sys->fmt_video.video.i_chroma);
    p_sys->frame_size = 0; // need to set to 0
    for (unsigned i=0; i<dsc->plane_count; i++) {
        unsigned pitch = (frameBufferWidth + (dsc->p[i].w.den - 1))
                         * dsc->p[i].w.num / dsc->p[i].w.den * dsc->pixel_size;
        unsigned lines = (frameBufferHeight + (dsc->p[i].h.den - 1))
                         * dsc->p[i].h.num / dsc->p[i].h.den;
        p_sys->frame_size += pitch * lines;
    }
    p_sys->p_es_video = es_out_Add( p_demux->out, &p_sys->fmt_video);
    p_demux->pf_demux   = Demux;
    p_demux->pf_control = Control;

    dim = frameBufferWidth * frameBufferHeight * 3;
    p_rfb_buffer = (uint8_t*) malloc(dim);
    zrleFrameMaker = new ZrleFrameMaker(frameBufferWidth, frameBufferHeight, fbsPixelFormat, p_rfb_buffer);
    // skip to data
	readBytes = vlc_stream_Read(p_demux->s, NULL, pos);
    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close: frees unused data
 *****************************************************************************/
static void Close( vlc_object_t *p_this ) {
    demux_t *p_demux = (demux_t*) p_this;
    demux_sys_t *p_sys  = (demux_sys_t*) p_demux->p_sys;
    delete p_sys;
    delete p_rfb_buffer;
    delete p_block;
    delete fbsPixelFormat;
    delete zrleFrameMaker;
}

/*****************************************************************************
 * Control:
 *****************************************************************************/
static int Control( demux_t *p_demux, int i_query, va_list args) {
    const int64_t i_bps = 8LL * dim;
    double f, *pf;

    switch (i_query) {
    case DEMUX_GET_LENGTH:
        //assign a number in microseconds: timestamp (in ms) * 1000
        *va_arg(args, vlc_tick_t *) = lastTimestamp * 1000;
        return VLC_SUCCESS;
    case DEMUX_CAN_SEEK:
        *va_arg(args, bool *) = true;
        return VLC_SUCCESS;
    case DEMUX_GET_TIME:
        *va_arg(args, vlc_tick_t *) = timestamp * 1000;
        return VLC_SUCCESS;
    case DEMUX_GET_POSITION:
        pf = va_arg( args, double * );
        *pf = (double) timestamp / (double) lastTimestamp;
        return VLC_SUCCESS;
    case DEMUX_SET_POSITION:
        f = va_arg(args, double);
        vlc_tick_t i64 = f * 1000;
		return Seek(p_demux, i64);
    }
    return demux_vaControlHelper(p_demux->s, 0, -1, i_bps, dim, i_query, args);
}

/*****************************************************************************
 * Demux: reads and demuxes data packets
 *****************************************************************************
 * Returns -1 in case of error, 0 in case of EOF, 1 otherwise
 *****************************************************************************/
static int Demux(demux_t *p_demux) {
	demux_sys_t *p_sys  = (demux_sys_t *) p_demux->p_sys;
    vlc_tick_t i_pcr = date_Get( &p_sys->pcr );
    es_out_SetPCR( p_demux->out, i_pcr);

    while (timestamp <= i_pcr / 1000 && !seeking) {
    	p_block = vlc_stream_Block(p_demux->s, 4);
        if (p_block == NULL) {
        	timestamp = 0;
            return VLC_DEMUXER_EOF;
        }

    	int dataLength = numerize4(p_block->p_buffer, 0);
    	int paddedDataLength = 4 * ((dataLength + 3) / 4);
    	p_block = vlc_stream_Block( p_demux->s, paddedDataLength + /*timestamp*/ 4);
        p_block->i_size = p_sys->frame_size + BLOCK_ALIGN + 2 * BLOCK_PADDING;
        p_block->i_buffer = p_sys->frame_size;
    	timestamp = numerize4(p_block->p_buffer, paddedDataLength);
        zrleFrameMaker->handleFrame(p_block->p_buffer);
        p_block->p_buffer = p_rfb_buffer;
        p_block->i_dts = p_block->i_pts = i_pcr;
        es_out_Send( p_demux->out, p_sys->p_es_video, p_block );
    }
    soughtTimestamp = 0;
    p_sys->pcr.i_divider_num = framesPerSecond;//how many times in a second Demux() is called
    p_sys->pcr.i_divider_den = 1;

    date_Increment(&p_sys->pcr, 1);
    return VLC_DEMUXER_SUCCESS;
}

static int Seek(demux_t *p_demux, vlc_tick_t i_date) {
	demux_sys_t *p_sys  = (demux_sys_t *) p_demux->p_sys;
	soughtTimestamp = i_date * lastTimestamp / 1000;
	// Rewind to right after the header
    int readBytes = vlc_stream_Seek(p_demux->s, headerPos);
    if (readBytes == -1) {
    	return VLC_DEMUXER_EOF;
    }
	timestamp = 0;
	seeking = true;
    p_sys->pcr.i_divider_num = 0;//framesPerSecond;//how many times in a second Demux() is called
    zrleFrameMaker->reset();

    timestamp = 0;
    while (timestamp <= soughtTimestamp) {
    	p_block = vlc_stream_Block(p_demux->s, 4);
        if (p_block == NULL) {
        	timestamp = 0;
            return VLC_DEMUXER_EOF;
        }

    	int dataLength = numerize4(p_block->p_buffer, 0);
    	int paddedDataLength = 4 * ((dataLength + 3) / 4);
    	p_block = vlc_stream_Block(p_demux->s, paddedDataLength + /*timestamp*/ 4);
        p_block->i_size = p_sys->frame_size + BLOCK_ALIGN + 2 * BLOCK_PADDING;
        p_block->i_buffer = p_sys->frame_size;
    	timestamp = numerize4(p_block->p_buffer, paddedDataLength);
        zrleFrameMaker->handleFrame(p_block->p_buffer);
        p_block->p_buffer = p_rfb_buffer;
        p_block->i_dts = p_block->i_pts = timestamp * 1000;//i_pcr;
        es_out_Send( p_demux->out, p_sys->p_es_video, p_block );
    }
    seeking = false;
    p_sys->pcr.date = soughtTimestamp * 1000;
    soughtTimestamp = 0;
    return VLC_SUCCESS;
}

int readLastTimestamp(char* filepath) {
	int fd;
	unsigned char c[4]; // read last 4 bytes
	fd = open(filepath, O_RDONLY);
	if (fd == -1) {
		printf("Error opening file\n");
		return -1;
	}
	lseek(fd, -4L, SEEK_END);
	int readBytes = read(fd, c, 4); // Read 4 bytes
	close(fd);
	if (readBytes == 4) {
		return numerize4(c, 0);
	} else {
		return -1;
	}
}
} // namespace
