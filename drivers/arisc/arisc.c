/*
 *  drivers/arisc/arisc.c
 *
 * Copyright (c) 2012 Allwinner.
 * 2012-05-01 Written by sunny (sunny@allwinnertech.com).
 * 2012-10-01 Written by superm (superm@allwinnertech.com).
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "arisc_i.h"
#include <mach/sunxi-chip.h>
#include <asm/firmware.h>
#include <linux/dma-mapping.h>

/* local functions */
static int     arisc_wait_ready(unsigned int timeout);

/* external vars */
extern char *arisc_binary_start;
extern char *arisc_binary_end;

unsigned long arisc_sram_a2_vbase = (unsigned long)IO_ADDRESS(SUNXI_SRAM_A2_PBASE);
#if defined CONFIG_ARCH_SUN8IW1P1
static unsigned int arisc_debug_baudrate = 57600;
#elif (defined CONFIG_ARCH_SUN8IW3P1) || (defined CONFIG_ARCH_SUN8IW5P1) || (defined CONFIG_ARCH_SUN8IW6P1) ||\
      (defined CONFIG_ARCH_SUN8IW7P1) || (defined CONFIG_ARCH_SUN8IW9P1) || (defined CONFIG_ARCH_SUN9IW1P1)
static unsigned int arisc_debug_baudrate = 115200;
#endif
unsigned int arisc_debug_dram_crc_en = 0;
unsigned int arisc_debug_dram_crc_srcaddr = 0x40000000;
unsigned int arisc_debug_dram_crc_len = (1024 * 1024);
unsigned int arisc_debug_dram_crc_error = 0;
unsigned int arisc_debug_dram_crc_total_count = 0;
unsigned int arisc_debug_dram_crc_error_count = 0;
unsigned int arisc_debug_level = 2;
static unsigned char arisc_version[40] = "arisc defualt version";
static unsigned int arisc_pll = 0;
#if defined CONFIG_ARCH_SUN8IW1P1
static struct arisc_p2wi_block_cfg block_cfg;
static u8 regaddr = 0;
static u8 data = 0;
#elif (defined CONFIG_ARCH_SUN8IW3P1) || (defined CONFIG_ARCH_SUN8IW5P1) || (defined CONFIG_ARCH_SUN8IW6P1) ||\
      (defined CONFIG_ARCH_SUN8IW7P1) || (defined CONFIG_ARCH_SUN8IW9P1) || (defined CONFIG_ARCH_SUN9IW1P1)
static struct arisc_rsb_block_cfg block_cfg;
static u32 devaddr = 0;
static u8 regaddr = 0;
static u32 data = 0;
static u32 datatype = 0;
#endif
static atomic_t arisc_suspend_flag;
/* for save power check configuration */
struct standby_info_para arisc_powchk_back;

static ssize_t arisc_version_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	ssize_t size = 0;

	size = sprintf(buf, "%s\n", arisc_version);

	return size;
}

static ssize_t arisc_debug_mask_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	ssize_t size = 0;

	size = sprintf(buf, "%u\n", arisc_debug_level);

	return size;
}

static ssize_t arisc_debug_mask_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	u32 value = 0;

	sscanf(buf, "%u", &value);
	if ((value < 0) || (value > 3)) {
		ARISC_WRN("invalid arisc debug mask [%d] to set\n", value);
		return size;
	}

	arisc_debug_level = value;
	arisc_set_debug_level(arisc_debug_level);
	ARISC_LOG("debug_mask change to %d\n", arisc_debug_level);

	return size;
}

static ssize_t arisc_debug_baudrate_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	ssize_t size = 0;

	size = sprintf(buf, "%u\n", arisc_debug_baudrate);

	return size;
}

static ssize_t arisc_debug_baudrate_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	u32 value = 0;

	sscanf(buf, "%u", &value);
	if ((value != 115200) && (value != 57600) && (value != 9600)) {
		ARISC_WRN("invalid arisc uart baudrate [%d] to set\n", value);
		return size;
	}

	arisc_debug_baudrate = value;
	arisc_set_uart_baudrate(arisc_debug_baudrate);
	ARISC_LOG("debug_baudrate change to %d\n", arisc_debug_baudrate);

	return size;
}

static ssize_t arisc_dram_crc_paras_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	ssize_t size = 0;

	size = sprintf(buf, "enable:0x%x srcaddr:0x%x lenght:0x%x\n", arisc_debug_dram_crc_en,
							 arisc_debug_dram_crc_srcaddr, arisc_debug_dram_crc_len);

	return size;
}

static ssize_t arisc_dram_crc_paras_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	u32 dram_crc_en      = 0;
	u32 dram_crc_srcaddr = 0;
	u32 dram_crc_len     = 0;

	sscanf(buf, "%x %x %x\n", &dram_crc_en, &dram_crc_srcaddr, &dram_crc_len);

	if ((dram_crc_en != 0) && (dram_crc_en != 1)) {
		ARISC_WRN("invalid arisc debug dram crc paras [%x] [%x] [%x] to set\n",
								  dram_crc_en, dram_crc_srcaddr, dram_crc_len);

		return size;
	}

	arisc_debug_dram_crc_en = dram_crc_en;
	arisc_debug_dram_crc_srcaddr = dram_crc_srcaddr;
	arisc_debug_dram_crc_len = dram_crc_len;
	arisc_set_dram_crc_paras(arisc_debug_dram_crc_en,
							 arisc_debug_dram_crc_srcaddr,
							 arisc_debug_dram_crc_len);
	ARISC_LOG("dram_crc_en=0x%x, dram_crc_srcaddr=0x%x, dram_crc_len=0x%x\n",
			  arisc_debug_dram_crc_en, arisc_debug_dram_crc_srcaddr, arisc_debug_dram_crc_len);

	return size;
}

static ssize_t arisc_dram_crc_result_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	arisc_query_dram_crc_result((unsigned long *)&arisc_debug_dram_crc_error,
								(unsigned long *)&arisc_debug_dram_crc_total_count,
								(unsigned long *)&arisc_debug_dram_crc_error_count);
	return sprintf(buf, "dram info:\n" \
						"  enable %u\n" \
						"  error %u\n" \
						"  total count %u\n" \
						"  error count %u\n" \
						"  src:%p\n" \
						"  len:0x%x\n", \
				arisc_debug_dram_crc_en,
				arisc_debug_dram_crc_error,
				arisc_debug_dram_crc_total_count,
				arisc_debug_dram_crc_error_count,
				(void *)arisc_debug_dram_crc_srcaddr,
				arisc_debug_dram_crc_len);
}

static ssize_t arisc_dram_crc_result_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	u32 error = 0;
	u32 total_count = 0;
	u32 error_count = 0;

	sscanf(buf, "%u %u %u", &error, &total_count, &error_count);
	if ((error != 0) && (error != 1)) {
		ARISC_WRN("invalid arisc dram crc result [%d] [%d] [%d] to set\n", error, total_count, error_count);
		return size;
	}

	arisc_debug_dram_crc_error = error;
	arisc_debug_dram_crc_total_count = total_count;
	arisc_debug_dram_crc_error_count = error_count;
	arisc_set_dram_crc_result((unsigned long)arisc_debug_dram_crc_error,
							  (unsigned long)arisc_debug_dram_crc_total_count,
							  (unsigned long)arisc_debug_dram_crc_error_count);
	ARISC_LOG("debug_dram_crc_result change to error:%u total count:%u error count:%u\n",
			arisc_debug_dram_crc_error, arisc_debug_dram_crc_total_count, arisc_debug_dram_crc_error_count);

	return size;
}

static ssize_t arisc_power_enable_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "power enable 0x%x\n", \
				arisc_powchk_back.power_state.enable);
}

static ssize_t arisc_power_enable_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	unsigned long value;

	if (kstrtoul(buf, 0, &value) < 0) {
		ARISC_ERR("illegal value, only one para support\n");
		return -EINVAL;
	}

	if (value & ~(CPUS_ENABLE_POWER_EXP | CPUS_WAKEUP_POWER_STA | CPUS_WAKEUP_POWER_CSM)) {
		ARISC_ERR("invalid format, 'enable' should:\n"\
				  "  bit31:enable power check during standby\n"\
				  "  bit1: enable wakeup when power state exception\n"\
				  "  bit0: enable wakeup when power consume exception\n");
		return size;
	}

	arisc_powchk_back.power_state.enable = value;
	arisc_set_standby_power_cfg(&arisc_powchk_back);
	ARISC_LOG("standby_power_set enable:0x%x\n", arisc_powchk_back.power_state.enable);

	return size;
}

