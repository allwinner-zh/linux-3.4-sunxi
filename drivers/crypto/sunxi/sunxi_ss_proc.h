/*
 * Declare the function interface of SUNXI SS process.
 *
 * Copyright (C) 2014 Allwinner.
 *
 * Mintow <duanmintao@allwinnertech.com>
 *
 * This file is licensed under the terms of the GNU General Public
 * License version 2.  This program is licensed "as is" without any
 * warranty of any kind, whether express or implied.
 */

#ifndef _SUNXI_SECURITY_SYSTEM_PROC_H_
#define _SUNXI_SECURITY_SYSTEM_PROC_H_

#include <crypto/aes.h>
#include <crypto/sha.h>
#include <crypto/algapi.h>
#include <linux/scatterlist.h>

/* Inner functions declaration, defined in vx/sunxi_ss_proc.c */

int ss_aes_key_valid(struct crypto_ablkcipher *tfm, int len);
int ss_aes_crypt(struct ablkcipher_request *req, int dir, int method, int mode);

int ss_rng_get_random(struct crypto_rng *tfm, u8 *rdata, unsigned int dlen);
#ifdef SS_TRNG_ENABLE
int ss_trng_get_random(struct crypto_rng *tfm, u8 *rdata, unsigned int dlen);
#endif

int ss_hash_update(struct ahash_request *req);
int ss_hash_final(struct ahash_request *req);
int ss_hash_finup(struct ahash_request *req);

irqreturn_t sunxi_ss_irq_handler(int irq, void *dev_id);
void sunxi_ss_work(struct work_struct *work);

#endif /* end of _SUNXI_SECURITY_SYSTEM_PROC_H_ */

