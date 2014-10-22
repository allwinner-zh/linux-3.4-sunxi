/*
 * sound\soc\sunxi\audiocodec\sndcodec.c
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
#include <linux/clk.h>
#include <linux/gpio.h>
#include <linux/io.h>
#include <linux/regulator/consumer.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/initval.h>
#include <mach/sys_config.h>
#include <mach/gpio.h>
#include <linux/power/scenelock.h>

#include "sunxi_codecdma.h"
#include "sun8iw7_sndcodec.h"

#define sndpcm_RATES  (SNDRV_PCM_RATE_8000_192000|SNDRV_PCM_RATE_KNOT)
#define sndpcm_FORMATS (SNDRV_PCM_FMTBIT_S8 | SNDRV_PCM_FMTBIT_S16_LE | \
		                     SNDRV_PCM_FMTBIT_S18_3LE | SNDRV_PCM_FMTBIT_S20_3LE)
static void  __iomem *baseaddr;
static struct regulator* hp_ldo = NULL;
static char *hp_ldo_str = NULL;

/*for pa gpio ctrl*/
static script_item_u item;
static script_item_value_type_e  type;

static int pa_vol 						= 0;
static int cap_vol 						= 0;
static int earpiece_vol 				= 0;
static int headphone_vol 				= 0;
static int pa_double_used 				= 0;
static int phone_main_mic_vol 			= 0;
static int phone_headset_mic_vol 		= 0;

static struct clk *codec_pll,*codec_moduleclk;

static struct sunxi_dma_params sunxi_pcm_pcm_stereo_out = {
	.name		= "audio_play",
	.dma_addr	= CODEC_BASSADDRESS + SUNXI_DAC_TXDATA,//send data address	
};

static struct sunxi_dma_params sunxi_pcm_pcm_stereo_in = {
	.name   	= "audio_capture",
	.dma_addr	= CODEC_BASSADDRESS + SUNXI_ADC_RXDATA,//accept data address	
};

static unsigned int read_prcm_wvalue(unsigned int addr)
{
  unsigned int reg;
	reg = readl(ADDA_PR_CFG_REG);
	reg |= (0x1<<28);
	writel(reg, ADDA_PR_CFG_REG);

	reg = readl(ADDA_PR_CFG_REG);
	reg &= ~(0x1<<24);
	writel(reg, ADDA_PR_CFG_REG);

	reg = readl(ADDA_PR_CFG_REG);
	reg &= ~(0x1f<<16);
	reg |= (addr<<16);
	writel(reg, ADDA_PR_CFG_REG);

	reg = readl(ADDA_PR_CFG_REG); 
	reg &= (0xff<<0);

	return reg;
}

static void write_prcm_wvalue(unsigned int addr, unsigned int val)
{
  unsigned int reg;
	reg = readl(ADDA_PR_CFG_REG);
	reg |= (0x1<<28);
	writel(reg, ADDA_PR_CFG_REG);

	reg = readl(ADDA_PR_CFG_REG);
	reg &= ~(0x1f<<16);
	reg |= (addr<<16);
	writel(reg, ADDA_PR_CFG_REG);

	reg = readl(ADDA_PR_CFG_REG);
	reg &= ~(0xff<<8);
	reg |= (val<<8);
	writel(reg, ADDA_PR_CFG_REG);

	reg = readl(ADDA_PR_CFG_REG);
	reg |= (0x1<<24);
	writel(reg, ADDA_PR_CFG_REG);

	reg = readl(ADDA_PR_CFG_REG);
	reg &= ~(0x1<<24);
	writel(reg, ADDA_PR_CFG_REG);
}

/**
* codec_wrreg_bits - update codec register bits
* @reg: codec register
* @mask: register mask
* @value: new value
*
* Writes new register value.
* Return 1 for change else 0.
*/
static int codec_wrreg_prcm_bits(unsigned short reg, unsigned int mask, unsigned int value)
{
	unsigned int old, new;
		
	old	=	read_prcm_wvalue(reg);
	new	=	(old & ~mask) | value;
	write_prcm_wvalue(reg,new);

	return 0;
}

static int codec_wr_prcm_control(u32 reg, u32 mask, u32 shift, u32 val)
{
	u32 reg_val;
	reg_val = val << shift;
	mask = mask << shift;
	codec_wrreg_prcm_bits(reg, mask, reg_val);
	return 0;
}

/**
* codec_wrreg_bits - update codec register bits
* @reg: codec register
* @mask: register mask
* @value: new value
*
* Writes new register value.
* Return 1 for change else 0.
*/
static int codec_wrreg_bits(unsigned short reg, unsigned int	mask,	unsigned int value)
{
	unsigned int old, new;
		
	old	=	codec_rdreg(reg);
	new	=	(old & ~mask) | value;
	codec_wrreg(reg,new);

	return 0;
}

/**
*	snd_codec_info_volsw	-	single	mixer	info	callback
*	@kcontrol:	mixer control
*	@uinfo:	control	element	information
*	Callback to provide information about a single mixer control
*
*	Returns 0 for success
*/
int snd_codec_info_volsw(struct snd_kcontrol *kcontrol,
		struct	snd_ctl_elem_info	*uinfo)
{
	struct	codec_mixer_control *mc	= (struct codec_mixer_control*)kcontrol->private_value;
	int	max	=	mc->max;
	unsigned int shift  = mc->shift;
	unsigned int rshift = mc->rshift;

	if (max	== 1)
		uinfo->type = SNDRV_CTL_ELEM_TYPE_BOOLEAN;//the info of type
	else
		uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;

	uinfo->count = shift ==	rshift	?	1:	2;	//the info of elem count
	uinfo->value.integer.min = 0;				//the info of min value
	uinfo->value.integer.max = max;				//the info of max value
	return	0;
}

