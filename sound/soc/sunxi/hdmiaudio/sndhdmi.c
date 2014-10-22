/*
 * sound\soc\sunxi\hdmiaudio\sndhdmi.c
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
#include <linux/delay.h>
#include <linux/slab.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/initval.h>
#include <linux/io.h>
#include <video/drv_hdmi.h>

#if defined CONFIG_ARCH_SUN9I || CONFIG_ARCH_SUN8IW6
#include "sunxi-hdmipcm.h"
#endif
#ifdef CONFIG_ARCH_SUN8IW7
#include "sunxi-hdmitdm.h"
#endif
static __audio_hdmi_func 	g_hdmi_func;
static hdmi_audio_t 		hdmi_para;
atomic_t pcm_count_num;

#define SNDHDMI_RATES  (SNDRV_PCM_RATE_8000_192000|SNDRV_PCM_RATE_KNOT)
#define SNDHDMI_FORMATS (SNDRV_PCM_FMTBIT_S8 | SNDRV_PCM_FMTBIT_S16_LE | \
		                     SNDRV_PCM_FMTBIT_S18_3LE | SNDRV_PCM_FMTBIT_S20_3LE | SNDRV_PCM_FMTBIT_S24_LE | SNDRV_PCM_FMTBIT_S32_LE)

/*the struct just for register the hdmiaudio codec node*/
struct sndhdmi_priv {
	int sysclk;
	int dai_fmt;

	struct snd_pcm_substream *master_substream;
	struct snd_pcm_substream *slave_substream;
};

void audio_set_hdmi_func(__audio_hdmi_func *hdmi_func)
{
	if (hdmi_func) {
		g_hdmi_func.hdmi_audio_enable 	= hdmi_func->hdmi_audio_enable;
		g_hdmi_func.hdmi_set_audio_para = hdmi_func->hdmi_set_audio_para;
		g_hdmi_func.hdmi_is_playback = hdmi_func->hdmi_is_playback;
	} else {
		pr_err("error:%s,line:%d\n", __func__, __LINE__);
	}
}
EXPORT_SYMBOL(audio_set_hdmi_func);

static int sndhdmi_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params,
	struct snd_soc_dai *dai)
{
	if ((!substream)||(!params)) {
		pr_err("error:%s,line:%d\n", __func__, __LINE__);
		return -EAGAIN;
	}
	hdmi_para.sample_rate = params_rate(params);
	hdmi_para.channel_num = params_channels(params);
#ifdef CONFIG_SND_SUNXI_SOC_SUPPORT_AUDIO_RAW
	hdmi_para.data_raw 		= params_raw(params);
#else
	hdmi_para.data_raw 		= 1;
#endif	
	switch (params_format(params))
	{
		case SNDRV_PCM_FORMAT_S16_LE:
			hdmi_para.sample_bit = 16;
			break;
		case SNDRV_PCM_FORMAT_S20_3LE:
			hdmi_para.sample_bit = 24;
			break;
		case SNDRV_PCM_FORMAT_S24_LE:
			hdmi_para.sample_bit = 24;
			break;
		case SNDRV_PCM_FORMAT_S32_LE:
			hdmi_para.sample_bit = 24;
			break;
		default:
			return -EINVAL;
	}
	/*
		PCM = 1,
		AC3 = 2,
		MPEG1 = 3,
		MP3 = 4,
		MPEG2 = 5,
		AAC = 6,
		DTS = 7,
		ATRAC = 8,
		ONE_BIT_AUDIO = 9,
		DOLBY_DIGITAL_PLUS = 10,
		DTS_HD = 11,
		MAT = 12,
		DST = 13,
		WMAPRO = 14. 
	*/
	if (hdmi_para.data_raw > 1) {
		hdmi_para.sample_bit = 24; //??? TODO
	}
	if (hdmi_para.channel_num == 8) {
		hdmi_para.ca = 0x12;
	} else if ((hdmi_para.channel_num >= 3)) {
		hdmi_para.ca = 0x1f;
	} else {
		hdmi_para.ca = 0x0;
	}

	return 0;
}