static ssize_t arisc_power_state_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "power regs 0x%x\n", \
				arisc_powchk_back.power_state.power_reg);
}

static ssize_t arisc_power_state_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	unsigned long value;

	if (kstrtoul(buf, 0, &value) < 0) {
		ARISC_ERR("illegal value, only one para support");
		return -EINVAL;
	}
	arisc_powchk_back.power_state.power_reg = value;

	arisc_set_standby_power_cfg(&arisc_powchk_back);
	ARISC_LOG("standby_power_set power_state 0x%x\n",
			arisc_powchk_back.power_state.power_reg);

	return size;
}

static ssize_t arisc_power_consum_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "power consume %dmw\n", \
				arisc_powchk_back.power_state.system_power);
}

static ssize_t arisc_power_consum_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	unsigned long value;

	if (kstrtoul(buf, 0, &value) < 0) {
		if (1 != sscanf(buf, "%lu", &value)) {
			ARISC_ERR("illegal value, only one para support");
			return -EINVAL;
		}
	}
	arisc_powchk_back.power_state.system_power = value;

	arisc_set_standby_power_cfg(&arisc_powchk_back);
	ARISC_LOG("standby_power_set power_consum %dmw\n",
			arisc_powchk_back.power_state.system_power);

	return size;
}

#if (defined CONFIG_ARCH_SUN9IW1P1)
#define SST_POWER_MASK 0xffffffff
static const unsigned char pmu_powername[32][8] = {
	"dc5ldo", "dcdc1", "dcdc2", "dcdc3", "dcdc4", "dcdc5", "aldo1", "aldo2",
	"eldo1",  "eldo2", "eldo3", "dldo1", "dldo2", "aldo3", "swout", "dc1sw",
	"dcdca",  "dcdcb", "dcdcc", "dcdcd", "dcdce", "aldo1", "aldo2", "aldo3",
	"bldo1",  "bldo2", "bldo3", "bldo4", "cldo1", "cldo2", "cldo3", "swout",
};
#elif (defined CONFIG_ARCH_SUN8IW1P1) || (defined CONFIG_ARCH_SUN8IW3P1) || (defined CONFIG_ARCH_SUN8IW5P1) ||\
      (defined CONFIG_ARCH_SUN8IW7P1) || (defined CONFIG_ARCH_SUN8IW9P1)
#define SST_POWER_MASK 0x07ffff
static const unsigned char pmu_powername[20][8] = {
	"dc5ldo", "dcdc1",  "dcdc2",  "dcdc3", "dcdc4", "dcdc5", "aldo1", "aldo2",
	"eldo1",  "eldo2",  "eldo3",  "dldo1", "dldo2", "dldo3", "dldo4", "dc1sw",
	"aldo3",  "io0ldo", "io1ldo",
};
#elif (defined CONFIG_ARCH_SUN8IW6P1)
#define SST_POWER_MASK 0xffffff
static const unsigned char pmu_powername[26][10] = {
	"dcdc1", "dcdc2",  "dcdc3",  "dcdc4", "dcdc5", "dcdc6", "dcdc7", "reserved0",
	"eldo1",  "eldo2",  "eldo3",  "dldo1", "dldo2", "dldo3", "dldo4", "dc1sw",
	"reserved1",  "reserved2",  "fldo1",  "fldo2", "fldo3", "aldo1", "aldo2", "aldo3",
	"io0ldo",  "io1ldo",
};
#endif

static ssize_t arisc_power_trueinfo_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned int i, count, power_reg;
	standby_info_para_t sst_info;
	unsigned char pmu_name[320];
	unsigned char *pbuf;

	arisc_query_standby_power(&sst_info);
	power_reg = sst_info.power_state.power_reg;
	power_reg &= SST_POWER_MASK;

#if (defined CONFIG_ARCH_SUN9IW1P1)
	/* print the pmu power on state */
	strcpy(pmu_name, "these power on during standby:\n  axp809:");
	pbuf = pmu_name + strlen("these power on during standby:\n  axp809:");
	count = 0;
	for (i = 0; i < 16; i++) {
		if (power_reg & (1 << i)) {
			strncpy(pbuf, pmu_powername[i], 8);
			pbuf += strlen(pmu_powername[i]);
			*pbuf++ = ',';
			count++;
		}
	}
	if (count)
		strcpy(--pbuf, ""); /* rollback the last ',' */
	else {
		strcpy(pbuf, "(null)");
		pbuf += strlen("(null)");
	}

	strcpy(pbuf, "\n  axp806:");
	pbuf += strlen("\n  axp806:");
	count = 0;
	for (i = 16; i < 32; i++) {
		if (power_reg & (1 << i)) {
			strncpy(pbuf, pmu_powername[i], 8);
			pbuf += strlen(pmu_powername[i]);
			*pbuf++ = ',';
			count++;
		}
	}
	if (count)
		strcpy(--pbuf, "\n"); /* rollback the last ',' */
	else {
		strcpy(pbuf, "(null)");
		pbuf += strlen("(null)");
	}

	/* print the pmu power off state */
	power_reg ^= SST_POWER_MASK;

	strcpy(pbuf, "\nthese power off during standby:\n  axp809:");
	pbuf += strlen("\nthese power off during standby:\n  axp809:");
	count = 0;
	for (i = 0; i < 16; i++) {
		if (power_reg & (1 << i)) {
			strncpy(pbuf, pmu_powername[i], 8);
			pbuf += strlen(pmu_powername[i]);
			*pbuf++ = ',';
			count++;
		}
	}
	if (count)
		strcpy(--pbuf, ""); /* rollback the last ',' */
	else {
		strcpy(pbuf, "(null)");
		pbuf += strlen("(null)");
	}
	strcpy(pbuf, "\n  axp806:");
	pbuf += strlen("\n  axp806:");
	count = 0;
	for (i = 16; i < 32; i++) {
		if (power_reg & (1 << i)) {
			strncpy(pbuf, pmu_powername[i], 8);
			pbuf += strlen(pmu_powername[i]);
			*pbuf++ = ',';
			count++;
		}
	}
	if (count)
		strcpy(--pbuf, ""); /* rollback the last ',' */
	else {
		strcpy(pbuf, "(null)");
		pbuf += strlen("(null)");
	}
#else
	/* print the pmu power on state */
	strcpy(pmu_name, "these power on during standby:\n  axp:");
	pbuf = pmu_name + strlen("these power on during standby:\n  axp:");
	count = 0;
	for (i = 0; i < 32; i++) {
		if (power_reg & (1 << i)) {
			strncpy(pbuf, pmu_powername[i], 8);
			pbuf += strlen(pmu_powername[i]);
			*pbuf++ = ',';
			count++;
		}
	}
	if (count)
		strcpy(--pbuf, ""); /* rollback the last ',' */
	else {
		strcpy(pbuf, "(null)");
		pbuf += strlen("(null)");
	}

	/* print the pmu power off state */
	power_reg ^= SST_POWER_MASK;

	strcpy(pbuf, "\nthese power off during standby:\n  axp:");
	pbuf += strlen("\nthese power off during standby:\n  axp:");
	count = 0;
	for (i = 0; i < 32; i++) {
		if (power_reg & (1 << i)) {
			strncpy(pbuf, pmu_powername[i], 8);
			pbuf += strlen(pmu_powername[i]);
			*pbuf++ = ',';
			count++;
		}
	}
	if (count)
		strcpy(--pbuf, ""); /* rollback the last ',' */
	else {
		strcpy(pbuf, "(null)");
		pbuf += strlen("(null)");
	}
#endif

	return sprintf(buf, "power info:\n" \
				"  enable 0x%x\n" \
				"  regs 0x%x\n" \
				"  power consume %dmw\n" \
				"%s\n", \
			sst_info.power_state.enable, \
			sst_info.power_state.power_reg, \
			sst_info.power_state.system_power, \
			pmu_name);
}

static ssize_t arisc_freq_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	ssize_t size = 0;

	struct clk *pll = NULL;

#if (defined CONFIG_ARCH_SUN8IW1P1) || (defined CONFIG_ARCH_SUN8IW3P1)
	if (arisc_pll == 1)
		pll = clk_get(NULL, "pll1");
#elif defined CONFIG_ARCH_SUN9IW1P1
	if (arisc_pll == 1) {
		pll = clk_get(NULL, "pll1");
	} else if (arisc_pll == 2) {
		pll = clk_get(NULL, "pll2");
	}
#elif (defined CONFIG_ARCH_SUN8IW5P1) || (defined CONFIG_ARCH_SUN8IW7P1)
	if (arisc_pll == 1)
		pll = clk_get(NULL, "pll_cpu");
