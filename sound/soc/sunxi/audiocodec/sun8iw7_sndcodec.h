/*
 * sound\soc\sunxi\audiocodec\sun8iw3-sndcodec.h
 * (C) Copyright 2010-2016
 * Reuuimlla Technology Co., Ltd. <www.reuuimllatech.com>
 * huangxin <huangxin@reuuimllatech.com>
 *
 * some simple description for this code
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 */
#ifndef _SUN8IW3_CODEC_H
#define _SUN8IW3_CODEC_H
#include <mach/platform.h>
/*Codec Register*/
#define CODEC_BASSADDRESS         (0x01c22c00)
#define SUNXI_DAC_DPC                (0x00)
#define SUNXI_DAC_FIFOC              (0x04)
#define SUNXI_DAC_FIFOS              (0x08)

#define SUNXI_ADC_FIFOC              (0x10)
#define SUNXI_ADC_FIFOS              (0x14)
#define SUNXI_ADC_RXDATA			 (0x18)

#define SUNXI_DAC_TXDATA             (0x20)

#define SUNXI_DAC_DEBUG              (0x48)
#define SUNXI_ADC_DEBUG              (0x4c)

#define SUNXI_DAC_DAP_CTR            (0x60)
#define SUNXI_DAC_DAP_VOL            (0x64)
#define SUNXI_DAC_DAP_COFF           (0x68)
#define SUNXI_DAC_DAP_OPT            (0x6c)

#define SUNXI_ADC_DAP_CTR            (0x70)
#define SUNXI_ADC_DAP_LCTR           (0x74)
#define SUNXI_ADC_DAP_RCTR           (0x78)
#define SUNXI_ADC_DAP_PARA           (0x7C)
#define SUNXI_ADC_DAP_LAC	         (0x80)
#define SUNXI_ADC_DAP_LDAT	         (0x84)
#define SUNXI_ADC_DAP_RAC	         (0x88)
#define SUNXI_ADC_DAP_RDAT	         (0x8C)
#define SUNXI_ADC_DAP_HPFC	         (0x90)
#define SUNXI_ADC_DAP_LINAC	         (0x94)
#define SUNXI_ADC_DAP_RINAC	         (0x98)
#define SUNXI_ADC_DAP_ORT	         (0x9C)

/*DAC Digital Part Control Register 
* codecbase+0x00
* SUNXI_DAC_DPC
*/
#define DAC_EN                    (31)
#define HPF_EN					  (18)
#define DIGITAL_VOL               (12)
#define HUB_EN					  (0)

/*DAC FIFO Control Register 
* codecbase+0x04
* SUNXI_DAC_FIFOC
*/
#define DAC_FS					  (29)
#define FIR_VER					  (28)
#define SEND_LASAT                (26)
#define FIFO_MODE              	  (24)
#define DAC_DRQ_CLR_CNT           (21)
#define TX_TRI_LEVEL              (8)
#define ADDA_LOOP_EN			  (7)
#define DAC_MONO_EN               (6)
#define TX_SAMPLE_BITS            (5)
#define DAC_DRQ_EN                (4)
#define FIFO_FLUSH            	  (0)

/*ADC FIFO Control Register
* codecbase+0x10
* SUNXI_ADC_FIFOC
*/
#define ADFS					  (29)
#define EN_AD                	  (28)
#define RX_FIFO_MODE              (24)
#define ADCFDT					  (17)
#define ADCDFEN					  (16)
#define RX_FIFO_TRG_LEVEL         (8)
#define ADC_MONO_EN               (7)
#define RX_SAMPLE_BITS            (6)
#define ADC_DRQ_EN                (4)
#define ADC_FIFO_FLUSH            (0)

/*DAC Debug Register
* codecbase+0x48
* SUNXI_DAC_DEBUG
*/
#define DAC_MODU_SELECT			  (11)
#define DAC_PATTERN_SELECT		  (9)
#define DAC_SWP					  (6)

/*DAC Debug Register
* codecbase+0x4C
* SUNXI_ADC_DEBUG
*/
#define AD_SWP					  (24)