/**
*	snd_codec_get_volsw	-	single	mixer	get	callback
*	@kcontrol:	mixer	control
*	@ucontrol:	control	element	information
*
*	Callback to get the value of a single mixer control
*	return 0 for success.
*/
int snd_codec_get_volsw(struct snd_kcontrol	*kcontrol,
		struct	snd_ctl_elem_value	*ucontrol)
{
	struct codec_mixer_control *mc= (struct codec_mixer_control*)kcontrol->private_value;
	unsigned int shift = mc->shift;
	unsigned int rshift = mc->rshift;
	int	max = mc->max;
	/*fls(7) = 3,fls(1)=1,fls(0)=0,fls(15)=4,fls(3)=2,fls(23)=5*/
	unsigned int mask = (1 << fls(max)) -1;
	unsigned int invert = mc->invert;
	unsigned int reg = mc->reg;

	ucontrol->value.integer.value[0] =	
		(read_prcm_wvalue(reg)>>	shift) & mask;
	if (shift != rshift)
		ucontrol->value.integer.value[1] =
			(read_prcm_wvalue(reg) >> rshift) & mask;

	if (invert) {
		ucontrol->value.integer.value[0] =
			max - ucontrol->value.integer.value[0];
		if(shift != rshift)
			ucontrol->value.integer.value[1] =
				max - ucontrol->value.integer.value[1];
		}

		return 0;
}

/**
*	snd_codec_put_volsw	-	single	mixer put callback
*	@kcontrol:	mixer	control
*	@ucontrol:	control	element	information
*
*	Callback to put the value of a single mixer control
*
* return 0 for success.
*/
int snd_codec_put_volsw(struct	snd_kcontrol	*kcontrol,
	struct	snd_ctl_elem_value	*ucontrol)
{
	struct codec_mixer_control *mc= (struct codec_mixer_control*)kcontrol->private_value;
	unsigned int reg = mc->reg;
	unsigned int shift = mc->shift;
	unsigned int rshift = mc->rshift;
	int max = mc->max;
	unsigned int mask = (1<<fls(max))-1;
	unsigned int invert = mc->invert;
	unsigned int	val, val2, val_mask;

	val = (ucontrol->value.integer.value[0] & mask);
	if(invert)
		val = max - val;
	val <<= shift;
	val_mask = mask << shift;
	if(shift != rshift){
		val2	= (ucontrol->value.integer.value[1] & mask);
		if(invert)
			val2	=	max	- val2;
		val_mask |= mask <<rshift;
		val |= val2 <<rshift;
	}
	
	return codec_wrreg_prcm_bits(reg, val_mask, val);
}

static int codec_wr_control(u32 reg, u32 mask, u32 shift, u32 val)
{
	u32 reg_val;
	reg_val = val << shift;
	mask = mask << shift;
	codec_wrreg_bits(reg, mask, reg_val);
	return 0;
}

static void get_audio_param(void)
{
	script_item_value_type_e  type;
	script_item_u val;

	type = script_get_item("audio0", "headphone_vol", &val);
	if (SCIRPT_ITEM_VALUE_TYPE_INT != type) {
		pr_err("[audiocodec] headphone_vol type err!\n");
	}  else { 
		headphone_vol = val.val;
	}

	type = script_get_item("audio0", "cap_vol", &val);
	if (SCIRPT_ITEM_VALUE_TYPE_INT != type) {
		pr_err("[audiocodec] cap_vol type err!\n");
	}  else { 
		cap_vol = val.val;
	}

	pr_debug("headphone_vol=0x%x, earpiece_vol=0x%x, cap_vol=0x%x, \
		phone_headset_mic_vol=0x%x, phone_main_mic_vol=0x%x, \
		pa_double_used=0x%x, pa_vol=0x%x \n" \
		,headphone_vol, earpiece_vol, cap_vol,  \
		phone_headset_mic_vol, phone_main_mic_vol, \
		pa_double_used, pa_vol);
}

/*
*	enable the codec function which should be enable during system init.
*/
static void codec_init(void)
{

	get_audio_param();

	/*when TX FIFO available room less than or equal N,
	* DRQ Requeest will be de-asserted.
	*/
	codec_wr_control(SUNXI_DAC_FIFOC, 0x3, DAC_DRQ_CLR_CNT, 0x3);

	/*write 1 to flush tx fifo*/
	codec_wr_control(SUNXI_DAC_FIFOC, 0x1, FIFO_FLUSH, 0x1);
	/*write 1 to flush rx fifo*/
	codec_wr_control(SUNXI_ADC_FIFOC, 0x1, ADC_FIFO_FLUSH, 0x1);
	codec_wr_control(SUNXI_DAC_FIFOC, 0x1, FIR_VER, 0x1);
}

/*
*	the system voice come out from speaker
* 	this function just used for the system voice(such as music and moive voice and so on).
*/
static int codec_pa_play_open(void)
{
	/*enable dac digital*/
	codec_wr_control(SUNXI_DAC_DPC, 0x1, DAC_EN, 0x1);
	codec_wr_prcm_control(LINEOUT_PA_GAT, 0x1, PA_CLK_GC, 0x0);

	/*set TX FIFO send drq level*/
	codec_wr_control(SUNXI_DAC_FIFOC ,0x7f, TX_TRI_LEVEL, 0xf);
	/*set TX FIFO MODE*/
	codec_wr_control(SUNXI_DAC_FIFOC ,0x1, FIFO_MODE, 0x1);

	//send last sample when dac fifo under run
	codec_wr_control(SUNXI_DAC_FIFOC ,0x1, SEND_LASAT, 0x0);

	codec_wr_prcm_control(DAC_PA_SRC, 0x1, DACALEN, 0x1);
	codec_wr_prcm_control(DAC_PA_SRC, 0x1, DACAREN, 0x1);

	codec_wr_prcm_control(DAC_PA_SRC, 0x1, LMIXEN, 0x1);
	codec_wr_prcm_control(DAC_PA_SRC, 0x1, RMIXEN, 0x1);
    //by xzd left select dacl&dacr
	codec_wr_prcm_control(ROMIXSC, 0x7f, RMIXMUTE, 0x0);
	//by xzd right don't select src
	codec_wr_prcm_control(LOMIXSC, 0x7f, LMIXMUTE, 0x3);

	gpio_set_value(item.gpio.gpio, 1);
	msleep(62);

	return 0;
}