static int sndhdmi_set_dai_fmt(struct snd_soc_dai *codec_dai,
			       unsigned int fmt)
{
	return 0;
}
static int sndhdmi_perpare(struct snd_pcm_substream *substream,
	struct snd_soc_dai *dai)
{
	u32 reg_val;
#ifdef CONFIG_ARCH_SUN9I
	int is_play = 0, i = 0;
#endif
#if defined CONFIG_ARCH_SUN9I || CONFIG_ARCH_SUN8IW6
	/*Global Enable Digital Audio Interface*/
	reg_val = readl(SUNXI_I2S1_VBASE + SUNXI_I2S1CTL);
	reg_val |= SUNXI_I2S1CTL_GEN;
	writel(reg_val, SUNXI_I2S1_VBASE + SUNXI_I2S1CTL);
	msleep(10);
	/*flush TX FIFO*/
	reg_val = readl(SUNXI_I2S1_VBASE + SUNXI_I2S1FCTL);
    reg_val |= SUNXI_I2S1FCTL_FTX;
	writel(reg_val, SUNXI_I2S1_VBASE + SUNXI_I2S1FCTL);
#endif
#ifdef CONFIG_ARCH_SUN8IW7
	/*Global Enable Digital Audio Interface*/
	reg_val = readl(SUNXI_DAUDIO2_VBASE + SUNXI_DAUDIOCTL);
	reg_val |= SUNXI_DAUDIOCTL_GEN;
	writel(reg_val, SUNXI_DAUDIO2_VBASE + SUNXI_DAUDIOCTL);
	msleep(10);
	/*flush TX FIFO*/
	reg_val = readl(SUNXI_DAUDIO2_VBASE + SUNXI_DAUDIOFCTL);
	reg_val |= SUNXI_DAUDIOFCTL_FTX;
	writel(reg_val, SUNXI_DAUDIO2_VBASE + SUNXI_DAUDIOFCTL);
#endif
	if ((hdmi_para.data_raw > 1)||(hdmi_para.sample_bit!=16)||(hdmi_para.channel_num >= 3)) {
		atomic_set(&pcm_count_num, 0);
	} else {
		atomic_inc(&pcm_count_num);
	}
	/*
	*	set the first pcm param, need set the hdmi audio pcm param
	*	set the data_raw param, need set the hdmi audio raw param
	*/
	if (atomic_read(&pcm_count_num) <= 1) {
		g_hdmi_func.hdmi_set_audio_para(&hdmi_para);
	}
	g_hdmi_func.hdmi_audio_enable(1, 1);
#ifdef CONFIG_ARCH_SUN9I
	is_play = g_hdmi_func.hdmi_is_playback();
	i = 0;
	while (!is_play) {
		i++;
		msleep(20);
		if(i>50)
			break;
		is_play = g_hdmi_func.hdmi_is_playback();
		if(is_play)
			break;
	}
	is_play = g_hdmi_func.hdmi_is_playback();
#endif
	return 0;
}
static int sndhdmi_trigger(struct snd_pcm_substream *substream,
                              int cmd, struct snd_soc_dai *dai)
{
	int ret = 0;

	switch (cmd) {
		case SNDRV_PCM_TRIGGER_START:
		case SNDRV_PCM_TRIGGER_RESUME:
		case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
			if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
			} else {
			}
		break;
		case SNDRV_PCM_TRIGGER_STOP:
		case SNDRV_PCM_TRIGGER_SUSPEND:
		case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
			if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
			} else {
			}
			break;
		default:
			ret = -EINVAL;
			break;
	}
	return ret;
}
static int sunxi_sndhdmi_suspend(struct snd_soc_dai *dai)
{
	return 0;
}

static int sunxi_sndhdmi_resume(struct snd_soc_dai *dai)
{
	atomic_set(&pcm_count_num, 0);
	return 0;
}
static void sndhdmi_shutdown(struct snd_pcm_substream *substream,
	struct snd_soc_dai *dai)
{
	g_hdmi_func.hdmi_audio_enable(0, 1);
}