/*DAC Debug Register
* codecbase+0x60
* SUNXI_DAC_DAP_CTR
*/
#define DDAP_EN					  (31)
#define DDAP_START				  (30)
#define DDAP_STATE				  (29)
#define DDAP_BQ_EN				  (16)
#define DDAP_DRC_EN				  (15)
#define DDAP_HPF_EN				  (14)
#define DDAP_DE_CTL				  (12)
#define RAM_ADDR				  (0)

/*DAC Debug Register
* codecbase+0x64
* SUNXI_DAC_DAP_VOL
*/
#define DDAP_LCHAN_MUTE			  (30)
#define DDAP_RCHAN_MUTE			  (29)
#define DDAP_MMUTE			  	  (28)
#define DDAP_SKEW_CTL		  	  (24)
#define M_GAIN				  	  (16)
#define DDAP_LCHAN_GAIN		  	  (8)
#define DDAP_RCHAN_GAIN		  	  (0)

/*DAC Debug Register
* codecbase+0x68
* SUNXI_DAC_DAP_COFF
*/
#define DDAP_COF				  (0)

/*DAC Debug Register
* codecbase+0x6c
* SUNXI_DAC_DAP_OPT
*/
#define DDAP_OPT				  (0)

/*DAC Debug Register
* codecbase+0x70
* SUNXI_ADC_DAP_CTR
*/
#define DAP_EN					(31)
#define ADAP_START				(30)
#define ADAP_LSATU_FLAG			(21)
#define ADAP_LNOI_FLAG			(20)
#define ADAP_LCHAN_GAIN			(12)
#define ADAP_RSATU_FLAG			(9)
#define ADAP_RNOI_FLAG			(8)
#define ADAP_RCHAN_GAIN			(0)

/*DAC Debug Register
* codecbase+0x74
* SUNXI_ADC_DAP_LCTR
*/
#define ADAP_LNOI_SET			(16)
#define AAGC_LCHAN_EN			(14)
#define ADAP_LHPF_EN			(13)
#define ADAP_LNOI_DET			(12)
#define ADAP_LCHAN_HYS			(8)
#define ADAP_LNOI_DEB			(7)
#define ADAP_LSIG_DEB			(0)

/*DAC Debug Register
* codecbase+0x78
* SUNXI_ADC_DAP_RCTR
*/
#define ADAP_RNOI_SET			(16)
#define AAGC_RCHAN_EN			(14)
#define ADAP_RHPF_EN			(13)
#define ADAP_RNOI_DET			(12)
#define ADAP_RCHAN_HYS			(8)
#define ADAP_RNOI_DEB			(4)
#define ADAP_RSIG_DEB			(0)

/*DAC Debug Register
* codecbase+0x7c
* SUNXI_ADC_DAP_PARA
*/
#define ADAP_LTARG_SET			(24)
#define ADAP_RTARG_SET			(16)
#define ADAP_LGAIN_MAX			(8)
#define ADAP_RGAIN_MAX			(0)

/*DAC Debug Register
* codecbase+0x80
* SUNXI_ADC_DAP_LAC
*/
#define ADAP_LAC				(0)

/*DAC Debug Register
* codecbase+0x84
* SUNXI_ADC_DAP_LDAT
*/
#define ADAP_LATT_SET			(16)
#define ADAP_LDEC_SET			(0)

/*DAC Debug Register
* codecbase+0x88
* SUNXI_ADC_DAP_RAC
*/
#define ADAP_RAC				(0)

/*DAC Debug Register
* codecbase+0x8c
* SUNXI_ADC_DAP_RDAT
*/
#define ADAP_RATT_SET			(16)
#define ADAP_RDEC_SET			(0)

/*DAC Debug Register
* codecbase+0x90
* SUNXI_ADC_DAP_HPFC
*/
#define ADAP_HPFC				(0)

/*DAC Debug Register
* codecbase+0x94
* SUNXI_ADC_DAP_LINAC
*/
#define ADAP_LINAC				(0)

