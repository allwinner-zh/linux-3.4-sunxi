/*
 * sound\soc\sunxi\vir_audio\sunxi-vir_audiodma.c
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

#include <linux/module.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/dma-mapping.h>
#include <linux/dma/sunxi-dma.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/dmaengine_pcm.h>

#include <asm/dma.h>
#include <mach/hardware.h>
#include "ar200_audio.h"

static volatile unsigned int capture_dmasrc = 0;
static volatile unsigned int capture_dmadst = 0;
static volatile unsigned int play_dmasrc = 0;
static volatile unsigned int play_dmadst = 0;

struct virtual_runtime_data {
	struct audio_cb_t 	play_done_cb;
	struct audio_cb_t 	capture_done_cb;
	audio_mem_t			audio_mem;
	unsigned int 		pos;
	int 				mode;
};


static const struct snd_pcm_hardware sunxi_pcm_play_hardware = {
	.info			= SNDRV_PCM_INFO_INTERLEAVED | SNDRV_PCM_INFO_BLOCK_TRANSFER |
				      SNDRV_PCM_INFO_MMAP | SNDRV_PCM_INFO_MMAP_VALID |
				      SNDRV_PCM_INFO_PAUSE | SNDRV_PCM_INFO_RESUME,
	.formats		= SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S20_3LE | SNDRV_PCM_FMTBIT_S24_LE,
	.rates			= SNDRV_PCM_RATE_8000_192000 | SNDRV_PCM_RATE_KNOT,
	.rate_min		= 8000,
	.rate_max		= 192000,
	.channels_min		= 1,
	.channels_max		= 2,
	.buffer_bytes_max	= 128*1024,    /* value must be (2^n)Kbyte size */
	.period_bytes_min	= 1024,
	.period_bytes_max	= 1024*16,
	.periods_min		= 2,
	.periods_max		= 8,
	.fifo_size		= 128,
};

static const struct snd_pcm_hardware sunxi_pcm_capture_hardware = {
	.info			= SNDRV_PCM_INFO_INTERLEAVED | SNDRV_PCM_INFO_BLOCK_TRANSFER |
				      SNDRV_PCM_INFO_MMAP | SNDRV_PCM_INFO_MMAP_VALID |
				      SNDRV_PCM_INFO_PAUSE | SNDRV_PCM_INFO_RESUME,
	.formats		= SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S20_3LE | SNDRV_PCM_FMTBIT_S24_LE,
	.rates			= SNDRV_PCM_RATE_8000_192000 | SNDRV_PCM_RATE_KNOT,
	.rate_min		= 8000,
	.rate_max		= 192000,
	.channels_min		= 1,
	.channels_max		= 2,
	.buffer_bytes_max	= 128*1024,    /* value must be (2^n)Kbyte size */
	.period_bytes_min	= 1024,
	.period_bytes_max	= 1024*16,
	.periods_min		= 2,
	.periods_max		= 8,
	.fifo_size		= 128,
};

static void sunxi_play_perdone(int mode, void *parg)
{
	struct snd_pcm_substream *substream = parg;
	struct virtual_runtime_data *prtd = substream->runtime->private_data;

	prtd->pos += snd_pcm_lib_period_bytes(substream);
	if (prtd->pos >= snd_pcm_lib_buffer_bytes(substream)) {
		prtd->pos = 0;
	}
	snd_pcm_period_elapsed(substream);
}

static void sunxi_capture_perdone(int mode, void *parg)
{
	struct snd_pcm_substream *substream = parg;
	struct virtual_runtime_data *prtd = substream->runtime->private_data;

	prtd->pos += snd_pcm_lib_period_bytes(substream);
	if (prtd->pos >= snd_pcm_lib_buffer_bytes(substream)) {
		prtd->pos = 0;
	}
	snd_pcm_period_elapsed(substream);
}