/*codec dai operation*/
static struct snd_soc_dai_ops sndhdmi_dai_ops = {
	.hw_params 	= sndhdmi_hw_params,
	.set_fmt 	= sndhdmi_set_dai_fmt,
	.trigger 	= sndhdmi_trigger,
	.prepare  =	sndhdmi_perpare,
	.shutdown 	= sndhdmi_shutdown,
};

/*codec dai*/
static struct snd_soc_dai_driver sndhdmi_dai = {
	.name 		= "sndhdmi",
	/* playback capabilities */
	.playback 	= {
		.stream_name 	= "Playback",
		.channels_min 	= 1,
		.channels_max 	= 8,
		.rates 			= SNDHDMI_RATES,
		.formats 		= SNDHDMI_FORMATS,
	},
	/* pcm operations */
	.ops 		= &sndhdmi_dai_ops,
	.suspend 	= sunxi_sndhdmi_suspend,
	.resume 	= sunxi_sndhdmi_resume,
};
EXPORT_SYMBOL(sndhdmi_dai);

static int sndhdmi_soc_probe(struct snd_soc_codec *codec)
{
	struct sndhdmi_priv *sndhdmi = NULL;

	atomic_set(&pcm_count_num, 0);
	if (!codec) {
		pr_err("error:%s,line:%d\n", __func__, __LINE__);
		return -EAGAIN;
	}

	sndhdmi = kzalloc(sizeof(struct sndhdmi_priv), GFP_KERNEL);
	if (sndhdmi == NULL) {
		pr_err("error at:%s,%d\n",__func__,__LINE__);
		return -ENOMEM;
	}

	snd_soc_codec_set_drvdata(codec, sndhdmi);

	return 0;
}

static int sndhdmi_soc_remove(struct snd_soc_codec *codec)
{
	struct sndhdmi_priv *sndhdmi = NULL;
	if (!codec) {
		pr_err("error:%s,line:%d\n", __func__, __LINE__);
		return -EAGAIN;
	}
	sndhdmi = snd_soc_codec_get_drvdata(codec);

	kfree(sndhdmi);

	return 0;
}

static struct snd_soc_codec_driver soc_codec_dev_sndhdmi = {
	.probe 	=	sndhdmi_soc_probe,
	.remove =   sndhdmi_soc_remove,
};

static int __init sndhdmi_codec_probe(struct platform_device *pdev)
{	
	if (!pdev) {
		pr_err("error:%s,line:%d\n", __func__, __LINE__);
		return -EAGAIN;
	}
	return snd_soc_register_codec(&pdev->dev, &soc_codec_dev_sndhdmi, &sndhdmi_dai, 1);	
}

static int __exit sndhdmi_codec_remove(struct platform_device *pdev)
{
	if (!pdev) {
		pr_err("error:%s,line:%d\n", __func__, __LINE__);
		return -EAGAIN;
	}
	snd_soc_unregister_codec(&pdev->dev);
	return 0;
}

/*data relating*/
static struct platform_device sndhdmi_codec_device = {
	.name = "sunxi-hdmiaudio-codec",
};
static struct platform_driver sndhdmi_codec_driver = {
	.driver = {
		.name 	= "sunxi-hdmiaudio-codec",
		.owner 	= THIS_MODULE,
	},
	.probe 	= sndhdmi_codec_probe,
	.remove = __exit_p(sndhdmi_codec_remove),
};

static int __init sndhdmi_codec_init(void)
{
	int err = 0;
	if ((err = platform_device_register(&sndhdmi_codec_device)) < 0)
		return err;

	if ((err = platform_driver_register(&sndhdmi_codec_driver)) < 0)
		return err;

	return 0;
}
module_init(sndhdmi_codec_init);

static void __exit sndhdmi_codec_exit(void)
{
	platform_driver_unregister(&sndhdmi_codec_driver);
}
module_exit(sndhdmi_codec_exit);

MODULE_DESCRIPTION("SNDHDMI ALSA soc codec driver");
MODULE_AUTHOR("huangxin, <huangxin@Reuuimllatech.com>");
MODULE_LICENSE("GPL");