/*
*	use for the base system record(for pad record).
*/
static int codec_capture_open(void)
{
	/*enable mic1 pa*/
	codec_wr_prcm_control(MIC1G_MICBIAS_CTR, 0x1, MIC1AMPEN, 0x1);
	/*mic1 gain 36dB,if capture volume is too small, enlarge the mic1boost*/
	codec_wr_prcm_control(MIC1G_MICBIAS_CTR, 0x7, MIC1BOOST, cap_vol);
	/*enable Master microphone bias*/
	codec_wr_prcm_control(MIC1G_MICBIAS_CTR, 0x1, MMICBIASEN, 0x1);

	/*enable Left MIC1 Boost stage*/
	codec_wr_prcm_control(LADCMIXSC, 0x1, LADCMIXMUTEMIC1BOOST, 0x1);
	/*enable Right MIC1 Boost stage*/
	codec_wr_prcm_control(RADCMIXSC, 0x1, RADCMIXMUTEMIC1BOOST, 0x1);

	/*enable adc_r adc_l analog*/
	codec_wr_prcm_control(ADC_AP_EN, 0x1,  ADCREN, 0x1);
	codec_wr_prcm_control(ADC_AP_EN, 0x1,  ADCLEN, 0x1);

	/*set RX FIFO mode*/
	codec_wr_control(SUNXI_ADC_FIFOC, 0x1, RX_FIFO_MODE, 0x1);
	/*set RX FIFO rec drq level*/
	codec_wr_control(SUNXI_ADC_FIFOC, 0x1f, RX_FIFO_TRG_LEVEL, 0xf);
	/*enable adc digital part*/
	codec_wr_control(SUNXI_ADC_FIFOC, 0x1,EN_AD, 0x1);
	/*enable adc drq*/
	codec_wr_control(SUNXI_ADC_FIFOC ,0x1, ADC_DRQ_EN, 0x1);
	/*hardware fifo delay*/
	msleep(200);
	return 0;
}

/*
*	codec_speaker_headset_earpiece_en == 3, earpiece is open,speaker and headphone is close.
*	codec_speaker_headset_earpiece_en == 2, speaker is open, headphone is open.
*	codec_speaker_headset_earpiece_en == 1, speaker is open, headphone is close.
*	codec_speaker_headset_earpiece_en == 0, speaker is closed, headphone is open.
*/
static int codec_play_start(void)
{
	/*enable dac drq*/
	codec_wr_control(SUNXI_DAC_FIFOC ,0x1, DAC_DRQ_EN, 0x1);
	/*DAC FIFO Flush,Write '1' to flush TX FIFO, self clear to '0'*/
	codec_wr_control(SUNXI_DAC_FIFOC ,0x1, FIFO_FLUSH, 0x1);

	return 0;
}

static int codec_play_stop(void)
{
	return 0;
}

static int codec_capture_stop(void)
{
	/*disable adc digital part*/
	codec_wr_control(SUNXI_ADC_FIFOC, 0x1,EN_AD, 0x0);
	/*disable adc drq*/
	codec_wr_control(SUNXI_ADC_FIFOC ,0x1, ADC_DRQ_EN, 0x0);

	/*disable mic1 pa*/
	codec_wr_prcm_control(MIC1G_MICBIAS_CTR, 0x1, MIC1AMPEN, 0x0);
	/*disable Master microphone bias*/
	codec_wr_prcm_control(MIC1G_MICBIAS_CTR, 0x1, MMICBIASEN, 0x0);
	codec_wr_prcm_control(MIC2G_LINEOUT_CTR, 0x1, MIC2AMPEN, 0x0);

	/*disable Left MIC1 Boost stage*/
	codec_wr_prcm_control(LADCMIXSC, 0x1, LADCMIXMUTEMIC1BOOST, 0x1);
	/*disable Right MIC1 Boost stage*/
	codec_wr_prcm_control(RADCMIXSC, 0x1, RADCMIXMUTEMIC1BOOST, 0x1);

	/*disable Left MIC1 Boost stage*/
	codec_wr_prcm_control(LADCMIXSC, 0x1, LADCMIXMUTEMIC2BOOST, 0x1);
	/*disable Right MIC1 Boost stage*/
	codec_wr_prcm_control(RADCMIXSC, 0x1, RADCMIXMUTEMIC2BOOST, 0x1);

	/*disable LINEINL ADC*/
	codec_wr_prcm_control(LADCMIXSC, 0x1, LADCMIXMUTELINEINL, 0x1);
	/*disable LINEINR ADC*/
	codec_wr_prcm_control(RADCMIXSC, 0x1, RADCMIXMUTELINEINR, 0x1);

	/*disable adc_r adc_l analog*/
	codec_wr_prcm_control(ADC_AP_EN, 0x1,  ADCREN, 0x0);
	codec_wr_prcm_control(ADC_AP_EN, 0x1,  ADCLEN, 0x0);
	return 0;
}

static int sndpcm_mute(struct snd_soc_dai *dai, int mute)
{
	return 0;
}

static int sndpcm_startup(struct snd_pcm_substream *substream,
	struct snd_soc_dai *dai)
{
	return 0;
}

static void sndpcm_shutdown(struct snd_pcm_substream *substream,
	struct snd_soc_dai *dai)
{
	codec_wr_prcm_control(ADDA_APT2, 0x1, ZERO_CROSS_EN, 0x0);

	codec_wr_prcm_control(DAC_PA_SRC, 0x1, LMIXEN, 0x0);
	codec_wr_prcm_control(DAC_PA_SRC, 0x1, RMIXEN, 0x0);

	/*disable dac drq*/
	codec_wr_control(SUNXI_DAC_FIFOC ,0x1, DAC_DRQ_EN, 0x0);
	codec_wr_prcm_control(DAC_PA_SRC, 0x1, DACALEN, 0x0);
	codec_wr_prcm_control(DAC_PA_SRC, 0x1, DACAREN, 0x0);
	/*disable dac digital*/
	codec_wr_control(SUNXI_DAC_DPC ,  0x1, DAC_EN, 0x0);
	codec_wr_prcm_control(LINEOUT_PA_GAT, 0x1, PA_CLK_GC, 0x0);

}