#elif (defined CONFIG_ARCH_SUN8IW6P1) || (defined CONFIG_ARCH_SUN8IW9P1)
	if (arisc_pll == 1) {
		pll = clk_get(NULL, "pll_cpu0");
	} else if (arisc_pll == 2) {
		pll = clk_get(NULL, "pll_cpu1");
	}
#endif

	if(!pll || IS_ERR(pll)){
		ARISC_ERR("try to get pll%u failed!\n", arisc_pll);
		return size;
	}

	size = sprintf(buf, "%u\n", (unsigned int)clk_get_rate(pll));

	return size;
}

static ssize_t arisc_freq_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
#ifndef CONFIG_ARCH_SUN8IW7P1
	u32 freq = 0;
	u32 pll  = 0;
	u32 mode = 0;
	u32 ret = 0;

	sscanf(buf, "%u %u", &pll, &freq);

#if (defined CONFIG_ARCH_SUN8IW1P1) || (defined CONFIG_ARCH_SUN8IW3P1) || (defined CONFIG_ARCH_SUN8IW5P1) || \
    (defined CONFIG_ARCH_SUN8IW7P1)
	if ((pll != 1) || (freq < 0) || (freq > 3000000)) {
		ARISC_WRN("invalid pll [%u] or freq [%u] to set, this platform only support pll1, freq [0, 3000000]KHz\n", pll, freq);
		ARISC_WRN("pls echo like that: echo pll freq > freq\n");
		return size;
	}
#elif (defined CONFIG_ARCH_SUN9IW1P1) || (defined CONFIG_ARCH_SUN8IW6P1) || (defined CONFIG_ARCH_SUN8IW9P1)
	if (((pll != 1) && (pll != 2)) || (freq < 0) || (freq > 3000000)) {
		ARISC_WRN("invalid pll [%u] or freq [%u] to set, this platform only support pll1 and pll2, freq [0, 3000000]KHz\n", pll, freq);
		ARISC_WRN("pls echo like that: echo pll freq > freq\n");
		return size;
	}
#endif

	arisc_pll = pll;
	ret = arisc_dvfs_set_cpufreq(freq, pll, mode, NULL, NULL);
	if (ret) {
		ARISC_ERR("pll%u freq set to %u fail\n", pll, freq);
	} else {
		ARISC_LOG("pll%u freq set to %u success\n", pll, freq);
	}
#endif
	return size;
}

#if defined CONFIG_ARCH_SUN8IW1P1
static ssize_t arisc_p2wi_read_block_data_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	ssize_t size = 0;
	u32 ret = 0;

	if ((block_cfg.addr == NULL) || (block_cfg.data == NULL) ||
		(*block_cfg.addr < 0) || (*block_cfg.addr > 0xff)) {
		ARISC_LOG("invalid p2wi paras, regaddr:0x%x\n", block_cfg.addr ? *block_cfg.addr : 0);
		ARISC_LOG("pls echo like that: echo regaddr > p2wi_read_block_data\n");
		return size;
	}

	ret = arisc_p2wi_read_block_data(&block_cfg);
	if (ret) {
		ARISC_LOG("p2wi read data:0x%x from regaddr:0x%x fail\n", *block_cfg.data, *block_cfg.addr);
	} else {
		ARISC_LOG("p2wi read data:0x%x from regaddr:0x%x success\n", *block_cfg.data, *block_cfg.addr);
	}
	size = sprintf(buf, "%x\n", data);

	return size;
}

static ssize_t arisc_p2wi_read_block_data_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	sscanf(buf, "%x", (u32 *)&regaddr);
	if ((regaddr < 0) || (regaddr > 0xff)) {
		ARISC_WRN("invalid p2wi paras, regaddr:0x%x\n", regaddr);
		ARISC_LOG("pls echo like that: echo regaddr > p2wi_read_block_data\n");
		return size;
	}

	block_cfg.msgattr = ARISC_MESSAGE_ATTR_SOFTSYN;
	block_cfg.len = 1;
	block_cfg.addr = &regaddr;
	block_cfg.data = &data;

	ARISC_LOG("p2wi read regaddr:0x%x\n", *block_cfg.addr);

	return size;
}

static ssize_t arisc_p2wi_write_block_data_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	ssize_t size = 0;
	u32 ret = 0;

	if ((block_cfg.addr == NULL) || (block_cfg.data == NULL) ||
		(*block_cfg.addr < 0) || (*block_cfg.addr > 0xff)) {
		ARISC_WRN("invalid p2wi paras, regaddr:0x%x, data:0x%x\n", block_cfg.addr ? *block_cfg.addr : 0, block_cfg.data ? *block_cfg.data : 0);
		ARISC_LOG("pls echo like that: echo regaddr data > p2wi_write_block_data\n");
		return size;
	}

	block_cfg.msgattr = ARISC_MESSAGE_ATTR_SOFTSYN;
	block_cfg.len = 1;
	block_cfg.addr = &regaddr;
	block_cfg.data = &data;
	ret = arisc_p2wi_read_block_data(&block_cfg);
	if (ret) {
		ARISC_WRN("p2wi read data:0x%x from regaddr:0x%x fail\n", *block_cfg.data, *block_cfg.addr);
	} else {
		ARISC_LOG("p2wi read data:0x%x from regaddr:0x%x success\n", *block_cfg.data, *block_cfg.addr);
	}
	size = sprintf(buf, "%x\n", data);

	return size;
}

static ssize_t arisc_p2wi_write_block_data_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	u32 ret;

	sscanf(buf, "%x %x", (u32 *)&regaddr, (u32 *)&data);
	if ((regaddr < 0) || (regaddr > 0xff)) {
		ARISC_WRN("invalid p2wi paras, regaddr:0x%x, data:0x%x\n", regaddr, data);
		ARISC_WRN("pls echo like that: echo regaddr data > p2wi_write_block_data\n");
		return size;
	}

	block_cfg.msgattr = ARISC_MESSAGE_ATTR_SOFTSYN;
	block_cfg.len = 1;
	block_cfg.addr = &regaddr;
	block_cfg.data = &data;
	ret = arisc_p2wi_write_block_data(&block_cfg);
	if (ret) {
		ARISC_WRN("p2wi write data:0x%x to regaddr:0x%x fail\n", *block_cfg.data, *block_cfg.addr);
	} else {
		ARISC_LOG("p2wi write data:0x%x to regaddr:0x%x success\n", *block_cfg.data, *block_cfg.addr);
	}

	return size;
}

#elif (defined CONFIG_ARCH_SUN8IW3P1) || (defined CONFIG_ARCH_SUN8IW5P1) || (defined CONFIG_ARCH_SUN8IW6P1) ||\
      (defined CONFIG_ARCH_SUN8IW7P1) || (defined CONFIG_ARCH_SUN8IW9P1) || (defined CONFIG_ARCH_SUN9IW1P1)
static ssize_t arisc_rsb_read_block_data_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	ssize_t size = 0;
	u32 ret = 0;

	if ((block_cfg.regaddr == NULL) || (block_cfg.data == NULL) ||
		(block_cfg.devaddr > 0xff) ||
		((block_cfg.datatype !=  RSB_DATA_TYPE_BYTE) && (block_cfg.datatype !=  RSB_DATA_TYPE_HWORD) && (block_cfg.datatype !=  RSB_DATA_TYPE_WORD))) {
		ARISC_WRN("invalid rsb paras, devaddr:0x%x, regaddr:0x%x, datatype:0x%x\n", block_cfg.devaddr, block_cfg.regaddr ? *block_cfg.regaddr : 0, block_cfg.datatype);
		ARISC_WRN("pls echo like that: echo devaddr regaddr datatype > rsb_read_block_data\n");
		return size;
	}

	ret = arisc_rsb_read_block_data(&block_cfg);
	if (ret) {
		ARISC_LOG("rsb read data:0x%x from devaddr:0x%x regaddr:0x%x fail\n", *block_cfg.data, block_cfg.devaddr, *block_cfg.regaddr);
	} else {
		ARISC_LOG("rsb read data:0x%x from devaddr:0x%x regaddr:0x%x success\n", *block_cfg.data, block_cfg.devaddr, *block_cfg.regaddr);
	}
	size = sprintf(buf, "%x\n", data);

	return size;
}