//static void sunxi_set_perdone_fn(int mode, struct audio_cb_t *audio_cb)
//{
//	#if 0
//	pchan->hd_cb.func = audio_cb->func;
//	pchan->hd_cb.parg = audio_cb->parg;
//	#endif
//}

static int sunxi_pcm_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params)
{
	//int ret;
	unsigned long sample_rate 	= params_rate(params);
	unsigned long channels 		= params_channels(params);
	struct virtual_runtime_data *prtd = substream->runtime->private_data;

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
#if 1
		prtd->mode 						= AUDIO_PLAY;
		prtd->audio_mem.sram_base_addr 	= 0x00080000;
		prtd->audio_mem.buffer_size		= snd_pcm_lib_buffer_bytes(substream);
		prtd->audio_mem.period_size		= snd_pcm_lib_period_bytes(substream);
		
		/*���ò����ʣ�ͨ����*/
		sunxi_i2s_param(prtd->mode, sample_rate, channels);
		/*����sram�������ַ��buffer��С��period��С*/
		sunxi_buffer_period_param(prtd->mode, prtd->audio_mem);
		/*
	 	* set callback
	 	*/
		memset(&prtd->play_done_cb, 0, sizeof(prtd->play_done_cb));
		prtd->play_done_cb.func = sunxi_play_perdone;
		prtd->play_done_cb.parg = substream;
		sunxi_set_perdone_fn(prtd->mode, prtd->play_done_cb);

		snd_pcm_set_runtime_buffer(substream, &substream->dma_buffer);
#endif
	} else {
#if 1
		prtd->mode 						= AUDIO_CAPTURE;
		prtd->audio_mem.sram_base_addr 	= 0x00081000;
		prtd->audio_mem.buffer_size		= snd_pcm_lib_buffer_bytes(substream);
		prtd->audio_mem.period_size		= snd_pcm_lib_period_bytes(substream);
		
		/*���ò����ʣ�ͨ����*/
		sunxi_i2s_param(prtd->mode, sample_rate, channels);
		/*����sram�������ַ��buffer��С��period��С*/
		sunxi_buffer_period_param(prtd->mode, prtd->audio_mem);
		/*
	 	* set callback
	 	*/
		memset(&prtd->capture_done_cb, 0, sizeof(prtd->capture_done_cb));
		prtd->capture_done_cb.func = sunxi_capture_perdone;
		prtd->capture_done_cb.parg = substream;
		sunxi_set_perdone_fn(prtd->mode, prtd->capture_done_cb);

		snd_pcm_set_runtime_buffer(substream, &substream->dma_buffer);
#endif
	}

	return 0;
}

static int sunxi_pcm_hw_free(struct snd_pcm_substream *substream)
{
	snd_pcm_set_runtime_buffer(substream, NULL);
	
	return 0;
}

static snd_pcm_uframes_t sunxi_pcm_pointer(struct snd_pcm_substream *substream)
{
	unsigned long play_res = 0, capture_res = 0;
	struct virtual_runtime_data *prtd = NULL;
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		prtd = substream->runtime->private_data;
		
		sunxi_get_position(prtd->mode, (unsigned int*)&play_dmasrc, (unsigned int*)&play_dmadst);
		//play_res = play_dmasrc + prtd->dma_period - prtd->dma_start;
		if (play_res >= snd_pcm_lib_buffer_bytes(substream)) {
			if (play_res == snd_pcm_lib_buffer_bytes(substream))
				play_res = 0;
			}
		return bytes_to_frames(substream->runtime, play_res);
	} else {
		prtd = substream->runtime->private_data;
		sunxi_get_position(prtd->mode, (unsigned int*)&capture_dmasrc, (unsigned int*)&capture_dmadst);
		//capture_res = capture_dmadst + prtd->dma_period - prtd->dma_start;
		if (capture_res >= snd_pcm_lib_buffer_bytes(substream)) {
			if (capture_res == snd_pcm_lib_buffer_bytes(substream))
			capture_res = 0;
		}
		return bytes_to_frames(substream->runtime, capture_res);
	}
}

