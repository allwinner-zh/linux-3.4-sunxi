/*
 * sound\soc\sunxi\vir_audio\sunxi_sndvir_audio.c
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
#include <linux/clk.h>
#include <linux/mutex.h>

#include <sound/pcm.h>
#include <sound/soc.h>
#include <sound/pcm_params.h>
#include <sound/soc-dapm.h>
#include <linux/io.h>
#include <mach/sys_config.h>

static int sunxi_sndvir_audio_hw_params(struct snd_pcm_substream *substream,
					struct snd_pcm_hw_params *params)
{
	return 0;
}

/*
 * Card initialization
 */
static int sunxi_vir_audio_init(struct snd_soc_pcm_runtime *rtd)
{
//	struct snd_soc_codec *codec = rtd->codec;
//	struct snd_soc_card *card = rtd->card;
//	int ret;

	/* Add virtual switch */
//	ret = snd_soc_add_codec_controls(codec, sunxi_vir_audio_controls,
//					ARRAY_SIZE(sunxi_vir_audio_controls));
//	if (ret) {
//		dev_warn(card->dev,
//				"Failed to register audio mode control, "
//				"will continue without it.\n");
//	}
	return 0;
}

static struct snd_soc_ops sunxi_sndvir_audio_ops = {
	.hw_params 		= sunxi_sndvir_audio_hw_params,
};

static struct snd_soc_dai_link sunxi_sndvir_audio_dai_link = {
	.name 			= "VIR_AUDIO",
	.stream_name 	= "SUNXI-VIR_AUDIO",
	.cpu_dai_name 	= "vir_audio",
	.codec_dai_name = "sndvir_audio",
	.init 			= sunxi_vir_audio_init,
	.platform_name 	= "sunxi-vir_audio-pcm-audio.0",
	/*sunxi-codec is in twi1,addr is 0x34*/
	.codec_name 	= "vir_audio-codec.2-0034",
	.ops 			= &sunxi_sndvir_audio_ops,
};

static struct snd_soc_card snd_soc_sunxi_sndvir_audio = {
	.name 		= "sndvir_audio--hx",
	.owner 		= THIS_MODULE,
	.dai_link 	= &sunxi_sndvir_audio_dai_link,
	.num_links 	= 1,
};

static struct platform_device *sunxi_sndvir_audio_device;

static int __init sunxi_sndvir_audio_init(void)
{
	int ret = 0;

	sunxi_sndvir_audio_device = platform_device_alloc("soc-audio", 4);
	if(!sunxi_sndvir_audio_device)
		return -ENOMEM;
	platform_set_drvdata(sunxi_sndvir_audio_device, &snd_soc_sunxi_sndvir_audio);
	ret = platform_device_add(sunxi_sndvir_audio_device);
	if (ret) {
		platform_device_put(sunxi_sndvir_audio_device);
	}

	return ret;
}

static void __exit sunxi_sndvir_audio_exit(void)
{
	platform_device_unregister(sunxi_sndvir_audio_device);
}

module_init(sunxi_sndvir_audio_init);
module_exit(sunxi_sndvir_audio_exit);

MODULE_AUTHOR("huangxin");
MODULE_DESCRIPTION("SUNXI_sndvir_audio ALSA SoC audio driver");
MODULE_LICENSE("GPL");
