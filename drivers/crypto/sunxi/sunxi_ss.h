/*
 * Some macro and struct of SUNXI SecuritySystem controller.
 *
 * Copyright (C) 2013 Allwinner.
 *
 * Mintow <duanmintao@allwinnertech.com>
 *
 * This file is licensed under the terms of the GNU General Public
 * License version 2.  This program is licensed "as is" without any
 * warranty of any kind, whether express or implied.
 */

#ifndef _SUNXI_SECURITY_SYSTEM_H_
#define _SUNXI_SECURITY_SYSTEM_H_

#include <crypto/aes.h>
#include <crypto/sha.h>
#include <crypto/hash.h>
#include <crypto/algapi.h>

#include <linux/scatterlist.h>
#include <linux/interrupt.h>

/* flag for sunxi_ss_t.flags */
#define SS_FLAG_MODE_MASK		0xFF
#define SS_FLAG_NEW_KEY			BIT(0)
#define SS_FLAG_NEW_IV			BIT(1)
#define SS_FLAG_INIT			BIT(2)
#define SS_FLAG_FAST			BIT(3)
#define SS_FLAG_BUSY			BIT(4)
#define SS_FLAG_TRNG			BIT(8)

/* flag for crypto_async_request.flags */
#define SS_FLAG_AES				BIT(16)
#define SS_FLAG_HASH			BIT(17)

/* Define the capability of SS controller. */
#ifdef CONFIG_ARCH_SUN9IW1
#define SS_CTR_MODE_ENABLE		1
#define SS_CTS_MODE_ENABLE		1
/* #define SS_SHA224_ENABLE		1 sun9i don't support arbitrary mode of sha224 */
#define SS_SHA256_ENABLE		1
#define SS_TRNG_ENABLE			1
#define SS_RSA2048_ENABLE		1
/* #define SS_CRC32_ENABLE		1 The CRC32 don't support arbitrary mode */
#define SS_IDMA_ENABLE			1
#define SS_MULTI_FLOW_ENABLE	1

#define SS_SHA_SWAP_PRE_ENABLE	1 /* The initial IV need to be converted. */
#define SS_SHA_SWAP_MID_ENABLE	1 /* The middle IV need to be converted. */

#define SS_DMA_BUF_SIZE			SZ_8K

#define SS_RSA_MIN_SIZE			(512/8)  /* in Bytes. 512 bits */
#define SS_RSA_MAX_SIZE			(2048/8) /* in Bytes. 2048 bits */

#define SS_CLK_RATE				320000000 /* 320 MHz */
#define SS_PLL_CLK				PLL4_CLK
#define SS_FLOW_NUM				2
#endif

#ifdef CONFIG_ARCH_SUN8IW6
#define SS_CTR_MODE_ENABLE		1
#define SS_CTS_MODE_ENABLE		1
#define SS_SHA224_ENABLE		1
#define SS_SHA256_ENABLE		1
#define SS_TRNG_ENABLE			1
#define SS_RSA512_ENABLE		1
#define SS_RSA1024_ENABLE		1
#define SS_RSA2048_ENABLE		1
#define SS_RSA3072_ENABLE		1
/* #define SS_CRC32_ENABLE		1 The CRC32 don't support arbitrary mode */
#define SS_IDMA_ENABLE			1
#define SS_MULTI_FLOW_ENABLE	1

#define SS_SHA_SWAP_PRE_ENABLE	1 /* The initial IV need to be converted. */

#define SS_DMA_BUF_SIZE			SZ_8K

#define SS_RSA_MIN_SIZE			(512/8)  /* in Bytes. 512 bits */
#define SS_RSA_MAX_SIZE			(3072/8) /* in Bytes. 3072 bits */

#define SS_CLK_RATE				300000000 /* 300 MHz */
#define SS_PLL_CLK				PLL_PERIPH_CLK
#define SS_FLOW_NUM				2
#endif

#if defined(CONFIG_ARCH_SUN8IW1)
#define SS_SHA_SWAP_FINAL_ENABLE 1 /* The final digest need to be converted. */
#endif

#if defined(CONFIG_ARCH_SUN8IW5)
/* #define SS_CTR_MODE_ENABLE	1 The counter need to be increased by software in continue mode. */
/* #define SS_CTS_MODE_ENABLE	1 The continue mode doesn't support, so comment it. */
#define SS_SHA_SWAP_PRE_ENABLE	1 /* The initial IV need to be swapped. */
#define SS_SHA_NO_SWAP_IV4		1 /* A hw bug. The 4th word of IV needn't to be swapped. */
#endif