/*DAC Debug Register
* codecbase+0x98
* SUNXI_ADC_DAP_RINAC
*/
#define ADAP_RINAC				(0)

/*DAC Debug Register
* codecbase+0x9c
* SUNXI_ADC_DAP_ORT
*/
#define L_ENERGY_VAL			(10)
#define L_CHANNEL_GAIN			(8)
#define INPUT_SIGNAL_FILT		(5)
#define AGC_OUTPUT_NOISE_STATE	(4)
#define R_ENERGY_VAL			(2)
#define R_CHANNEL_GAIN			(0)

#define ADDA_PR_CFG_REG     	  (SUNXI_R_PRCM_VBASE+0x1c0)
#define LINEOUT_PA_GAT			  (0x00)
#define LOMIXSC					  (0x01)
#define ROMIXSC					  (0x02)
#define DAC_PA_SRC				  (0x03)
#define LINEIN_GCTR				  (0x05)
#define MIC_GCTR				  (0x06)
#define PAEN_CTR				  (0x07)
#define LINEOUT_VOLC			  (0x09)
#define MIC2G_LINEOUT_CTR		  (0x0A)
#define MIC1G_MICBIAS_CTR		  (0x0B)
#define LADCMIXSC		  		  (0x0C)
#define RADCMIXSC				  (0x0D)
#define ADC_AP_EN				  (0x0F)
#define ADDA_APT0				  (0x10)
#define ADDA_APT1				  (0x11)
#define ADDA_APT2				  (0x12)
#define BIAS_DA16_CTR0			  (0x13)
#define BIAS_DA16_CTR1			  (0x14)
#define DA16CAL					  (0x15)
#define DA16VERIFY				  (0x16)
#define BIASCALI				  (0x17)
#define BIASVERIFY				  (0x18)

/*
*	apb0 base
*	0x00 LINEOUT_PA_GAT
*/
#define PA_CLK_GC		(7)

/*
*	apb0 base
*	0x01 LOMIXSC
*/
#define LMIXMUTE				  (0)
#define LMIXMUTEDACR			  (0)
#define LMIXMUTEDACL			  (1)
#define LMIXMUTELINEINL			  (2)
#define LMIXMUTEMIC2BOOST		  (5)
#define LMIXMUTEMIC1BOOST		  (6)

/*
*	apb0 base
*	0x02 ROMIXSC
*/
#define RMIXMUTE				  (0)
#define RMIXMUTEDACL			  (0)
#define RMIXMUTEDACR			  (1)
#define RMIXMUTELINEINR			  (2)
#define RMIXMUTEMIC2BOOST		  (5)
#define RMIXMUTEMIC1BOOST		  (6)

/*
*	apb0 base
*	0x03 DAC_PA_SRC
*/
#define DACAREN			(7)
#define DACALEN			(6)
#define RMIXEN			(5)
#define LMIXEN			(4)

/*
*	apb0 base
*	0x05 LINEIN_GCTR
*/
#define LINEING			(4)

/*
*	apb0 base
*	0x06 MIC_GCTR
*/
#define MIC1G			(4)
#define MIC2G			(0)

/*
*	apb0 base
*	0x07 PAEN_CTR
*/
#define LINEOUTEN		 (7)
#define PA_ANTI_POP_CTRL (2)

/*
*	apb0 base
*	0x09 LINEOUT_VOLC
*/
#define LINEOUTVOL		 (3)

/*
*	apb0 base
*	0x0A MIC2G_LINEOUT_CTR
*/
#define MIC2AMPEN		 (7)
#define MIC2BOOST		 (4)
#define LINEOUTL_EN		 (3)
#define LINEOUTR_EN		 (2)
#define LINEOUTL_SS		 (1)
#define LINEOUTR_SS		 (0)

/*
*	apb0 base
*	0x0B MIC1G_MICBIAS_CTR
*/
#define MMICBIASEN		 (6)
#define MIC1AMPEN		 (3)
#define MIC1BOOST		 (0)