static int sunxi_pcm_trigger(struct snd_pcm_substream *substream, int cmd)
{
	struct virtual_runtime_data *prtd = NULL;
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		prtd = substream->runtime->private_data;
		switch (cmd) {
		case SNDRV_PCM_TRIGGER_START:
		case SNDRV_PCM_TRIGGER_RESUME:
		case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
			sunxi_audio_start(prtd->mode);
			pr_debug("%s,line :%d\n", __func__, __LINE__);
			return 0;
		case SNDRV_PCM_TRIGGER_SUSPEND:
		case SNDRV_PCM_TRIGGER_STOP:
		case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
			sunxi_audio_stop(prtd->mode);
			pr_debug("%s,line :%d\n", __func__, __LINE__);
			return 0;
		}
	} else {
		prtd = substream->runtime->private_data;
		switch (cmd) {
		case SNDRV_PCM_TRIGGER_START:
		case SNDRV_PCM_TRIGGER_RESUME:
		case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
			sunxi_audio_start(prtd->mode);
			pr_debug("%s,line :%d\n", __func__, __LINE__);
			return 0;
		case SNDRV_PCM_TRIGGER_SUSPEND:
		case SNDRV_PCM_TRIGGER_STOP:
		case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
			sunxi_audio_stop(prtd->mode);
			pr_debug("%s,line :%d\n", __func__, __LINE__);
			return 0;
		}
	}
	return 0;
}

static int sunxi_pcm_open(struct snd_pcm_substream *substream)
{	
	struct virtual_runtime_data *prtd;
	//struct device *dev = rtd->platform->dev;
	int ret = 0;
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		/* Set HW params now that initialization is complete */
		snd_soc_set_runtime_hwparams(substream, &sunxi_pcm_play_hardware);
		ret = snd_pcm_hw_constraint_integer(substream->runtime,
					    SNDRV_PCM_HW_PARAM_PERIODS);
		if (ret < 0)
			return ret;

		prtd = kzalloc(sizeof(*prtd), GFP_KERNEL);
		if (!prtd)
			return -ENOMEM;

		substream->runtime->private_data = prtd;

		return 0;		
	} else {
		/* Set HW params now that initialization is complete */
		snd_soc_set_runtime_hwparams(substream, &sunxi_pcm_capture_hardware);
	
		ret = snd_pcm_hw_constraint_integer(substream->runtime,
					    SNDRV_PCM_HW_PARAM_PERIODS);
		if (ret < 0)
			return ret;

		prtd = kzalloc(sizeof(*prtd), GFP_KERNEL);
		if (!prtd)
			return -ENOMEM;

		substream->runtime->private_data = prtd;
	}
	return 0;
}

static int sunxi_pcm_close(struct snd_pcm_substream *substream)
{
	struct virtual_runtime_data *prtd = substream->runtime->private_data;

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		kfree(prtd);
	} else {
		kfree(prtd);
	}
	return 0;
}

static struct snd_pcm_ops sunxi_pcm_ops = {
	.open			= sunxi_pcm_open,
	.close			= sunxi_pcm_close,
	.ioctl			= snd_pcm_lib_ioctl,
	.hw_params		= sunxi_pcm_hw_params,
	.hw_free		= sunxi_pcm_hw_free,
	.trigger		= sunxi_pcm_trigger,
	.pointer		= sunxi_pcm_pointer,
};

static int sunxi_pcm_preallocate_dma_buffer(struct snd_pcm *pcm, int stream)
{
	struct snd_pcm_substream *substream = pcm->streams[stream].substream;
	struct snd_dma_buffer *buf = &substream->dma_buffer;
	size_t size = 0;
	
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		size = sunxi_pcm_play_hardware.buffer_bytes_max;
	} else {
		size = sunxi_pcm_capture_hardware.buffer_bytes_max;
	}
	
	buf->dev.type = SNDRV_DMA_TYPE_DEV;
	buf->dev.dev = pcm->card->dev;
	buf->private_data = NULL;
	buf->area = dma_alloc_writecombine(pcm->card->dev, size,
					   &buf->addr, GFP_KERNEL);
	if (!buf->area)
		return -ENOMEM;
	buf->bytes = size;
	return 0;
}