#if defined(CONFIG_ARCH_SUN8IW8)
#define SS_SHA_SWAP_PRE_ENABLE	1 /* The initial IV need to be swapped. */
#endif

#if defined(CONFIG_ARCH_SUN8IW7) || defined(CONFIG_ARCH_SUN8IW9)
#define SS_CTR_MODE_ENABLE		1
#define SS_CTS_MODE_ENABLE		1
#define SS_CFB_MODE_ENABLE		1
#define SS_OFB_MODE_ENABLE		1
#define SS_CBC_MAC_MODE_ENABLE	1
#define SS_SHA224_ENABLE		1
#define SS_SHA256_ENABLE		1
#define SS_SHA384_ENABLE		1
#define SS_SHA512_ENABLE		1
#define SS_HMAC_SHA1_ENABLE		1

#define SS_RSA512_ENABLE		1
#define SS_RSA1024_ENABLE		1
#define SS_RSA2048_ENABLE		1
#define SS_RSA3072_ENABLE		1
#define SS_RSA4096_ENABLE		1

#define SS_DH_ENABLE			1
#define SS_ECC_ENABLE			1

#ifdef CONFIG_EVB_PLATFORM
#define SS_TRNG_ENABLE			1
#endif

#define SS_SHA_SWAP_PRE_ENABLE	1 /* The initial IV need to be converted. */

#define SS_RSA_MIN_SIZE			(512/8)  /* in Bytes. 512 bits */
#define SS_RSA_MAX_SIZE			(4096/8) /* in Bytes. 4096 bits */

#define SS_SCATTER_ENABLE		1

#define SS_CLK_RATE				300000000 /* 300 MHz */
#define SS_PLL_CLK				PLL_PERIPH0
#define SS_FLOW_NUM				4
#endif

#if defined(SS_RSA512_ENABLE) || defined(SS_RSA1024_ENABLE) \
	|| defined(SS_RSA2048_ENABLE) || defined(SS_RSA3072_ENABLE) \
	|| defined(SS_RSA4096_ENABLE)
#define SS_RSA_ENABLE			1
#endif

#if defined(CONFIG_ARCH_SUN8IW1)
#define SS_CLK_RATE				200000000 /* 200 MHz */
#define SS_PLL_CLK				PLL6_CLK
#define SS_FLOW_NUM				1
#endif

#if defined(CONFIG_ARCH_SUN8IW5) || defined(CONFIG_ARCH_SUN8IW8)
#define SS_CLK_RATE				200000000 /* 200 MHz */
#define SS_PLL_CLK				PLL_PERIPH_CLK
#define SS_FLOW_NUM				1
#endif

#define SS_PRNG_ENABLE			1
#define SS_PRNG_SEED_LEN		(192/8) /* 192 bits */

#define SUNXI_SS_DEV_NAME	"ss"

#define SUNXI_SS_REG_BASE	SUNXI_SS_VBASE
#define SUNXI_SS_MEM_START 	SUNXI_SS_PBASE
#define SUNXI_SS_MEM_RANGE	0x400
#define SUNXI_SS_MEM_END 	(SUNXI_SS_PBASE + SUNXI_SS_MEM_RANGE - 1)

#define SUNXI_SS_IRQ		SUNXI_IRQ_SS

#if defined(SS_SHA384_ENABLE) || defined(SS_SHA512_ENABLE)
#define SS_DIGEST_SIZE 		SHA512_DIGEST_SIZE
#define SS_HASH_PAD_SIZE	(SHA512_BLOCK_SIZE * 2)
#else
#define SS_DIGEST_SIZE 		SHA256_DIGEST_SIZE
#define SS_HASH_PAD_SIZE	(SHA1_BLOCK_SIZE * 2)
#endif

#ifdef CONFIG_EVB_PLATFORM
#define SS_WAIT_TIME		200	/* 200ms, used in wait_for_completion_timeout() */
#else
#define SS_WAIT_TIME		40000 /* 40s, used in wait_for_completion_timeout() */
#endif

/* For debug */
#define SS_DBG(fmt, arg...)	pr_debug("%s()%d - "fmt, __func__, __LINE__, ##arg)
#define SS_ERR(fmt, arg...)	pr_err("%s()%d - "fmt, __func__, __LINE__, ##arg)
#define SS_EXIT()  			SS_DBG("%s \n", "Exit")
#define SS_ENTER() 			SS_DBG("%s \n", "Enter ...")