static int sndpcm_perpare(struct snd_pcm_substream *substream,
	struct snd_soc_dai *dai)
{
	int play_ret = 0, capture_ret = 0;
	unsigned int reg_val;

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		switch (substream->runtime->rate) {
			case 44100:
				if (clk_set_rate(codec_pll, 22579200)) {
					pr_err("set codec_pll rate fail\n");
				}
				if (clk_set_rate(codec_moduleclk, 22579200)) {
					pr_err("set codec_moduleclk rate fail\n");
				}
				reg_val = readl(baseaddr + SUNXI_DAC_FIFOC);
				reg_val &=~(7<<29);
				reg_val |=(0<<29);
				writel(reg_val, baseaddr + SUNXI_DAC_FIFOC);
				break;
			case 22050:
				if (clk_set_rate(codec_pll, 22579200)) {
					pr_err("set codec_pll rate fail\n");
				}
				if (clk_set_rate(codec_moduleclk, 22579200)) {
					pr_err("set codec_moduleclk rate fail\n");
				}
				reg_val = readl(baseaddr + SUNXI_DAC_FIFOC);
				reg_val &=~(7<<29); 
				reg_val |=(2<<29);
				writel(reg_val, baseaddr + SUNXI_DAC_FIFOC);
				break;
			case 11025:
				if (clk_set_rate(codec_pll, 22579200)) {
					pr_err("set codec_pll rate fail\n");
				}
				if (clk_set_rate(codec_moduleclk, 22579200)) {
					pr_err("set codec_moduleclk rate fail\n");
				}
				reg_val = readl(baseaddr + SUNXI_DAC_FIFOC);
				reg_val &=~(7<<29); 
				reg_val |=(4<<29);
				writel(reg_val, baseaddr + SUNXI_DAC_FIFOC);
				break;
			case 48000:
				if (clk_set_rate(codec_pll, 24576000)) {
					pr_err("set codec_pll rate fail\n");
				}
				if (clk_set_rate(codec_moduleclk, 24576000)) {
					pr_err("set codec_moduleclk rate fail\n");
				}
				reg_val = readl(baseaddr + SUNXI_DAC_FIFOC);
				reg_val &=~(7<<29); 
				reg_val |=(0<<29);
				writel(reg_val, baseaddr + SUNXI_DAC_FIFOC);
				break;
			case 96000:
				if (clk_set_rate(codec_pll, 24576000)) {
					pr_err("set codec_pll rate fail\n");
				}
				if (clk_set_rate(codec_moduleclk, 24576000)) {
					pr_err("set codec_moduleclk rate fail\n");
				}
				reg_val = readl(baseaddr + SUNXI_DAC_FIFOC);
				reg_val &=~(7<<29);
				reg_val |=(7<<29);
				writel(reg_val, baseaddr + SUNXI_DAC_FIFOC);
				break;
			case 192000:
				if (clk_set_rate(codec_pll, 24576000)) {
					pr_err("set codec_pll rate fail\n");
				}
				if (clk_set_rate(codec_moduleclk, 24576000)) {
					pr_err("set codec_moduleclk rate fail\n");
				}
				reg_val = readl(baseaddr + SUNXI_DAC_FIFOC);
				reg_val &=~(7<<29);
				reg_val |=(6<<29);
				writel(reg_val, baseaddr + SUNXI_DAC_FIFOC);
				break;
			case 32000:
				if (clk_set_rate(codec_pll, 24576000)) {
					pr_err("set codec_pll rate fail\n");
				}
				if (clk_set_rate(codec_moduleclk, 24576000)) {
					pr_err("set codec_moduleclk rate fail\n");
				}
				reg_val = readl(baseaddr + SUNXI_DAC_FIFOC);
				reg_val &=~(7<<29); 
				reg_val |=(1<<29);
				writel(reg_val, baseaddr + SUNXI_DAC_FIFOC);
				break;
			case 24000:
				if (clk_set_rate(codec_pll, 24576000)) {
					pr_err("set codec_pll rate fail\n");
				}
				if (clk_set_rate(codec_moduleclk, 24576000)) {
					pr_err("set codec_moduleclk rate fail\n");
				}
				reg_val = readl(baseaddr + SUNXI_DAC_FIFOC);
				reg_val &=~(7<<29); 
				reg_val |=(2<<29);
				writel(reg_val, baseaddr + SUNXI_DAC_FIFOC);
				break;
			case 16000:
				if (clk_set_rate(codec_pll, 24576000)) {
					pr_err("set codec_pll rate fail\n");
				}
				if (clk_set_rate(codec_moduleclk, 24576000)) {
					pr_err("set codec_moduleclk rate fail\n");
				}
				reg_val = readl(baseaddr + SUNXI_DAC_FIFOC);
				reg_val &=~(7<<29); 
				reg_val |=(3<<29);
				writel(reg_val, baseaddr + SUNXI_DAC_FIFOC);
				break;
			case 12000:
				if (clk_set_rate(codec_pll, 24576000)) {
					pr_err("set codec_pll rate fail\n");
				}
				if (clk_set_rate(codec_moduleclk, 24576000)) {
					pr_err("set codec_moduleclk rate fail\n");
				}
				reg_val = readl(baseaddr + SUNXI_DAC_FIFOC);
				reg_val &=~(7<<29); 
				reg_val |=(4<<29);
				writel(reg_val, baseaddr + SUNXI_DAC_FIFOC);
				break;
			case 8000:
				if (clk_set_rate(codec_pll, 24576000)) {
					pr_err("set codec_pll rate fail\n");
				}
				if (clk_set_rate(codec_moduleclk, 24576000)) {
					pr_err("set codec_moduleclk rate fail\n");
				}
				reg_val = readl(baseaddr + SUNXI_DAC_FIFOC);
				reg_val &=~(7<<29);
				reg_val |=(5<<29);
				writel(reg_val, baseaddr + SUNXI_DAC_FIFOC);
				break;
			default:
				if (clk_set_rate(codec_pll, 24576000)) {
					pr_err("set codec_pll rate fail\n");
				}
				if (clk_set_rate(codec_moduleclk, 24576000)) {
					pr_err("set codec_moduleclk rate fail\n");
				}
				reg_val = readl(baseaddr + SUNXI_DAC_FIFOC);
				reg_val &=~(7<<29); 
				reg_val |=(0<<29);
				writel(reg_val, baseaddr + SUNXI_DAC_FIFOC);
				break;
		}
		switch (substream->runtime->channels) {
			case 1:
				reg_val = readl(baseaddr + SUNXI_DAC_FIFOC);
				reg_val |=(1<<6);
				writel(reg_val, baseaddr + SUNXI_DAC_FIFOC);
				break;
			case 2:
				reg_val = readl(baseaddr + SUNXI_DAC_FIFOC);
				reg_val &=~(1<<6);
				writel(reg_val, baseaddr + SUNXI_DAC_FIFOC);
				break;
			default:
				reg_val = readl(baseaddr + SUNXI_DAC_FIFOC);
				reg_val &=~(1<<6);
				writel(reg_val, baseaddr + SUNXI_DAC_FIFOC);
				break;
		}
	} else {
		switch (substream->runtime->rate) {
			case 44100:
				if (clk_set_rate(codec_pll, 22579200)) {
					pr_err("set codec_pll rate fail\n");
				}
				if (clk_set_rate(codec_moduleclk, 22579200)) {
					pr_err("set codec_moduleclk rate fail\n");
				}
				reg_val = readl(baseaddr + SUNXI_ADC_FIFOC);
				reg_val &=~(7<<29);
				reg_val |=(0<<29);
				writel(reg_val, baseaddr + SUNXI_ADC_FIFOC);
				break;
			case 22050:
				if (clk_set_rate(codec_pll, 22579200)) {
					pr_err("set codec_pll rate fail\n");
				}
				if (clk_set_rate(codec_moduleclk, 22579200)) {
					pr_err("set codec_moduleclk rate fail\n");
				}
				reg_val = readl(baseaddr + SUNXI_ADC_FIFOC);
				reg_val &=~(7<<29); 
				reg_val |=(2<<29);
				writel(reg_val, baseaddr + SUNXI_ADC_FIFOC);
				break;
			case 11025:
				if (clk_set_rate(codec_pll, 22579200)) {
					pr_err("set codec_pll rate fail\n");
				}
				if (clk_set_rate(codec_moduleclk, 22579200)) {
					pr_err("set codec_moduleclk rate fail\n");
				}
				reg_val = readl(baseaddr + SUNXI_ADC_FIFOC);
				reg_val &=~(7<<29); 
				reg_val |=(4<<29);
				writel(reg_val, baseaddr + SUNXI_ADC_FIFOC);
				break;
			case 48000:
				if (clk_set_rate(codec_pll, 24576000)) {
					pr_err("set codec_pll rate fail\n");
				}
				if (clk_set_rate(codec_moduleclk, 24576000)) {
					pr_err("set codec_moduleclk rate fail\n");
				}
				reg_val = readl(baseaddr + SUNXI_ADC_FIFOC);
				reg_val &=~(7<<29); 
				reg_val |=(0<<29);
				writel(reg_val, baseaddr + SUNXI_ADC_FIFOC);
				break;
			case 32000:
				if (clk_set_rate(codec_pll, 24576000)) {
					pr_err("set codec_pll rate fail\n");
				}
				if (clk_set_rate(codec_moduleclk, 24576000)) {
					pr_err("set codec_moduleclk rate fail\n");
				}
				reg_val = readl(baseaddr + SUNXI_ADC_FIFOC);
				reg_val &=~(7<<29); 
				reg_val |=(1<<29);
				writel(reg_val, baseaddr + SUNXI_ADC_FIFOC);
				break;
			case 24000:
				if (clk_set_rate(codec_pll, 24576000)) {
					pr_err("set codec_pll rate fail\n");
				}
				if (clk_set_rate(codec_moduleclk, 24576000)) {
					pr_err("set codec_moduleclk rate fail\n");
				}
				reg_val = readl(baseaddr + SUNXI_ADC_FIFOC);
				reg_val &=~(7<<29); 
				reg_val |=(2<<29);
				writel(reg_val, baseaddr + SUNXI_ADC_FIFOC);
				break;
			case 16000:
				if (clk_set_rate(codec_pll, 24576000)) {
					pr_err("set codec_pll rate fail\n");
				}
				if (clk_set_rate(codec_moduleclk, 24576000)) {
					pr_err("set codec_moduleclk rate fail\n");
				}
				reg_val = readl(baseaddr + SUNXI_ADC_FIFOC);
				reg_val &=~(7<<29); 
				reg_val |=(3<<29);
				writel(reg_val, baseaddr + SUNXI_ADC_FIFOC);
				break;
			case 12000:
				if (clk_set_rate(codec_pll, 24576000)) {
					pr_err("set codec_pll rate fail\n");
				}
				if (clk_set_rate(codec_moduleclk, 24576000)) {
					pr_err("set codec_moduleclk rate fail\n");
				}
				reg_val = readl(baseaddr + SUNXI_ADC_FIFOC);
				reg_val &=~(7<<29); 
				reg_val |=(4<<29);
				writel(reg_val, baseaddr + SUNXI_ADC_FIFOC);
				break;
			case 8000:
				if (clk_set_rate(codec_pll, 24576000)) {
					pr_err("set codec_pll rate fail\n");
				}
				if (clk_set_rate(codec_moduleclk, 24576000)) {
					pr_err("set codec_moduleclk rate fail\n");
				}
				reg_val = readl(baseaddr + SUNXI_ADC_FIFOC);
				reg_val &=~(7<<29); 
				reg_val |=(5<<29);
				writel(reg_val, baseaddr + SUNXI_ADC_FIFOC);
				break;
			default:
				if (clk_set_rate(codec_pll, 24576000)) {
					pr_err("set codec_pll rate fail\n");
				}
				if (clk_set_rate(codec_moduleclk, 24576000)) {
					pr_err("set codec_moduleclk rate fail\n");
				}
				reg_val = readl(baseaddr + SUNXI_ADC_FIFOC);
				reg_val &=~(7<<29);
				reg_val |=(0<<29);
				writel(reg_val, baseaddr + SUNXI_ADC_FIFOC);		
				break;
		}
		switch (substream->runtime->channels) {
			case 1:
				reg_val = readl(baseaddr + SUNXI_ADC_FIFOC);
				reg_val |=(1<<7);
				writel(reg_val, baseaddr + SUNXI_ADC_FIFOC);
			break;
			case 2:
				reg_val = readl(baseaddr + SUNXI_ADC_FIFOC);
				reg_val &=~(1<<7);
				writel(reg_val, baseaddr + SUNXI_ADC_FIFOC);
			break;
			default:
				reg_val = readl(baseaddr + SUNXI_ADC_FIFOC);
				reg_val &=~(1<<7);
				writel(reg_val, baseaddr + SUNXI_ADC_FIFOC);
			break;
		}
	}
	
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		play_ret = codec_pa_play_open();
		codec_play_start();
		return play_ret;
	} else {
   		codec_capture_open();
		return capture_ret;
	}
}

