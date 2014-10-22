/*
 * sound\soc\sunxi\vir_audio\sunxi-vir_audio.c
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

#include <linux/init.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/clk.h>
#include <linux/jiffies.h>
#include <linux/io.h>
#include <linux/pinctrl/consumer.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/initval.h>
#include <sound/soc.h>

#include <asm/dma.h>
#include <mach/hardware.h>
#include <mach/sys_config.h>
#include <linux/gpio.h>

static int sunxi_vir_audio_dai_probe(struct snd_soc_dai *dai)
{
	return 0;
}

static int sunxi_vir_audio_dai_remove(struct snd_soc_dai *dai)
{
	return 0;
}

static int sunxi_vir_audio_suspend(struct snd_soc_dai *cpu_dai)
{
	return 0;
}

static int sunxi_vir_audio_resume(struct snd_soc_dai *cpu_dai)
{
	return 0;
}

#define SUNXI_VIR_AUDIO_RATES (SNDRV_PCM_RATE_8000_192000 | SNDRV_PCM_RATE_KNOT)

static struct snd_soc_dai_driver sunxi_vir_audio_dai = {	
	.probe 		= sunxi_vir_audio_dai_probe,
	.suspend 	= sunxi_vir_audio_suspend,
	.resume 	= sunxi_vir_audio_resume,
	.remove 	= sunxi_vir_audio_dai_remove,
	.playback 	= {
		.channels_min = 1,
		.channels_max = 2,
		.rates = SUNXI_VIR_AUDIO_RATES,
		.formats = SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S20_3LE | SNDRV_PCM_FMTBIT_S24_LE,
	},
	.capture 	= {
		.channels_min = 1,
		.channels_max = 2,
		.rates = SUNXI_VIR_AUDIO_RATES,
		.formats = SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S20_3LE | SNDRV_PCM_FMTBIT_S24_LE,
	},
};

static int __init sunxi_vir_audio_dev_probe(struct platform_device *pdev)
{
	int ret;
	ret = snd_soc_register_dai(&pdev->dev, &sunxi_vir_audio_dai);	
	if (ret) {
		dev_err(&pdev->dev, "Failed to register DAI\n");
	}

	return 0;
}

static int __exit sunxi_vir_audio_dev_remove(struct platform_device *pdev)
{
	snd_soc_unregister_dai(&pdev->dev);
	platform_set_drvdata(pdev, NULL);

	return 0;
}

/*data relating*/
static struct platform_device sunxi_vir_audio_device = {
	.name 	= "vir_audio",
	.id 	= PLATFORM_DEVID_NONE,
};

/*method relating*/
static struct platform_driver sunxi_vir_audio_driver = {
	.probe = sunxi_vir_audio_dev_probe,
	.remove = __exit_p(sunxi_vir_audio_dev_remove),
	.driver = {
		.name = "vir_audio",
		.owner = THIS_MODULE,
	},
};

static int __init sunxi_vir_audio_init(void)
{
	int err = 0;

	if((err = platform_device_register(&sunxi_vir_audio_device)) < 0)
		return err;

	if ((err = platform_driver_register(&sunxi_vir_audio_driver)) < 0)
			return err;	

	return 0;
}
module_init(sunxi_vir_audio_init);

static void __exit sunxi_vir_audio_exit(void)
{
	platform_driver_unregister(&sunxi_vir_audio_driver);
}
module_exit(sunxi_vir_audio_exit);

/* Module information */
MODULE_AUTHOR("huangxin");
MODULE_DESCRIPTION("sunxi VIR_AUDIO SoC Interface");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:sunxi-vir_audio");