static void sunxi_pcm_free_dma_buffers(struct snd_pcm *pcm)
{
	struct snd_pcm_substream *substream;
	struct snd_dma_buffer *buf;
	int stream;
	
	for (stream = 0; stream < 2; stream++) {
		substream = pcm->streams[stream].substream;
		if (!substream)
			continue;

		buf = &substream->dma_buffer;
		if (!buf->area)
			continue;

		dma_free_writecombine(pcm->card->dev, buf->bytes,
				      buf->area, buf->addr);
		buf->area = NULL;
	}
}

static u64 sunxi_pcm_mask = DMA_BIT_MASK(32);

static int sunxi_pcm_new(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_card *card = rtd->card->snd_card;
	struct snd_pcm *pcm = rtd->pcm;
		
	int ret = 0;
	
	if (!card->dev->dma_mask)
		card->dev->dma_mask = &sunxi_pcm_mask;
	if (!card->dev->coherent_dma_mask)
		card->dev->coherent_dma_mask = 0xffffffff;

	if (pcm->streams[SNDRV_PCM_STREAM_PLAYBACK].substream) {
		ret = sunxi_pcm_preallocate_dma_buffer(pcm,
			SNDRV_PCM_STREAM_PLAYBACK);
		if (ret)
			goto out;
	}

	if (pcm->streams[SNDRV_PCM_STREAM_CAPTURE].substream) {
		ret = sunxi_pcm_preallocate_dma_buffer(pcm,
			SNDRV_PCM_STREAM_CAPTURE);
		if (ret)
			goto out;
	}
 	out:
		return ret;
}

static struct snd_soc_platform_driver sunxi_soc_platform = {
	.ops		= &sunxi_pcm_ops,
	.pcm_new	= sunxi_pcm_new,
	.pcm_free	= sunxi_pcm_free_dma_buffers,
};

static int __init sunxi_vir_audio_pcm_probe(struct platform_device *pdev)
{
	return snd_soc_register_platform(&pdev->dev, &sunxi_soc_platform);
}

static int __exit sunxi_vir_audio_pcm_remove(struct platform_device *pdev)
{
	snd_soc_unregister_platform(&pdev->dev);
	return 0;
}

/*data relating*/
static struct platform_device sunxi_vir_audio_pcm_device = {
	.name = "sunxi-vir_audio-pcm-audio",
};

/*method relating*/
static struct platform_driver sunxi_vir_audio_pcm_driver = {
	.probe = sunxi_vir_audio_pcm_probe,
	.remove = __exit_p(sunxi_vir_audio_pcm_remove),
	.driver = {
		.name = "sunxi-vir_audio-pcm-audio",
		.owner = THIS_MODULE,
	},
};

static int __init sunxi_soc_platform_vir_audio_init(void)
{
	int err = 0;
	if((err = platform_device_register(&sunxi_vir_audio_pcm_device)) < 0)
		return err;

	if ((err = platform_driver_register(&sunxi_vir_audio_pcm_driver)) < 0)
		return err;
	return 0;	
}
module_init(sunxi_soc_platform_vir_audio_init);

static void __exit sunxi_soc_platform_vir_audio_exit(void)
{
	return platform_driver_unregister(&sunxi_vir_audio_pcm_driver);
}
module_exit(sunxi_soc_platform_vir_audio_exit);

MODULE_AUTHOR("huangxin");
MODULE_DESCRIPTION("SUNXI I2S DMA module");
MODULE_LICENSE("GPL");