static ssize_t arisc_rsb_read_block_data_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	sscanf(buf, "%x %x %x", &devaddr, (u32 *)&regaddr, &datatype);
	if ((devaddr > 0xff) ||
	((datatype !=  RSB_DATA_TYPE_BYTE) && (datatype !=  RSB_DATA_TYPE_HWORD) && (datatype !=  RSB_DATA_TYPE_WORD))) {
		ARISC_WRN("invalid rsb paras to set, devaddr:0x%x, regaddr:0x%x, datatype:0x%x\n", devaddr, regaddr, datatype);
		ARISC_WRN("pls echo like that: echo devaddr regaddr datatype > rsb_read_block_data\n");
		return size;
	}

	block_cfg.msgattr = ARISC_MESSAGE_ATTR_SOFTSYN;
	block_cfg.datatype = datatype;
	block_cfg.len = 1;
	block_cfg.devaddr = devaddr;
	block_cfg.regaddr = &regaddr;
	block_cfg.data = &data;

	ARISC_LOG("rsb read data from devaddr:0x%x regaddr:0x%x\n", devaddr, regaddr);

	return size;
}

static ssize_t arisc_rsb_write_block_data_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	ssize_t size = 0;
	u32 ret = 0;

	if ((block_cfg.regaddr == NULL) || (block_cfg.data == NULL) ||
		(block_cfg.devaddr > 0xff) ||
		((block_cfg.datatype !=  RSB_DATA_TYPE_BYTE) && (block_cfg.datatype !=  RSB_DATA_TYPE_HWORD) && (block_cfg.datatype !=  RSB_DATA_TYPE_WORD))) {
		ARISC_WRN("invalid rsb paras, devaddr:0x%x, regaddr:0x%x, datatype:0x%x\n", block_cfg.devaddr, block_cfg.regaddr ? *block_cfg.regaddr : 0, block_cfg.datatype);
		ARISC_WRN("pls echo like that: echo devaddr regaddr data datatype > rsb_write_block_data\n");
		return size;
	}

	ret = arisc_rsb_read_block_data(&block_cfg);
	if (ret) {
		ARISC_ERR("rsb read data:0x%x from devaddr:0x%x regaddr:0x%x fail\n", *block_cfg.data, block_cfg.devaddr, *block_cfg.regaddr);
	} else {
		ARISC_LOG("rsb read data:0x%x from devaddr:0x%x regaddr:0x%x success\n", *block_cfg.data, block_cfg.devaddr, *block_cfg.regaddr);
	}
	size = sprintf(buf, "%x\n", data);

	return size;
}

static ssize_t arisc_rsb_write_block_data_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	u32 ret = 0;

	sscanf(buf, "%x %x %x %x", &devaddr, (u32 *)&regaddr, (u32 *)&data, &datatype);
	if ((devaddr > 0xff) ||
	((datatype !=  RSB_DATA_TYPE_BYTE) && (datatype !=  RSB_DATA_TYPE_HWORD) && (datatype !=  RSB_DATA_TYPE_WORD))) {
		ARISC_WRN("invalid rsb paras, devaddr:0x%x, regaddr:0x%x, data:0x%x, datatype:0x%x\n", devaddr, regaddr, data, datatype);
		ARISC_WRN("pls echo like that: echo devaddr regaddr data datatype > rsb_write_block_data\n");
		return size;
	}

	block_cfg.msgattr = ARISC_MESSAGE_ATTR_SOFTSYN;
	block_cfg.datatype = datatype;
	block_cfg.len = 1;
	block_cfg.devaddr = devaddr;
	block_cfg.regaddr = &regaddr;
	block_cfg.data = &data;
	ret = arisc_rsb_write_block_data(&block_cfg);
	if (ret) {
		ARISC_ERR("rsb write data:0x%x to devaddr:0x%x regaddr:0x%x fail\n", *block_cfg.data, block_cfg.devaddr, *block_cfg.regaddr);
	} else {
		ARISC_LOG("rsb write data:0x%x to devaddr:0x%x regaddr:0x%x success\n", *block_cfg.data, block_cfg.devaddr, *block_cfg.regaddr);
	}

	return size;
}
#endif

int arisc_suspend_flag_query(void)
{
	return atomic_read(&arisc_suspend_flag);
}

static void sunxi_arisc_shutdown(struct platform_device *dev)
{
	atomic_set(&arisc_suspend_flag, 1);
	while (arisc_semaphore_used_num_query()) {
			msleep(1);
	}
}

#ifdef CONFIG_PM
static int sunxi_arisc_suspend(struct device *dev)
{
	atomic_set(&arisc_suspend_flag, 1);
	while (arisc_semaphore_used_num_query()) {
			msleep(1);
	}

	return 0;
}

static int sunxi_arisc_resume(struct device *dev)
{
	unsigned long wake_event;
	standby_info_para_t sst_info;

	atomic_set(&arisc_suspend_flag, 0);
#if defined CONFIG_ARCH_SUN8IW7P1
	arisc_cpux_ready_notify();
#endif
	arisc_query_wakeup_source(&wake_event);
	if (wake_event & CPUS_WAKEUP_POWER_EXP) {
		ARISC_LOG("power exception during standby, enable:0x%x" \
				  " expect state:0x%x, expect consumption:%dmw", \
				arisc_powchk_back.power_state.enable, \
				arisc_powchk_back.power_state.power_reg, \
				arisc_powchk_back.power_state.system_power);
		arisc_query_standby_power(&sst_info);
		ARISC_LOG(" real state:0x%x, real consumption:%dmw\n", \
				sst_info.power_state.power_reg, \
				sst_info.power_state.system_power);
	}

	return 0;
}

static const struct dev_pm_ops sunxi_arisc_dev_pm_ops = {
	.suspend = sunxi_arisc_suspend,
	.resume = sunxi_arisc_resume,
};

#define SUNXI_ARISC_DEV_PM_OPS (&sunxi_arisc_dev_pm_ops)
#else
#define SUNXI_ARISC_DEV_PM_OPS NULL
#endif // CONFIG_PM

static struct device_attribute sunxi_arisc_attrs[] = {
	__ATTR(version,                 S_IRUGO,            arisc_version_show,                 NULL),
	__ATTR(debug_mask,              S_IRUGO | S_IWUSR,  arisc_debug_mask_show,              arisc_debug_mask_store),
	__ATTR(debug_baudrate,          S_IRUGO | S_IWUSR,  arisc_debug_baudrate_show,          arisc_debug_baudrate_store),
	__ATTR(dram_crc_paras,          S_IRUGO | S_IWUSR,  arisc_dram_crc_paras_show,          arisc_dram_crc_paras_store),
	__ATTR(dram_crc_result,         S_IRUGO | S_IWUSR,  arisc_dram_crc_result_show,         arisc_dram_crc_result_store),
	__ATTR(sst_power_enable_mask,   S_IRUGO | S_IWUSR,  arisc_power_enable_show,            arisc_power_enable_store),
	__ATTR(sst_power_state_mask,    S_IRUGO | S_IWUSR,  arisc_power_state_show,             arisc_power_state_store),
	__ATTR(sst_power_consume_mask,  S_IRUGO | S_IWUSR,  arisc_power_consum_show,            arisc_power_consum_store),
	__ATTR(sst_power_real_info,     S_IRUGO,            arisc_power_trueinfo_show,          NULL),
	__ATTR(freq,                    S_IRUGO | S_IWUSR,  arisc_freq_show,                    arisc_freq_store),
#if defined CONFIG_ARCH_SUN8IW1P1
	__ATTR(p2wi_read_block_data,    S_IRUGO | S_IWUSR,  arisc_p2wi_read_block_data_show,    arisc_p2wi_read_block_data_store),
	__ATTR(p2wi_write_block_data,   S_IRUGO | S_IWUSR,  arisc_p2wi_write_block_data_show,   arisc_p2wi_write_block_data_store),
#elif (defined CONFIG_ARCH_SUN8IW3P1) || (defined CONFIG_ARCH_SUN8IW5P1) || (defined CONFIG_ARCH_SUN8IW6P1) ||\
      (defined CONFIG_ARCH_SUN8IW7P1) || (defined CONFIG_ARCH_SUN8IW9P1) || (defined CONFIG_ARCH_SUN9IW1P1)
	__ATTR(rsb_read_block_data,     S_IRUGO | S_IWUSR,  arisc_rsb_read_block_data_show,     arisc_rsb_read_block_data_store),
	__ATTR(rsb_write_block_data,    S_IRUGO | S_IWUSR,  arisc_rsb_write_block_data_show,    arisc_rsb_write_block_data_store),
#endif
};

static void sunxi_arisc_sysfs(struct platform_device *pdev)
{
	unsigned int i;
	memset((void*)&block_cfg, 0, sizeof(block_cfg));
	for (i = 0; i < ARRAY_SIZE(sunxi_arisc_attrs); i++) {
		device_create_file(&pdev->dev, &sunxi_arisc_attrs[i]);
	}
}