/*
*	apb0 base
*	0x0C LADCMIXSC
*/
#define LADCMIXMUTE		 		  (0)
#define LADCMIXMUTEMIC1BOOST	  (6)
#define LADCMIXMUTEMIC2BOOST	  (5)
#define LADCMIXMUTELINEINL		  (2)
#define LADCMIXMUTELOUTPUT		  (1)
#define LADCMIXMUTEROUTPUT		  (0)

/*
*	apb0 base
*	0x0D RADCMIXSC
*/
#define RADCMIXMUTE		          (0)
#define RADCMIXMUTEMIC1BOOST	  (6)
#define RADCMIXMUTEMIC2BOOST	  (5)
#define RADCMIXMUTEPHONEPN		  (4)
#define RADCMIXMUTEPHONEP		  (3)
#define RADCMIXMUTELINEINR		  (2)
#define RADCMIXMUTEROUTPUT		  (1)
#define RADCMIXMUTELOUTPUT		  (0)

/*
*	apb0 base
*	0x0F ADC_AP_EN
*/
#define ADCREN			 (7)
#define ADCLEN			 (6)
#define ADCG			 (0)

/*
*	apb0 base
*	0x10 ADDA_APT0
*/
#define OPDRV_OPCOM_CUR				(6)
#define OPADC1_BIAS_CUR				(4)
#define OPADC2_BIAS_CUR				(2)
#define OPAAF_BIAS_CUR				(0)

/*
*	apb0 base
*	0x11 ADDA_APT1
*/
#define OPMIC_BIAS_CUR				(6)
#define OPDAC_BIAS_CUR				(2)
#define OPMIX_BIAS_CUR				(0)

/*
*	apb0 base
*	0x12 ADDA_APT2
*/
#define ZERO_CROSS_EN 	  			(7)
#define TIMEOUT_ZERO_CROSS 			(6)
#define PTDBS						(4)
#define PA_SLOPE_SELECT	  			(3)
#define USB_BIAS_CUR				(0)

/*
*	apb0 base
*	0x13 BIAS_DA16_CTR0
*/
#define MMIC_BIAS_CHOP_EN			(7)
#define MMIC_BIAS_CLK_SEL			(5)
#define DITHER						(4)
#define DITHER_CLK_SELECT			(2)
#define BIHE_CTRL					(0)

/*
*	apb0 base
*	0x14 BIAS_DA16_CTR1
*/
#define PA_SPEED_SEL				(7)
#define CURRENT_TEST_SEL			(6)
#define BIAS_DA16_CAL_CLK_SEL		(4)
#define BIAS_CAL_MODE_SEL			(3)
#define BIAS_DA16_CAL_CTRL			(2)
#define BIASCALIVERIFY				(1)
#define DA16CALIVERIFY				(0)

/*
*	apb0 base
*	0x15 DA16CALI
*/
#define DA16CALI_DATA				(0)		

/*
*	apb0 base
*	0x17 BIASCALI
*/
#define BIASCALI_DATA				(0)

/*
*	apb0 base
*	0x18 BIASVERIFY
*/
#define BIASVERIFY_DATA				(0)

#define codec_rdreg(reg)	  readl((baseaddr+(reg)))
#define codec_wrreg(reg,val)  writel((val),(baseaddr+(reg)))

extern int snd_codec_info_volsw(struct snd_kcontrol *kcontrol,
		struct	snd_ctl_elem_info	*uinfo);
extern int snd_codec_get_volsw(struct snd_kcontrol	*kcontrol,
		struct	snd_ctl_elem_value	*ucontrol);
extern int snd_codec_put_volsw(struct	snd_kcontrol	*kcontrol,
	struct	snd_ctl_elem_value	*ucontrol);

/*
* Convenience kcontrol builders
*/
#define CODEC_SINGLE_VALUE(xreg, xshift, xmax,	xinvert)\
		((unsigned long)&(struct codec_mixer_control)\
		{.reg	=	xreg,	.shift	=	xshift,	.rshift	=	xshift,	.max	=	xmax,\
   	.invert	=	xinvert})