static int sndpcm_trigger(struct snd_pcm_substream *substream,
                              int cmd, struct snd_soc_dai *dai)
{
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		switch (cmd) {
			case SNDRV_PCM_TRIGGER_START:
			case SNDRV_PCM_TRIGGER_RESUME:
			case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
				return 0;
			case SNDRV_PCM_TRIGGER_SUSPEND:
			case SNDRV_PCM_TRIGGER_STOP:
			case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
				codec_play_stop();
				return 0;
			default:
				return -EINVAL;
			}
	} else {
		switch (cmd) {
		case SNDRV_PCM_TRIGGER_START:
		case SNDRV_PCM_TRIGGER_RESUME:
		case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
			return 0;
		case SNDRV_PCM_TRIGGER_SUSPEND:
		case SNDRV_PCM_TRIGGER_STOP:
		case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
			codec_capture_stop();
			return 0;
		default:
			pr_err("error:%s,%d\n", __func__, __LINE__);
			return -EINVAL;
		}
	}
	return 0;
}
static int sndpcm_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params,
	struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct sunxi_dma_params *dma_data;

	/* play or record */
	if(substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		dma_data = &sunxi_pcm_pcm_stereo_out;
	else
		dma_data = &sunxi_pcm_pcm_stereo_in;

	snd_soc_dai_set_dma_data(rtd->cpu_dai, substream, dma_data);
	return 0;
}