static int arisc_wait_ready(unsigned int timeout)
{
	unsigned long          expire;
#ifdef CONFIG_SUNXI_MODULE
	struct sunxi_module_info arisc_module_info;
#endif

	expire = msecs_to_jiffies(timeout) + jiffies;

	/* wait arisc startup ready */
	while (1) {
		/*
		 * linux cpu interrupt is disable now,
		 * we should query message by hand.
		 */
		struct arisc_message *pmessage = arisc_hwmsgbox_query_message();
		if (pmessage == NULL) {
			if (time_is_before_eq_jiffies(expire)) {
				return -ETIMEDOUT;
			}
			/* try to query again */
			continue;
		}
		/* query valid message */
		if (pmessage->type == ARISC_STARTUP_NOTIFY) {
			/* check arisc software and driver version match or not */
			if (pmessage->paras[0] != ARISC_VERSIONS) {
				ARISC_ERR("arisc firmware:%d and driver version:%d not matched\n", pmessage->paras[0], ARISC_VERSIONS);
				return -EINVAL;
			} else {
				/* printf the main and sub version string */
				memcpy((void *)arisc_version, (const void*)(&(pmessage->paras[1])), 40);
				ARISC_LOG("arisc version: [%s]\n", arisc_version);
#ifdef CONFIG_SUNXI_MODULE
				strncpy((char *)arisc_module_info.module, (const char *)"arisc", 16);
				strncpy((char *)arisc_module_info.version, (const char *)arisc_version, 16);
				sunxi_module_info_register(&arisc_module_info);
#endif
			}

			/* received arisc startup ready message */
			ARISC_INF("arisc startup ready\n");
			if ((pmessage->attr & ARISC_MESSAGE_ATTR_SOFTSYN) ||
				(pmessage->attr & ARISC_MESSAGE_ATTR_HARDSYN)) {
				/* synchronous message, just feedback it */
				ARISC_INF("arisc startup notify message feedback\n");
				pmessage->paras[0] = virt_to_phys((void *)&arisc_binary_start);
				arisc_hwmsgbox_feedback_message(pmessage, ARISC_SEND_MSG_TIMEOUT);
			} else {
				/* asyn message, free message directly */
				ARISC_INF("arisc startup notify message free directly\n");
				arisc_message_free(pmessage);
			}
			break;
		}
		/*
		 * invalid message detected, ignore it.
		 * by sunny at 2012-7-6 18:34:38.
		 */
		ARISC_WRN("arisc startup waiting ignore message\n");
		if ((pmessage->attr & ARISC_MESSAGE_ATTR_SOFTSYN) ||
			(pmessage->attr & ARISC_MESSAGE_ATTR_HARDSYN)) {
			/* synchronous message, just feedback it */
			arisc_hwmsgbox_send_message(pmessage, ARISC_SEND_MSG_TIMEOUT);
		} else {
			/* asyn message, free message directly */
			arisc_message_free(pmessage);
		}
		/* we need waiting continue */
	}

	return 0;
}

static int  sunxi_arisc_clk_cfg(struct platform_device *pdev)
{
#if (defined CONFIG_ARCH_SUN8IW1P1) || (defined CONFIG_ARCH_SUN8IW3P1)
	struct clk *pll5 = NULL;
	struct clk *pll6 = NULL;
	struct clk *hosc = NULL;
	struct clk *losc = NULL;

	ARISC_INF("device [%s] clk resource request enter\n", dev_name(&pdev->dev));

	/* config PLL5 for dram clk */
	pll5 = clk_get(NULL, PLL5_CLK);

	if(!pll5 || IS_ERR(pll5)){
		ARISC_ERR("try to get pll5 failed!\n");
		return -EINVAL;
	}

	if(clk_prepare_enable(pll5)) {
		ARISC_ERR("try to enable pll5 output failed!\n");
		return -EINVAL;
	}

	/* config PLL6 for cpus clk */
	pll6 = clk_get(NULL, PLL6_CLK);
	if(!pll6 || IS_ERR(pll6)){
		ARISC_ERR("try to get pll6 failed!\n");
		return -EINVAL;
	}

	if(clk_prepare_enable(pll6)) {
		ARISC_ERR("try to enable pll6 output failed!\n");
		return -EINVAL;
	}

#elif (defined CONFIG_ARCH_SUN8IW5P1)
	struct clk *pllddr0 = NULL;
	struct clk *pllddr1 = NULL;
	struct clk *pllperiph = NULL;
	struct clk *hosc = NULL;
	struct clk *losc = NULL;

	ARISC_INF("device [%s] clk resource request enter\n", dev_name(&pdev->dev));
	/* config PLL5 for dram clk */
	pllddr0 = clk_get(NULL, PLL_DDR0_CLK);
	if(!pllddr0 || IS_ERR(pllddr0)){
		ARISC_ERR("try to get pll_ddr0 failed!\n");
		return -EINVAL;
	}

	if(clk_prepare_enable(pllddr0)) {
		ARISC_ERR("try to enable pll_ddr0 output failed!\n");
		return -EINVAL;
	}

	pllddr1 = clk_get(NULL, PLL_DDR1_CLK);
	if(!pllddr1 || IS_ERR(pllddr1)){
		ARISC_ERR("try to get pll_ddr1 failed!\n");
		return -EINVAL;
	}

	if(clk_prepare_enable(pllddr1)) {
		ARISC_ERR("try to enable pll_ddr1 output failed!\n");
		return -EINVAL;
	}

	/* config PLL6 for cpus clk */
	pllperiph = clk_get(NULL, PLL_PERIPH_CLK);
	if(!pllperiph || IS_ERR(pllperiph)){
		ARISC_ERR("try to get pll_periph failed!\n");
		return -EINVAL;
	}

	if(clk_prepare_enable(pllperiph)) {
		ARISC_ERR("try to enable pll_periph output failed!\n");
		return -EINVAL;
	}

#elif (defined CONFIG_ARCH_SUN8IW7P1)
	struct clk *pllddr = NULL;
	struct clk *pllperiph = NULL;
	struct clk *hosc = NULL;
	struct clk *losc = NULL;

	ARISC_INF("device [%s] clk resource request enter\n", dev_name(&pdev->dev));

	/* config PLL5 for dram clk */
	pllddr = clk_get(NULL, PLL_DDR_CLK);
	if(!pllddr || IS_ERR(pllddr)){
		ARISC_ERR("try to get pll_ddr0 failed!\n");
		return -EINVAL;
	}

	if(clk_prepare_enable(pllddr)) {
		ARISC_ERR("try to enable pll_ddr0 output failed!\n");
		return -EINVAL;
	}

	/* config PLL6 for cpus clk */
	pllperiph = clk_get(NULL, PLL_PERIPH0_CLK);
	if(!pllperiph || IS_ERR(pllperiph)){
		ARISC_ERR("try to get pll_periph failed!\n");
		return -EINVAL;
	}

	if(clk_prepare_enable(pllperiph)) {
		ARISC_ERR("try to enable pll_periph output failed!\n");
		return -EINVAL;
	}

#elif (defined CONFIG_ARCH_SUN8IW6P1)
	struct clk *pllddr = NULL;
	struct clk *pllperiph = NULL;
	struct clk *hosc = NULL;
	struct clk *losc = NULL;

	ARISC_INF("device [%s] clk resource request enter\n", dev_name(&pdev->dev));

	/* config PLL_DDR for dram clk */
	pllddr = clk_get(NULL, PLL_DDR_CLK);
	if(!pllddr || IS_ERR(pllddr)){
		ARISC_ERR("try to get pll_ddr failed!\n");
		return -EINVAL;
	}

	if(clk_prepare_enable(pllddr)) {
		ARISC_ERR("try to enable pll_ddr output failed!\n");
		return -EINVAL;
	}

	/* config PLL_PERIPH for cpus clk */
	pllperiph = clk_get(NULL, PLL_PERIPH_CLK);
	if(!pllperiph || IS_ERR(pllperiph)){
		ARISC_ERR("try to get pll_periph failed!\n");
		return -EINVAL;
	}

	if(clk_prepare_enable(pllperiph)) {
		ARISC_ERR("try to enable pll_periph output failed!\n");
		return -EINVAL;
	}

#elif (defined CONFIG_ARCH_SUN8IW9P1)
	struct clk *pllddr0 = NULL;
	struct clk *pllddr1 = NULL;
	struct clk *pllperiph = NULL;
	struct clk *hosc = NULL;
	struct clk *losc = NULL;

	ARISC_INF("device [%s] clk resource request enter\n", dev_name(&pdev->dev));
	/* config PLL5 for dram clk */
	pllddr0 = clk_get(NULL, PLL_DDR0_CLK);
	if(!pllddr0 || IS_ERR(pllddr0)){
		ARISC_ERR("try to get pll_ddr0 failed!\n");
		return -EINVAL;
	}

	if(clk_prepare_enable(pllddr0)) {
		ARISC_ERR("try to enable pll_ddr0 output failed!\n");
		return -EINVAL;
	}

	pllddr1 = clk_get(NULL, PLL_DDR1_CLK);
	if(!pllddr1 || IS_ERR(pllddr1)){
		ARISC_ERR("try to get pll_ddr1 failed!\n");
		return -EINVAL;
	}

	if(clk_prepare_enable(pllddr1)) {
		ARISC_ERR("try to enable pll_ddr1 output failed!\n");
		return -EINVAL;
	}

	/* config PLL6 for cpus clk */
	pllperiph = clk_get(NULL, PLL_PERIPH0_CLK);
	if(!pllperiph || IS_ERR(pllperiph)){
		ARISC_ERR("try to get pll_periph failed!\n");
		return -EINVAL;
	}

	if(clk_prepare_enable(pllperiph)) {
		ARISC_ERR("try to enable pll_periph output failed!\n");
		return -EINVAL;
	}

#elif defined CONFIG_ARCH_SUN9IW1P1
	struct clk *pll3 = NULL;
	struct clk *pll4 = NULL;
	struct clk *pll6 = NULL;
	struct clk *hosc = NULL;
	struct clk *losc = NULL;

	ARISC_INF("device [%s] clk resource request enter\n", dev_name(&pdev->dev));

	/* config PLL6 for dram clk */
	pll6 = clk_get(NULL, PLL6_CLK);
	if(!pll6 || IS_ERR(pll6)){
		ARISC_ERR("try to get pll6 failed!\n");
		return -EINVAL;
	}

	if(clk_prepare_enable(pll6)) {
		ARISC_ERR("try to enable pll6 output failed!\n");
		return -EINVAL;
	}

	/* config PLL3 for cpus clk */
	pll3 = clk_get(NULL, PLL3_CLK);
	if(!pll3 || IS_ERR(pll3)){
		ARISC_ERR("try to get pll3 failed!\n");
		return -EINVAL;
	}

	if(clk_prepare_enable(pll3)) {
		ARISC_ERR("try to enable pll3 output failed!\n");
		return -EINVAL;
	}

	/* config PLL4 for cpus clk */
	pll4 = clk_get(NULL, PLL4_CLK);
	if(!pll4 || IS_ERR(pll4)){
		ARISC_ERR("try to get pll4 failed!\n");
		return -EINVAL;
	}

	if(clk_prepare_enable(pll4)) {
		ARISC_ERR("try to enable pll4 output failed!\n");
		return -EINVAL;
	}
#endif

	/* config HOSC for cpus clk */
	hosc = clk_get(NULL, HOSC_CLK);
	if(!hosc || IS_ERR(hosc)){
		ARISC_ERR("try to get hosc failed!\n");
		return -EINVAL;
	}

	if(clk_prepare_enable(hosc)) {
		ARISC_ERR("try to enable hosc output failed!\n");
		return -EINVAL;
	}

	/* config LOSC for cpus clk */
	losc = clk_get(NULL, LOSC_CLK);
	if(!losc || IS_ERR(losc)){
		ARISC_ERR("try to get losc failed!\n");
		return -EINVAL;
	}

	if(clk_prepare_enable(losc)) {
		ARISC_ERR("try to enable losc output failed!\n");
		return -EINVAL;
	}

	ARISC_INF("device [%s] clk resource request ok\n", dev_name(&pdev->dev));
	return 0;
}