#define CODEC_SINGLE(xname,	reg,	shift,	max,	invert)\
{	.iface	= SNDRV_CTL_ELEM_IFACE_MIXER, .name = xname,\
	.info	= snd_codec_info_volsw,	.get = snd_codec_get_volsw,\
	.put	= snd_codec_put_volsw,\
	.private_value	= CODEC_SINGLE_VALUE(reg, shift, max, invert)}

/*	mixer control*/	
struct	codec_mixer_control{
	int		min;
	int     max;
	int     where;
	unsigned int mask;
	unsigned int reg;
	unsigned int rreg;
	unsigned int shift;
	unsigned int rshift;
	unsigned int invert;
	unsigned int value;
};

#define    DAC_Digital_Part_Control 0x00
#define    DAC_FIFO_Control        0x04
#define    DAC_FIFO_Status         0x08
#define    DAC_TX_DATA             0x0c
#define    ADC_FIFO_Control        0x10
#define    ADC_FIFO_Status         0x14
#define    ADC_RX_DATA             0x18
#define    DAC_TX_Counter          0x40
#define    ADC_RX_Counter          0x44
#define    DAC_Debug               0x48
#define    ADC_Debug               0x4c

#define    Headphone_Volume_Control 			0x0  
#define    Left_Output_Mixer_Source_Control 		0x1  
#define    Right_Output_Mixer_Source_Control 		0x2  
#define    DAC_Analog_Enable_and_PA_Source_Control 	0x3  
#define    Phonein_Stereo_Gain_Control 			0x4  
#define    Linein_and_Phone_P_N_Gain_Control 		0x5  
#define    MIC1_and_MIC2_GAIN_Control 			0x6  
#define    PA_Enable_and_HP_Control 			0x7  
#define    Phoneout_Control 				0x8  
#define    Lineout_Volume_Control 			0x9  
#define    Mic2_Boost_and_Lineout_Enable_Control 	0xA  
#define    Mic1_Boost_and_MICBIAS_Control 		0xB  
#define    Left_ADC_Mixer_Source_Control 		0xC  
#define    Right_ADC_Mixer_Source_Control 		0xD  

#define    PA_UPTIME_CTRL                                   0xE  
#define    ADC_ANALIG_PART_ENABLE_REG                    0xF  


struct label {
    const char *name;
    int value;
};

#define LABEL(constant) { #constant, constant }
#define LABEL_END { NULL, -1 }
static struct label reg_labels[]={
        LABEL(DAC_Digital_Part_Control),
        LABEL(DAC_FIFO_Control),
        LABEL(DAC_FIFO_Status),
        LABEL(DAC_TX_DATA),
        LABEL(ADC_FIFO_Control),
        LABEL(ADC_FIFO_Status),
        LABEL(ADC_RX_DATA),
        LABEL(DAC_TX_Counter),
        LABEL(ADC_RX_Counter),
        LABEL(DAC_Debug),
        LABEL(ADC_Debug),
       
        LABEL(Headphone_Volume_Control), 		  
        LABEL(Left_Output_Mixer_Source_Control), 
        LABEL(Right_Output_Mixer_Source_Control), 
        LABEL(DAC_Analog_Enable_and_PA_Source_Control), 
        LABEL(Phonein_Stereo_Gain_Control), 
        LABEL(Linein_and_Phone_P_N_Gain_Control), 
        LABEL(MIC1_and_MIC2_GAIN_Control),   
        LABEL(PA_Enable_and_HP_Control),  
        LABEL(Phoneout_Control), 
        LABEL(Lineout_Volume_Control), 
        LABEL(Mic2_Boost_and_Lineout_Enable_Control),  
        LABEL(Mic1_Boost_and_MICBIAS_Control), 
        LABEL(Left_ADC_Mixer_Source_Control), 
        LABEL(Right_ADC_Mixer_Source_Control),  
		LABEL(PA_UPTIME_CTRL),
		LABEL(ADC_ANALIG_PART_ENABLE_REG),
	LABEL_END,
};

#endif