#define SS_FLOW_AVAILABLE	0
#define SS_FLOW_UNAVAILABLE	1

#ifdef SS_SCATTER_ENABLE

/* CE: Crypto Engine, start using CE from sun8iw7/sun8iw9 */
#define CE_SCATTERS_PER_TASK		8

typedef struct {
	dma_addr_t addr;
	int len; /* in word (4 bytes). Exception: in byte for AES_CTS */
} ce_scatter_t;

/* The descriptor of a CE task. */
typedef struct ce_task_desc {
	u32 chan_id;
	u32 comm_ctl;
	u32 sym_ctl;
	u32 asym_ctl;

	dma_addr_t key_addr;
	dma_addr_t iv_addr;
	dma_addr_t ctr_addr;
	u32 data_len; /* in word(4 byte). Exception: in byte for AES_CTS */

	ce_scatter_t src[CE_SCATTERS_PER_TASK];
	ce_scatter_t dst[CE_SCATTERS_PER_TASK];

	struct ce_task_desc *next;
	u32 reserved[3];
} ce_task_desc_t;

#endif

typedef struct {
	int dir;
	struct dma_chan *chan;
	int nents;
	struct scatterlist *sg;
#ifdef SS_IDMA_ENABLE
	struct sg_table sgt_for_cp; /* Used to copy data from/to user space. */
#endif
#ifdef SS_SCATTER_ENABLE
	int  has_padding;
	char *padding;
	dma_addr_t original;
#endif
} ss_dma_info_t;

typedef struct {
	int dir;
	int type;
	int mode;
#ifdef SS_CFB_MODE_ENABLE
	int bitwidth;	/* the bitwidth of CFB mode */
#endif
	struct completion done;
	ss_dma_info_t dma_src;
	ss_dma_info_t dma_dst;
} ss_aes_req_ctx_t;

/* The common context of AES and HASH */
typedef struct {
	u32 flow;
	u32 flags;
} ss_comm_ctx_t;

typedef struct {
	ss_comm_ctx_t comm; /* must be in the front. */

#ifdef SS_RSA_ENABLE
	u8  key[SS_RSA_MAX_SIZE];
	u8  iv[SS_RSA_MAX_SIZE];
#else
	u8	key[AES_MAX_KEY_SIZE];
	u8	iv[AES_MAX_KEY_SIZE];
#endif
#ifdef SS_SCATTER_ENABLE
	u8  next_iv[AES_MAX_KEY_SIZE];	/* saved the next IV/Counter in continue mode */
#endif
	int key_size;
	int iv_size;
	int cnt;	/* in Byte */
} ss_aes_ctx_t;

typedef struct {
	ss_comm_ctx_t comm; /* must be in the front. */

	u8  md[SS_DIGEST_SIZE];
	u8  pad[SS_HASH_PAD_SIZE];
	int md_size;
	int cnt;	/* in Byte */
} ss_hash_ctx_t;

typedef struct {
#ifdef SS_IDMA_ENABLE
	char *buf_src;
	dma_addr_t buf_src_dma;
	char *buf_dst;
	dma_addr_t buf_dst_dma;
#endif
#ifdef SS_SCATTER_ENABLE
	ce_task_desc_t task;
#endif
	struct completion done;
	int available;
} ss_flow_t;

typedef struct {
    struct platform_device *pdev;
	void __iomem *base_addr; /* for register */
	u32 base_addr_phy;

	struct clk *pclk;  /* PLL clock */
	struct clk *mclk;  /* module clock */
	unsigned int irq; /* irq NO. */
	char dev_name[8];

	ss_flow_t flows[SS_FLOW_NUM];

	struct workqueue_struct *workqueue;
	struct work_struct work;
	struct crypto_queue queue;
	spinlock_t lock;

	int suspend;
} sunxi_ss_t;

/* Global variable */

extern sunxi_ss_t *ss_dev;

/* Inner functions declaration */

void ss_hash_swap(char *data, int len);
void ss_dev_lock(void);
void ss_dev_unlock(void);
void ss_reset(void);

void print_hex(char *_data, int _len, int _addr);
void ss_hash_padding_data_prepare(ss_hash_ctx_t *ctx, char *tail, int len);
void ss_hash_padding_sg_prepare(struct scatterlist *last, int total);
int ss_hash_padding(ss_hash_ctx_t *ctx, int type);
int ss_hash_blk_size(int type);

#endif /* end of _SUNXI_SECURITY_SYSTEM_H_ */