static int  sunxi_arisc_pin_cfg(struct platform_device *pdev)
{
	script_item_u script_val;
	script_item_value_type_e type;
	script_item_u  *pin_list;
	int            pin_count = 0;
	int            pin_index = 0;
	struct gpio_config    *pin_cfg;
	char          pin_name[SUNXI_PIN_NAME_MAX_LEN];
	unsigned long      config;

	ARISC_INF("device [%s] pin resource request enter\n", dev_name(&pdev->dev));
	/*
	 * request arisc resources:
	 * p2wi/rsb gpio...
	 */
	/* get pin sys_config info */
#if defined CONFIG_ARCH_SUN8IW1P1
	pin_count = script_get_pio_list ("s_p2twi0", &pin_list);
#elif (defined CONFIG_ARCH_SUN8IW3P1) || (defined CONFIG_ARCH_SUN8IW5P1) || (defined CONFIG_ARCH_SUN8IW6P1) || \
      (defined CONFIG_ARCH_SUN8IW7P1) || (defined CONFIG_ARCH_SUN8IW9P1)
	pin_count = script_get_pio_list ("s_rsb0", &pin_list);
#elif defined CONFIG_ARCH_SUN9IW1P1
	pin_count = script_get_pio_list ("s_rsb0", &pin_list);
#else
#error "please select a platform\n"
#endif

	if (pin_count == 0) {
		/* "s_p2twi0" or "s_rsb0" have no pin configuration */
		ARISC_WRN("arisc s_p2twi0/s_rsb0 have no pin configuration\n");
		return -EINVAL;
	}

	/* request pin individually */
	for (pin_index = 0; pin_index < pin_count; pin_index++) {
		pin_cfg = &(pin_list[pin_index].gpio);

		/* valid pin of sunxi-pinctrl, config pin attributes individually.*/
		sunxi_gpio_to_name(pin_cfg->gpio, pin_name);
		config = SUNXI_PINCFG_PACK(SUNXI_PINCFG_TYPE_FUNC, pin_cfg->mul_sel);
		pin_config_set(SUNXI_PINCTRL, pin_name, config);
		if (pin_cfg->pull != GPIO_PULL_DEFAULT) {
			config = SUNXI_PINCFG_PACK(SUNXI_PINCFG_TYPE_PUD, pin_cfg->pull);
			pin_config_set (SUNXI_PINCTRL, pin_name, config);
		}
		if (pin_cfg->drv_level != GPIO_DRVLVL_DEFAULT) {
			config = SUNXI_PINCFG_PACK(SUNXI_PINCFG_TYPE_DRV, pin_cfg->drv_level);
			pin_config_set (SUNXI_PINCTRL, pin_name, config);
		}
		if (pin_cfg->data != GPIO_DATA_DEFAULT) {
			config = SUNXI_PINCFG_PACK(SUNXI_PINCFG_TYPE_DAT, pin_cfg->data);
			pin_config_set (SUNXI_PINCTRL, pin_name, config);
		}
	}

	/*
	 * request arisc resources:
	 * uart gpio...
	 */
	type = script_get_item("s_uart0", "s_uart_used", &script_val);
	if (SCIRPT_ITEM_VALUE_TYPE_INT != type) {
		ARISC_WRN("sys_config.fex have no arisc s_uart0 config!");
		script_val.val = 0;
	}
	if (1 == script_val.val) {
		pin_count = script_get_pio_list ("s_uart0", &pin_list);
		if (pin_count == 0) {
			/* "s_uart0" have no pin configuration */
			ARISC_WRN("arisc s_uart0 have no pin configuration\n");
			return -EINVAL;
		}

		/* request pin individually */
		for (pin_index = 0; pin_index < pin_count; pin_index++) {
			pin_cfg = &(pin_list[pin_index].gpio);

			/* valid pin of sunxi-pinctrl, config pin attributes individually.*/
			sunxi_gpio_to_name(pin_cfg->gpio, pin_name);
			config = SUNXI_PINCFG_PACK(SUNXI_PINCFG_TYPE_FUNC, pin_cfg->mul_sel);
			pin_config_set(SUNXI_PINCTRL, pin_name, config);
			if (pin_cfg->pull != GPIO_PULL_DEFAULT) {
				config = SUNXI_PINCFG_PACK(SUNXI_PINCFG_TYPE_PUD, pin_cfg->pull);
				pin_config_set (SUNXI_PINCTRL, pin_name, config);
			}
			if (pin_cfg->drv_level != GPIO_DRVLVL_DEFAULT) {
				config = SUNXI_PINCFG_PACK(SUNXI_PINCFG_TYPE_DRV, pin_cfg->drv_level);
				pin_config_set (SUNXI_PINCTRL, pin_name, config);
			}
			if (pin_cfg->data != GPIO_DATA_DEFAULT) {
				config = SUNXI_PINCFG_PACK(SUNXI_PINCFG_TYPE_DAT, pin_cfg->data);
				pin_config_set (SUNXI_PINCTRL, pin_name, config);
			}
		}
	}
	ARISC_INF("arisc uart debug config [%s] [%s] : %d\n", "s_uart0", "s_uart_used", script_val.val);

	/*
	 * request arisc resources:
	 * jtag gpio...
	 */
	type = script_get_item("s_jtag0", "s_jtag_used", &script_val);
	if (SCIRPT_ITEM_VALUE_TYPE_INT != type) {
		ARISC_WRN("sys_config.fex have no arisc s_jtag0 config!");
		script_val.val = 0;
	}
	if (script_val.val) {
		pin_count = script_get_pio_list ("s_jtag0", &pin_list);
		if (pin_count == 0) {
			/* "s_jtag0" have no pin configuration */
			ARISC_WRN("arisc s_jtag0 have no pin configuration\n");
			return -EINVAL;
		}

		/* request pin individually */
		for (pin_index = 0; pin_index < pin_count; pin_index++) {
			pin_cfg = &(pin_list[pin_index].gpio);

			/* valid pin of sunxi-pinctrl, config pin attributes individually.*/
			sunxi_gpio_to_name(pin_cfg->gpio, pin_name);
			config = SUNXI_PINCFG_PACK(SUNXI_PINCFG_TYPE_FUNC, pin_cfg->mul_sel);
			pin_config_set(SUNXI_PINCTRL, pin_name, config);
			if (pin_cfg->pull != GPIO_PULL_DEFAULT) {
				config = SUNXI_PINCFG_PACK(SUNXI_PINCFG_TYPE_PUD, pin_cfg->pull);
				pin_config_set (SUNXI_PINCTRL, pin_name, config);
			}
			if (pin_cfg->drv_level != GPIO_DRVLVL_DEFAULT) {
				config = SUNXI_PINCFG_PACK(SUNXI_PINCFG_TYPE_DRV, pin_cfg->drv_level);
				pin_config_set (SUNXI_PINCTRL, pin_name, config);
			}
			if (pin_cfg->data != GPIO_DATA_DEFAULT) {
				config = SUNXI_PINCFG_PACK(SUNXI_PINCFG_TYPE_DAT, pin_cfg->data);
				pin_config_set (SUNXI_PINCTRL, pin_name, config);
			}
		}
	}
	ARISC_INF("arisc jtag debug config [%s] [%s] : %d\n", "s_jtag0", "s_jtag_used", script_val.val);

	ARISC_INF("device [%s] pin resource request ok\n", dev_name(&pdev->dev));

	return 0;
}

