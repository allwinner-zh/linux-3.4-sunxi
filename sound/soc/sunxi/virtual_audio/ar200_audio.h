/*
 * sound\soc\sunxi\i2s0\sunxi-i2s0dma.h
 * (C) Copyright 2010-2016
 * Reuuimlla Technology Co., Ltd. <www.reuuimllatech.com>
 * huangxin <huangxin@Reuuimllatech.com>
 *
 * some simple description for this code
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 */

#ifndef SUNXI_VIR_AUDIODMA_H_
#define SUNXI_VIR_AUDIODMA_H_

#undef VIR_AUDIO_DBG
#if (1)
    #define VIR_AUDIO_DBG(format,args...)  printk("[VIR_AUDIO] "format,##args)    
#else
    #define VIR_AUDIO_DBG(...)
#endif

/* sunxi_perdone_cbfn
 *
 * period done callback routine type
*/
typedef void (*sunxi_perdone_cbfn)(int mode, void *buf);

enum audio_mode {
	AUDIO_PLAY,
	AUDIO_CAPTURE
};

/* audio callback struct */
struct audio_cb_t {
	sunxi_perdone_cbfn	func;	/* dma callback fuction */
	void 				*parg;	/* args of func */
};

typedef struct sunxi_config_audio_mem_t {
	unsigned int sram_base_addr;
	unsigned int buffer_size;
	unsigned int period_size;
}audio_mem_t;

static int sunxi_audio_start(int mode)
{
	return 0;
}

static int sunxi_audio_stop(int mode)
{
	return 0;
}

static int sunxi_buffer_period_param(int mode, audio_mem_t audio_mem)
{
	return 0;
}

static int sunxi_get_position(int mode, unsigned int *pSrc, unsigned int *pDst)
{
	return 0;
}

static int sunxi_set_perdone_fn(int mode, struct audio_cb_t cb_t)
{
	return 0;
}

static int sunxi_i2s_param(int mode, int samplerate, int channel)
{
	return 0;
}


#endif