static int sndpcm_set_dai_sysclk(struct snd_soc_dai *codec_dai,
				  int clk_id, unsigned int freq, int dir)
{
	return 0;
}

static int sndpcm_set_dai_clkdiv(struct snd_soc_dai *codec_dai, int div_id, int div)
{
	return 0;
}

static int sndpcm_set_dai_fmt(struct snd_soc_dai *codec_dai,
			       unsigned int fmt)
{
	return 0;
}

static const char *spk_headset_earpiece_function[] = {"headset", "spk", "spk_headset"};
static const struct soc_enum spk_headset_earpiece_enum[] = {
        SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(spk_headset_earpiece_function), spk_headset_earpiece_function),
};

static const struct snd_kcontrol_new sunxi_codec_controls[] = {
	/*SUNXI_PA_CTRL = 0x24*/
	CODEC_SINGLE("MIC1_G boost stage output mixer control", 	MIC_GCTR, MIC1G, 0x7, 0),
	CODEC_SINGLE("MIC2_G boost stage output mixer control", 	MIC_GCTR, MIC2G, 0x7, 0),
	CODEC_SINGLE("LINEIN_G boost stage output mixer control", 	LINEIN_GCTR, LINEING, 0x7, 0),

	/*SUNXI_MIC_CTRL = 0x28*/
	CODEC_SINGLE("MIC1 boost AMP gain control", 				MIC1G_MICBIAS_CTR, MIC1BOOST, 0x7, 0),
	CODEC_SINGLE("MIC2 boost AMP gain control", 				MIC2G_LINEOUT_CTR, MIC2BOOST, 0x7, 0),

	CODEC_SINGLE("Lineout volume control", 						LINEOUT_VOLC, LINEOUTVOL, 0x1f, 0),

	/*SUNXI_ADC_ACTL = 0x2c*/
	CODEC_SINGLE("ADC input gain ctrl", 						ADC_AP_EN, ADCG, 0x7, 0),
};

static struct snd_soc_dai_ops sndpcm_dai_ops = {
	.startup = sndpcm_startup,
	.shutdown = sndpcm_shutdown,
	.prepare  =	sndpcm_perpare,
	.trigger 	= sndpcm_trigger,
	.hw_params = sndpcm_hw_params,
	.digital_mute = sndpcm_mute,
	.set_sysclk = sndpcm_set_dai_sysclk,
	.set_clkdiv = sndpcm_set_dai_clkdiv,
	.set_fmt = sndpcm_set_dai_fmt,
};

static struct snd_soc_dai_driver sndpcm_dai = {
	.name = "sndcodec",
	/* playback capabilities */
	.playback = {
		.stream_name = "Playback",
		.channels_min = 1,
		.channels_max = 2,
		.rates = sndpcm_RATES,
		.formats = sndpcm_FORMATS,
	},
	.capture = {
		.stream_name = "Capture",
		.channels_min = 1,
		.channels_max = 2,
		.rates = sndpcm_RATES,
		.formats = sndpcm_FORMATS,
	},
	/* pcm operations */
	.ops = &sndpcm_dai_ops,
};
EXPORT_SYMBOL(sndpcm_dai);