static s32 sunxi_arisc_para_init(struct arisc_para *para)
{
	script_item_u script_val;
	script_item_value_type_e type;

	/* init para */
	memset(para, 0, ARISC_PARA_SIZE);
#if defined CONFIG_ARCH_SUN9IW1P1

	/* parse arisc->machine */
	para->machine = ARISC_MACHINE_PAD;
	type = script_get_item("arisc", "machine", &script_val);
	if (SCIRPT_ITEM_VALUE_TYPE_STR != type) {
		ARISC_ERR("arisc->machine config type err or can not be found!\n");
		return -EINVAL;
	}

	if (!strcmp(script_val.str, "homlet proto")) {
		para->machine = ARISC_MACHINE_HOMLET;
	}
	ARISC_LOG("arisc->machine:%s\n", script_val.str);

	/* parse arisc->oz_scale_delay */
	type = script_get_item("arisc", "oz_scale_delay", &script_val);
	if (SCIRPT_ITEM_VALUE_TYPE_INT != type) {
		ARISC_ERR("arisc->oz_scale_delay config type err or can not be found!\n");
		return -EINVAL;
	}

	para->oz_scale_delay = script_val.val;
	ARISC_LOG("arisc->oz_scale_delay:%u\n", para->oz_scale_delay);

	/* parse arisc->oz_onoff_delay */
	type = script_get_item("arisc", "oz_onoff_delay", &script_val);
	if (SCIRPT_ITEM_VALUE_TYPE_INT != type) {
		ARISC_ERR("arisc->oz_onoff_delay config type err or can not be found!\n");
		return -EINVAL;
	}

	para->oz_onoff_delay = script_val.val;
	ARISC_LOG("arisc->oz_onoff_delay:%u\n", para->oz_onoff_delay);
#endif
	/* parse s_uart_pin_used */
	type = script_get_item("s_uart0", "s_uart_used", &script_val);
	if (SCIRPT_ITEM_VALUE_TYPE_INT != type) {
		ARISC_WRN("sys_config.fex have no arisc s_uart0 config!");
		script_val.val = 0;
	}
	if (1 == script_val.val)
		para->uart_pin_used = script_val.val;
	else
		para->uart_pin_used = 0;

	return 0;
}

static void sunxi_arisc_setup_para(struct arisc_para *para)
{
	void *dest;

	dest = (void *)(arisc_sram_a2_vbase + ARISC_PARA_ADDR_OFFSET);

	/* copy arisc parameters to target address */
	memcpy(dest, (void *)para, ARISC_PARA_SIZE);
	ARISC_INF("setup arisc para sram_a2 finished\n");
}

int sunxi_deassert_arisc(void)
{
	ARISC_INF("set arisc reset to de-assert state\n");
#if (defined CONFIG_ARCH_SUN8IW1P1) || (defined CONFIG_ARCH_SUN8IW3P1) || (defined CONFIG_ARCH_SUN8IW5P1) || \
    (defined CONFIG_ARCH_SUN8IW6P1) || (defined CONFIG_ARCH_SUN8IW7P1) || (defined CONFIG_ARCH_SUN8IW9P1)
	{
		volatile unsigned long value;
		value = readl((IO_ADDRESS(SUNXI_R_CPUCFG_PBASE) + 0x0));
		value &= ~1;
		writel(value, (IO_ADDRESS(SUNXI_R_CPUCFG_PBASE) + 0x0));
		value = readl((IO_ADDRESS(SUNXI_R_CPUCFG_PBASE) + 0x0));
		value |= 1;
		writel(value, (IO_ADDRESS(SUNXI_R_CPUCFG_PBASE) + 0x0));
	}
#elif defined CONFIG_ARCH_SUN9IW1P1
	{
		volatile unsigned long value;
		value = readl((IO_ADDRESS(SUNXI_R_PRCM_PBASE) + 0x0));
		value &= ~1;
		writel(value, (IO_ADDRESS(SUNXI_R_PRCM_PBASE) + 0x0));
		value = readl((IO_ADDRESS(SUNXI_R_PRCM_PBASE) + 0x0));
		value |= 1;
		writel(value, (IO_ADDRESS(SUNXI_R_PRCM_PBASE) + 0x0));
	}
#endif

	return 0;
}

u32 sunxi_load_arisc(void *image, u32 image_size, void *para, u32 para_size)
{
	u32   ret;
	void *dest;
	
	if (sunxi_soc_is_secure()) {
		ret = call_firmware_op(load_arisc, image, image_size, 
		                       para, para_size, ARISC_PARA_ADDR_OFFSET);
	} else {
		/* clear sram_a2 area */
		memset((void *)arisc_sram_a2_vbase, 0, SUNXI_SRAM_A2_SIZE);
		
		/* load arisc system binary data to sram_a2 */
		memcpy((void *)arisc_sram_a2_vbase, image, image_size);
		ARISC_INF("load arisc image finish\n");
		
		/* setup arisc parameters */
		dest = (void *)(arisc_sram_a2_vbase + ARISC_PARA_ADDR_OFFSET);
		memcpy(dest, (void *)para, para_size);
		ARISC_INF("setup arisc para finish\n");
		
		/* relese arisc reset */
		sunxi_deassert_arisc();
		ARISC_INF("release arisc reset finish\n");
	}
	ARISC_INF("load arisc finish\n");
	
	return 0;
}

