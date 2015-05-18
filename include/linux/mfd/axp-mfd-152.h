#ifndef AXP_MFD_152_HH
#define AXP_MFD_152_HH

enum {
	AXP152_ID_LDO0,
	AXP152_ID_LDO1,
	AXP152_ID_LDO2,
	AXP152_ID_LDO3,
	AXP152_ID_LDO4,
	AXP152_ID_LDO5,
	AXP152_ID_LDOIO0,
	AXP152_ID_DCDC1,
	AXP152_ID_DCDC2,
	AXP152_ID_DCDC3,
	AXP152_ID_DCDC4,
	AXP152_ID_SUPPLY,
	AXP152_ID_GPIO,
};

#define AXP152                       15
#define POWER15_STATUS              (0x00)
#define POWER15_MODE_CHGSTATUS      (0x01)
#define POWER15_OTG_STATUS          (0x02)
#define POWER15_IC_TYPE             (0x03)
#define POWER15_DATA_BUFFER1        (0x04)
#define POWER15_DATA_BUFFER2        (0x05)
#define POWER15_DATA_BUFFER3        (0x06)
#define POWER15_DATA_BUFFER4        (0x07)
#define POWER15_DATA_BUFFER5        (0x08)
#define POWER15_DATA_BUFFER6        (0x09)
#define POWER15_DATA_BUFFER7        (0x0A)
#define POWER15_DATA_BUFFER8        (0x0B)
#define POWER15_DATA_BUFFER9        (0x0C)
#define POWER15_DATA_BUFFERA        (0x0D)
#define POWER15_DATA_BUFFERB        (0x0E)
#define POWER15_DATA_BUFFERC        (0x0F)
#define POWER15_IPS_SET             (0x30)
#define POWER15_VOFF_SET            (0x31)
#define POWER15_OFF_CTL             (0x32)
#define POWER15_CHARGE1             (0x33)
#define POWER15_CHARGE2             (0x34)
#define POWER15_BACKUP_CHG          (0x35)
#define POWER15_PEK_SET             (0x36)
#define POWER15_DCDC_FREQSET        (0x37)
#define POWER15_VLTF_CHGSET         (0x38)
#define POWER15_VHTF_CHGSET         (0x39)
#define POWER15_APS_WARNING1        (0x3A)
#define POWER15_APS_WARNING2        (0x3B)
#define POWER15_TLTF_DISCHGSET      (0x3C)
#define POWER15_THTF_DISCHGSET      (0x3D)
#define POWER15_DCDC_MODESET        (0x80)
#define POWER15_ADC_EN1             (0x82)
#define POWER15_ADC_EN2             (0x83)
#define POWER15_ADC_SPEED           (0x84)
#define POWER15_ADC_INPUTRANGE      (0x85)
#define POWER15_ADC_IRQ_RETFSET     (0x86)
#define POWER15_ADC_IRQ_FETFSET     (0x87)
#define POWER15_TIMER_CTL           (0x8A)
#define POWER15_VBUS_DET_SRP        (0x8B)
#define POWER15_HOTOVER_CTL         (0x8F)
#define POWER15_GPIO012_SIGNAL      (0x94)

#define POWER15_INTEN1              (0x40)
#define POWER15_INTEN2              (0x41)
#define POWER15_INTEN3              (0x42)
#define POWER15_INTSTS1             (0x48)
#define POWER15_INTSTS2             (0x49)
#define POWER15_INTSTS3             (0x4A)

/*For ajust axp15-reg only*/
#define POWER15_STATUS            	(0x00)
#define POWER15_LDO0OUT_VOL         (0x15)
#define POWER15_LDO34OUT_VOL        (0x28)
#define POWER15_LDO5OUT_VOL         (0x29)
#define POWER15_LDO6OUT_VOL         (0x2A)
#define POWER15_GPIO0_VOL           (0x96)
#define POWER15_DC1OUT_VOL          (0x26)
#define POWER15_DC2OUT_VOL          (0x23)
#define POWER15_DC3OUT_VOL          (0x27)
#define POWER15_DC4OUT_VOL          (0x2B)
#define POWER15_LDO0_CTL            (0x15)
#define POWER15_LDO3456_DC1234_CTL  (0x12)
#define POWER15_GPIO0_CTL           (0x90)
#define POWER15_GPIO1_CTL           (0x91)
#define POWER15_GPIO2_CTL           (0x92)
#define POWER15_GPIO3_CTL           (0x93)
#define POWER15_GPIO0123_SIGNAL       (0x97)
#define POWER15_DCDC_MODESET        (0x80)
#define POWER15_DCDC_FREQSET        (0x37)

#define	AXP152_IRQ_PEKLO		( 1 << 8)
#define	AXP152_IRQ_PEKSH	    ( 1 << 9)

#endif /* AXP_MFD152_HH */