static int sndpcm_soc_probe(struct snd_soc_codec *codec)
{
	/* Add virtual switch */
	snd_soc_add_codec_controls(codec, sunxi_codec_controls,
					ARRAY_SIZE(sunxi_codec_controls));

	return 0;
}
#ifdef CONFIG_PM
static int sndpcm_suspend(struct snd_soc_codec *codec)
{
	/* check if called in talking standby */
	if (check_scene_locked(SCENE_TALKING_STANDBY) == 0) {
		pr_debug("In talking standby, audio codec do not suspend!!\n");
		return 0;
	}

	codec_wr_prcm_control(MIC1G_MICBIAS_CTR, 0x1, MMICBIASEN, 0x0);
	codec_wr_prcm_control(MIC1G_MICBIAS_CTR, 0x1, HMICBIAS_MODE, 0x0);

	codec_wr_prcm_control(DAC_PA_SRC, 0x1, LMIXEN, 0x0);
	codec_wr_prcm_control(DAC_PA_SRC, 0x1, RMIXEN, 0x0);

	codec_wr_prcm_control(DAC_PA_SRC, 0x1, DACALEN, 0x0);
	codec_wr_prcm_control(DAC_PA_SRC, 0x1, DACAREN, 0x0);
	/*disable dac digital*/
	codec_wr_control(SUNXI_DAC_DPC ,  0x1, DAC_EN, 0x0);

	codec_wr_prcm_control(ADDA_APT2, 0x1, ZERO_CROSS_EN, 0x0);

	//codec_wr_prcm_control(LINEOUT_VOLC, 0x1f, LINEOUTVOL, 0x0);
	gpio_set_value(item.gpio.gpio, 0);

	if ((NULL == codec_moduleclk)||(IS_ERR(codec_moduleclk))) {
		pr_err("codec_moduleclk handle is invaled, just return\n");
	} else {
		clk_disable_unprepare(codec_moduleclk);
	}

	pr_debug("[audio codec]:suspend end\n");
	return 0;
}

static int sndpcm_resume(struct snd_soc_codec *codec)
{

	pr_debug("[audio codec]:resume start\n");

	if (clk_prepare_enable(codec_moduleclk)) {
		pr_err("open codec_moduleclk failed; \n");
	}

	if (headphone_direct_used) {
		codec_wr_prcm_control(PAEN_CTR, 0x3, HPCOM_FC, 0x3);
		codec_wr_prcm_control(PAEN_CTR, 0x1, COMPTEN, 0x1);
	} else {
		codec_wr_prcm_control(PAEN_CTR, 0x3, HPCOM_FC, 0x0);
		codec_wr_prcm_control(PAEN_CTR, 0x1, COMPTEN, 0x0);
	}

	/*process for normal standby*/
	if (NORMAL_STANDBY == standby_type) {
	/*process for super standby*/
	} else if(SUPER_STANDBY == standby_type) {
		/*when TX FIFO available room less than or equal N,
		* DRQ Requeest will be de-asserted.
		*/
		codec_wr_control(SUNXI_DAC_FIFOC, 0x3, DAC_DRQ_EN_CLR_CNT,0x3);
		/*write 1 to flush tx fifo*/
		codec_wr_control(SUNXI_DAC_FIFOC, 0x1, FIFO_FLUSH, 0x1);
		/*write 1 to flush rx fifo*/
		codec_wr_control(SUNXI_ADC_FIFOC, 0x1, ADC_FIFO_FLUSH, 0x1);
	}

	codec_wr_control(SUNXI_DAC_FIFOC, 0x1, FIR_VER, 0x1);

	pr_debug("[audio codec]:resume end\n");
	return 0;
}
#endif
/* power down chip */
static int sndpcm_soc_remove(struct snd_soc_codec *codec)
{
	return 0;
}

static struct snd_soc_codec_driver soc_codec_dev_sndpcm = {
	.probe 	 =	sndpcm_soc_probe,
	.remove  =  sndpcm_soc_remove,
#ifdef CONFIG_PM
	.suspend = 	sndpcm_suspend,
	.resume  =  sndpcm_resume,
#endif
};


static ssize_t show_audio_reg(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	int count = 0;
	int i = 0;
	int reg_group =0;

	count += sprintf(buf, "dump audio reg:\n");

	while (reg_labels[i].name != NULL){
		if (reg_labels[i].value == 0){
			reg_group++;
		} 
		if (reg_group == 1){
			count +=sprintf(buf + count, "%s 0x%p: 0x%x\n", reg_labels[i].name, 
							(baseaddr + reg_labels[i].value), 
							readl(baseaddr + reg_labels[i].value) );
		} else 	if (reg_group == 2){
			count +=sprintf(buf + count, "%s 0x%x: 0x%x\n", reg_labels[i].name, 
							(reg_labels[i].value), 
						        read_prcm_wvalue(reg_labels[i].value) );
		}
		i++;
	}

	return count;
}

/* ex: 
   echo "1,0x0,0xffffffff" > store_audio
   echo "2,0x0,0xff" > store_audio
*/
static ssize_t store_audio_reg(struct device *dev, struct device_attribute *attr,
		const char *buf, size_t count)
{
	int ret;
	int reg_group =0;
	int find_labels_reg_offset =-1;
	int input_reg_group =0;
	int input_reg_offset =0;
	int input_reg_val =0;
	int i = 0;

	ret = sscanf(buf, "%d,0x%x,0x%x", &input_reg_group, &input_reg_offset, &input_reg_val);
	pr_err("ret:%d, reg_group:%d, reg_offset:%d, reg_val:0x%x", ret, input_reg_group, input_reg_offset, input_reg_val);
	if (ret != 3){
		ret = count;
		goto out;
	}

	if (!(input_reg_group ==1 || input_reg_group ==2)){
		pr_err("not exist reg group\n");
		ret = count;
		goto out;
	}

	while (reg_labels[i].name != NULL){
		if (reg_labels[i].value == 0){
			reg_group++;
		} 

		if ( (input_reg_group == reg_group) ){
			if ( reg_labels[i].value == input_reg_offset ){
				find_labels_reg_offset = i;
				break;
			}
		}

		i++;
	}

	if (find_labels_reg_offset < 0){
		pr_err("not exist reg offset\n");
		ret = count;
		goto out;
	}

	if (reg_group == 1){
		writel(input_reg_val, baseaddr + reg_labels[find_labels_reg_offset].value);	
	} else if (reg_group == 2){
		write_prcm_wvalue(reg_labels[find_labels_reg_offset].value, input_reg_val & 0xff);
	}

	ret = count;

out:
	return ret;
}

static DEVICE_ATTR(audio_reg, 0644, show_audio_reg, store_audio_reg);

static struct attribute *audio_debug_attrs[] = {
	&dev_attr_audio_reg.attr,
	NULL,
};

static struct attribute_group audio_debug_attr_group = {
	.name   = "audio_reg_debug",
	.attrs  = audio_debug_attrs,
};