static int  sunxi_arisc_probe(struct platform_device *pdev)
{
	int   binary_len;
	int   ret;
	struct arisc_para para;
	u32    message_addr;
	u32    message_phys;
	u32    message_size;

	ARISC_INF("arisc initialize\n");
	
	/* cfg sunxi arisc clk */
	ret = sunxi_arisc_clk_cfg(pdev);
	if (ret) {
		ARISC_ERR("sunxi-arisc clk cfg failed\n");
		return -EINVAL;
	}

	/* cfg sunxi arisc pin */
	ret = sunxi_arisc_pin_cfg(pdev);
	if (ret) {
		ARISC_ERR("sunxi-arisc pin cfg failed\n");
		return -EINVAL;
	}

	ARISC_INF("sram_a2 vaddr(%x)\n", (unsigned int)arisc_sram_a2_vbase);
	ARISC_INF("sram_a2 lengt(%x)\n", (unsigned int)SUNXI_SRAM_A2_SIZE);

#if (defined CONFIG_ARCH_SUN8IW1P1) || (defined CONFIG_ARCH_SUN8IW3P1) || (defined CONFIG_ARCH_SUN8IW5P1) || \
    (defined CONFIG_ARCH_SUN8IW6P1) || (defined CONFIG_ARCH_SUN8IW9P1)
	binary_len = 0x13000;
#elif (defined CONFIG_ARCH_SUN8IW7P1)
	binary_len = SUNXI_SRAM_A2_SIZE;
#elif defined CONFIG_ARCH_SUN9IW1P1
	binary_len = (int)(&arisc_binary_end) - (int)(&arisc_binary_start);
#endif

#if (defined CONFIG_ARCH_SUN8IW7P1)
	if ((int)(&arisc_binary_end) - (int)(&arisc_binary_start) - binary_len > SUPER_STANDBY_MEM_SIZE)
		ARISC_ERR("reserve dram space littler than cpus code!");
	memcpy((void *)phys_to_virt(SUPER_STANDBY_MEM_BASE), \
	       (void *)(((unsigned char *)&arisc_binary_start) + binary_len), \
	       (int)(&arisc_binary_end) - (int)(&arisc_binary_start) - binary_len);
	ARISC_INF("move arisc binary data1 [addr = %p, len = %x] to dram:%p finished\n",
			 (void *)(((unsigned char *)&arisc_binary_start) + binary_len), \
			 (int)(&arisc_binary_end) - (int)(&arisc_binary_start) - binary_len, \
			 (void *)SUPER_STANDBY_MEM_BASE);
#endif
	/* initialize hwspinlock */
	ARISC_INF("hwspinlock initialize\n");
	arisc_hwspinlock_init();

	/* initialize hwmsgbox */
	ARISC_INF("hwmsgbox initialize\n");
	arisc_hwmsgbox_init();
	
	/* setup arisc parameter */
	sunxi_arisc_para_init(&para);
	sunxi_arisc_setup_para(&para);

	/* allocate shared message buffer,
	 * the shared buffer should be non-cacheable.
	 * secure    : sram-a2 last 4k byte;
	 * non-secure: dram non-cacheable buffer.
	 */
	if (sunxi_soc_is_secure()) {
		message_addr = (u32)dma_alloc_coherent(NULL, PAGE_SIZE, &(message_phys), GFP_KERNEL);
		message_size = PAGE_SIZE;
		para.message_pool_phys = message_phys;	 
		para.message_pool_size = message_size;
	} else {
		/* use sram-a2 last 4k byte */
		message_addr = (u32)arisc_sram_a2_vbase + ARISC_MESSAGE_POOL_START;
		message_size = ARISC_MESSAGE_POOL_END - ARISC_MESSAGE_POOL_START;
		para.message_pool_phys = ARISC_MESSAGE_POOL_START;
		para.message_pool_size = ARISC_MESSAGE_POOL_END - ARISC_MESSAGE_POOL_START;
	}
	
	/* initialize message manager */
	ARISC_INF("message manager initialize\n");
	arisc_message_manager_init((void *)message_addr, message_size);


	/* load arisc */
	sunxi_load_arisc((void *)(&arisc_binary_start), binary_len, 
	                 (void *)(&para), sizeof(struct arisc_para));

	/* wait arisc ready */
	ARISC_INF("wait arisc ready....\n");
	if (arisc_wait_ready(10000)) {
		ARISC_LOG("arisc startup failed\n");
	}

	/* enable arisc asyn tx interrupt */
	arisc_hwmsgbox_enable_receiver_int(ARISC_HWMSGBOX_ARISC_ASYN_TX_CH, AW_HWMSG_QUEUE_USER_AC327);

	/* enable arisc syn tx interrupt */
	arisc_hwmsgbox_enable_receiver_int(ARISC_HWMSGBOX_ARISC_SYN_TX_CH, AW_HWMSG_QUEUE_USER_AC327);

	/*
	 * detect sunxi chip id
	 * include soc chip id, pmu chip id and serial.
	 */
	sunxi_chip_id_init();
	
#ifndef CONFIG_ARCH_SUN8IW7P1
	/* config dvfs v-f table */
	if (arisc_dvfs_cfg_vf_table()) {
		ARISC_WRN("config dvfs v-f table failed\n");
	}
#endif

#if (defined CONFIG_ARCH_SUN8IW1P1) || (defined CONFIG_ARCH_SUN8IW6P1) || \
    (defined CONFIG_ARCH_SUN9IW1P1)
	/* config ir config paras */
	if (arisc_config_ir_paras()) {
		ARISC_WRN("config ir paras failed\n");
	}
#endif

#if (defined CONFIG_ARCH_SUN8IW1P1) || (defined CONFIG_ARCH_SUN8IW3P1) || \
    (defined CONFIG_ARCH_SUN8IW5P1) || (defined CONFIG_ARCH_SUN8IW6P1)
	/* config pmu config paras */
	if (arisc_config_pmu_paras()) {
		ARISC_WRN("config pmu paras failed\n");
	}
#endif

#ifndef CONFIG_ARCH_SUN8IW7P1
	/* config dram config paras */
	if (arisc_config_dram_paras()) {
		ARISC_WRN("config dram paras failed\n");
	}
#endif

#if (defined CONFIG_ARCH_SUN8IW5P1) || (defined CONFIG_ARCH_SUN8IW6P1) || (defined CONFIG_ARCH_SUN9IW1P1)
	/* config standby power paras */
	if (arisc_sysconfig_sstpower_paras()) {
		ARISC_WRN("config sst power paras failed\n");
	}
#endif
	atomic_set(&arisc_suspend_flag, 0);

	/* arisc initialize succeeded */
	ARISC_LOG("sunxi-arisc driver v%s startup succeeded\n", DRV_VERSION);

	return 0;
}

/* msgbox irq no */
static struct resource sunxi_arisc_resource[] = {
	[0] = {
		.start = SUNXI_IRQ_MBOX,
		.end   = SUNXI_IRQ_MBOX,
		.flags = IORESOURCE_IRQ,
	},
};

static struct platform_device sunxi_arisc_device = {
	.name           = DEV_NAME,
	.id             = PLATFORM_DEVID_NONE,
	.num_resources  = ARRAY_SIZE(sunxi_arisc_resource),
	.resource       = sunxi_arisc_resource,
};

static struct platform_driver sunxi_arisc_driver = {
	.probe      = sunxi_arisc_probe,
	.shutdown   = sunxi_arisc_shutdown,
	.driver     = {
		.name     = DRV_NAME,
		.owner    = THIS_MODULE,
		.pm       = SUNXI_ARISC_DEV_PM_OPS,
	},
};

static int __init arisc_init(void)
{
	int ret;

	ARISC_LOG("sunxi-arisc driver v%s\n", DRV_VERSION);

	ret = platform_driver_register(&sunxi_arisc_driver);
	if (IS_ERR_VALUE(ret)) {
		ARISC_ERR("register sunxi arisc platform driver failed\n");
		goto err_platform_driver_register;
	}
	ret = platform_device_register(&sunxi_arisc_device);
	if (IS_ERR_VALUE(ret)) {
		ARISC_ERR("register sunxi arisc platform device failed\n");
		goto err_platform_device_register;
	}

	sunxi_arisc_sysfs(&sunxi_arisc_device);

	/* arisc init ok */
	arisc_notify(ARISC_INIT_READY, NULL);

	return 0;

err_platform_device_register:
	platform_device_unregister(&sunxi_arisc_device);
err_platform_driver_register:
	platform_driver_unregister(&sunxi_arisc_driver);
	return -EINVAL;
}

static void __exit arisc_exit(void)
{
	platform_device_unregister(&sunxi_arisc_device);
	platform_driver_unregister(&sunxi_arisc_driver);
	ARISC_LOG("module unloaded\n");
}

subsys_initcall(arisc_init);
module_exit(arisc_exit);

MODULE_DESCRIPTION("SUNXI ARISC Driver");
MODULE_AUTHOR("Superm Wu <superm@allwinnertech.com>");
MODULE_LICENSE("GPL");
MODULE_VERSION(DRV_VERSION);
MODULE_ALIAS("platform:sunxi arisc driver");