static int __init sndpcm_codec_probe(struct platform_device *pdev)
{
	int err = -1;
	int req_status;

	baseaddr = ioremap(CODEC_BASSADDRESS, 0x50);
	if (baseaddr == NULL)
		return -ENXIO;
	/* codec_pll */
	codec_pll = clk_get(NULL, "pll_audio");
	if ((!codec_pll)||(IS_ERR(codec_pll))) {
		pr_err("try to get codec_pll failed!\n");
	}
	if (clk_prepare_enable(codec_pll)) {
		pr_err("enable codec_pll failed; \n");
	}
	/* codec_moduleclk */
	codec_moduleclk = clk_get(NULL, "adda");
	if ((!codec_moduleclk)||(IS_ERR(codec_moduleclk))) {
		pr_err("try to get codec_moduleclk failed!\n");
	}
	if (clk_set_parent(codec_moduleclk, codec_pll)) {
		pr_err("err:try to set parent of codec_moduleclk to codec_pll failed!\n");
	}
	if (clk_set_rate(codec_moduleclk, 24576000)) {
		pr_err("err:set codec_moduleclk clock freq 24576000 failed!\n");
	}
	if (clk_prepare_enable(codec_moduleclk)) {
		pr_err("err:open codec_moduleclk failed; \n");
	}

	codec_init();
	
	/* check if hp_vcc_ldo exist, if exist enable it */
	type = script_get_item("audio0", "audio_hp_ldo", &item);
	if (SCIRPT_ITEM_VALUE_TYPE_STR != type) {
		pr_err("script_get_item return type err, consider it no ldo\n");
	} else {
		if (!strcmp(item.str, "none"))
			hp_ldo = NULL;
		else {
			hp_ldo_str = item.str;
			hp_ldo = regulator_get(NULL, hp_ldo_str);
			if (!hp_ldo) {
				pr_err("get audio hp-vcc(%s) failed\n", hp_ldo_str);
				return -EFAULT;
			}
			regulator_set_voltage(hp_ldo, 3000000, 3000000);
			regulator_enable(hp_ldo);
		}
	}
	/*get the default pa val(close)*/
	type = script_get_item("audio0", "audio_pa_ctrl", &item);
	if (SCIRPT_ITEM_VALUE_TYPE_PIO != type) {
		pr_err("script_get_item return type err\n");
		return -EFAULT;
	}
	/*request gpio*/
	req_status = gpio_request(item.gpio.gpio, NULL);
	if (0 != req_status) {
		pr_err("request gpio failed!\n");
	}
	gpio_direction_output(item.gpio.gpio, 1);

	gpio_set_value(item.gpio.gpio, 0);
	
	snd_soc_register_codec(&pdev->dev, &soc_codec_dev_sndpcm, &sndpcm_dai, 1);

       err=sysfs_create_group(&pdev->dev.kobj, &audio_debug_attr_group);
       if (err){
		pr_err("failed to create attr group\n");
       }

	return 0;
}

static int __exit sndpcm_codec_remove(struct platform_device *pdev)
{
	if ((NULL == codec_moduleclk)||(IS_ERR(codec_moduleclk))) {
		pr_err("codec_moduleclk handle is invaled, just return\n");
		return -EINVAL;
	} else {
		clk_disable_unprepare(codec_moduleclk);
	}
	if ((NULL == codec_pll)||(IS_ERR(codec_pll))) {
		pr_err("codec_pll handle is invaled, just return\n");
		return -EINVAL;
	} else {
		clk_put(codec_pll);
	}

	/* disable audio hp-vcc ldo if it exist */
	if (hp_ldo) {
		regulator_disable(hp_ldo);
		hp_ldo = NULL;
	}
 	
	sysfs_remove_group(&pdev->dev.kobj, &audio_debug_attr_group);

	snd_soc_unregister_codec(&pdev->dev);
	return 0;
}

static void sunxi_codec_shutdown(struct platform_device *devptr)
{
	item.gpio.data = 0;

	codec_wr_prcm_control(ADDA_APT2, 0x1, ZERO_CROSS_EN, 0x0);

	codec_wr_prcm_control(DAC_PA_SRC, 0x1, DACALEN, 0x0);
	codec_wr_prcm_control(DAC_PA_SRC, 0x1, DACAREN, 0x0);

	/*disable dac digital*/
	codec_wr_control(SUNXI_DAC_DPC ,  0x1, DAC_EN, 0x0);

	codec_wr_prcm_control(MIC1G_MICBIAS_CTR, 0x1, MMICBIASEN, 0x0);

	//codec_wr_prcm_control(LINEOUT_VOLC, 0x1f, LINEOUTVOL, 0x0);
	gpio_set_value(item.gpio.gpio, 0);

	if ((NULL == codec_moduleclk)||(IS_ERR(codec_moduleclk))) {
		pr_err("codec_moduleclk handle is invaled, just return\n");
	} else {
		clk_disable_unprepare(codec_moduleclk);
	}
}

/*data relating*/
static struct platform_device sndpcm_codec_device = {
	.name = "sunxi-pcm-codec",
	.id = -1,
};

/*method relating*/
static struct platform_driver sndpcm_codec_driver = {
	.driver = {
		.name = "sunxi-pcm-codec",
		.owner = THIS_MODULE,
	},
	.probe = sndpcm_codec_probe,
	.remove = __exit_p(sndpcm_codec_remove),
	.shutdown = sunxi_codec_shutdown,
};

static int __init sndpcm_codec_init(void)
{
	int err = 0;

	if((err = platform_device_register(&sndpcm_codec_device)) < 0)
		return err;

	if ((err = platform_driver_register(&sndpcm_codec_driver)) < 0)
		return err;
	
	return 0;
}
module_init(sndpcm_codec_init);

static void __exit sndpcm_codec_exit(void)
{
	platform_driver_unregister(&sndpcm_codec_driver);
}
module_exit(sndpcm_codec_exit);

MODULE_DESCRIPTION("SNDPCM ALSA soc codec driver");
MODULE_AUTHOR("Zoltan Devai, Christian Pellegrin <chripell@evolware.org>");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:sunxi-pcm-codec");
