/*
 * The driver of SUNXI SecuritySystem controller.
 *
 * Copyright (C) 2013 Allwinner.
 *
 * Mintow <duanmintao@allwinnertech.com>
 *
 * This file is licensed under the terms of the GNU General Public
 * License version 2.  This program is licensed "as is" without any
 * warranty of any kind, whether express or implied.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/spinlock.h>
#include <linux/errno.h>
#include <linux/clk.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <crypto/md5.h>
#include <crypto/des.h>
#include <crypto/internal/hash.h>
#include <crypto/internal/rng.h>

#include <linux/clk/sunxi_name.h>

#include "../sunxi_ss.h"
#include "../sunxi_ss_proc.h"
#include "sunxi_ss_reg.h"

void ss_task_desc_init(ce_task_desc_t *task, u32 flow)
{
	memset(task, 0, sizeof(ce_task_desc_t));
	task->chan_id = flow;
	task->comm_ctl |= CE_COMM_CTL_TASK_INT_MASK; 
}

static int ss_sg_cnt(struct scatterlist *sg, int total)
{
	int cnt = 0;
	int nbyte = 0;
	struct scatterlist *cur = sg;
	struct scatterlist *prev = sg;

	while (cur != NULL) {
		cnt++;
		prev = cur;
		SS_DBG("cnt: %d, cur: %p, len: %d, is_last: %ld\n", cnt, cur,
				sg_dma_len(cur), sg_is_last(cur));
		nbyte += sg_dma_len(cur);
		if (nbyte >= total)
			break;

		cur = sg_next(cur);
	}

	if (!sg_is_last(prev))
		sg_mark_end(prev);
	return cnt;
}

static int ss_sg_len(struct scatterlist *sg, int total)
{
	int nbyte = 0;
	struct scatterlist *cur = sg;

	while (cur != NULL) {
		SS_DBG("cur: %p, len: %d, is_last: %ld\n", cur,
				sg_dma_len(cur), sg_is_last(cur));
		nbyte += sg_dma_len(cur);

		cur = sg_next(cur);
	}

	return nbyte;
}

static int ss_aes_align_size(int type, int mode)
{
	if ((type == SS_METHOD_ECC) || (type == SS_METHOD_HMAC_SHA1)
		|| (CE_METHOD_IS_AES(type) && (mode == SS_AES_MODE_CTS)))
		return 4;
	else
		return AES_BLOCK_SIZE;
}

static int ss_sg_config(ce_scatter_t *scatter, ss_dma_info_t *info, int type, int mode, int tail)
{
	int cnt = 0;
	int last_sg_len = 0;
	struct scatterlist *cur = info->sg;

	while (cur != NULL) {
		if (cnt >= CE_SCATTERS_PER_TASK) {
			WARN(1, "have too many scatter.\n");
			return -1;
		}
		
		scatter[cnt].addr = sg_dma_address(cur);
		scatter[cnt].len = sg_dma_len(cur)/4;
			
		last_sg_len = sg_dma_len(cur);
		SS_DBG("%d cur: 0x%p, scatter: addr 0x%08x, len %d (%d)\n",
				cnt, cur, scatter[cnt].addr, scatter[cnt].len, sg_dma_len(cur));
		cnt++;
		cur = sg_next(cur);
	}

	info->nents = cnt;
	if (tail == 0) {
		info->has_padding = 0;
		return 0;
	}

	if (CE_METHOD_IS_HASH(type)) {
		scatter[cnt-1].len -= tail/4;
		return 0;
	}
	
	/* AES-CTS/CTR/CFB/OFB need algin with word/block, so replace the last sg. */

	last_sg_len += ss_aes_align_size(0, mode) - tail;
	info->padding = kzalloc(last_sg_len, GFP_KERNEL);
	if (info->padding == NULL) {
		SS_ERR("Failed to kmalloc(%d)! \n", last_sg_len);
		return -ENOMEM;
	}
	SS_DBG("AES(%d)-%d padding: 0x%p, tail = %d/%d\n", type, mode, info->padding, tail, last_sg_len);

	info->original = scatter[cnt-1].addr;
	scatter[cnt-1].addr = virt_to_phys(info->padding);
	memcpy(info->padding, (char *)phys_to_virt(info->original), last_sg_len - ss_aes_align_size(0, mode) + tail);
	scatter[cnt-1].len = last_sg_len/4;

	info->has_padding = 1;
	return 0;
}

static void ss_aes_unpadding(ce_scatter_t *scatter, ss_dma_info_t *info, int mode, int tail)
{
	int last_sg_len = 0;
	int index = info->nents - 1;

	if (info->has_padding == 0)
		return;
	
	/* Only the dst sg need to be recovered. */
	if (info->dir == DMA_DEV_TO_MEM) {
		last_sg_len = scatter[index].len * 4;
		last_sg_len -= ss_aes_align_size(0, mode) - tail;
		memcpy((char *)phys_to_virt(info->original), info->padding, last_sg_len);
	}

	kfree(info->padding);
	info->padding = NULL;
	info->has_padding = 0;
}

static void ss_aes_map_padding(ce_scatter_t *scatter, ss_dma_info_t *info, int mode, int dir)
{
	int len = 0;
	int index = info->nents - 1;
	
	if (info->has_padding == 0)
		return;

	len = scatter[index].len * 4;
	SS_DBG("AES padding: 0x%08x, len: %d, dir: %d \n", scatter[index].addr, len, dir);
	dma_map_single(&ss_dev->pdev->dev, phys_to_virt(scatter[index].addr), len, dir);
	info->dir = dir;
}

static void ss_aes_unmap_padding(ce_scatter_t *scatter, ss_dma_info_t *info, int mode, int dir)
{
	int len = 0;
	int index = info->nents - 1;

	if (info->has_padding == 0)
		return;

	len = scatter[index].len * 4;
	SS_DBG("AES padding: 0x%08x, len: %d, dir: %d \n", scatter[index].addr, len, dir);
	dma_unmap_single(&ss_dev->pdev->dev, scatter[index].addr, len, dir);
}

static int ss_aes_start(ss_aes_ctx_t *ctx, ss_aes_req_ctx_t *req_ctx, int len)
{
	int ret = 0;
	int src_len = len;
	int align_size = 0;
	u32 flow = ctx->comm.flow;
	ce_task_desc_t *task = &ss_dev->flows[flow].task;

	ss_task_desc_init(task, flow);

	ss_pending_clear(flow);
	ss_irq_enable(flow);
	
	ss_method_set(req_ctx->dir, req_ctx->type, task);
	if ((req_ctx->type == SS_METHOD_RSA) || (req_ctx->type == SS_METHOD_DH)) {
		ss_rsa_width_set(ctx->key_size, task);
		ss_rsa_op_mode_set(req_ctx->mode, task);
	}
	else if (req_ctx->type == SS_METHOD_ECC) {
		ss_ecc_width_set(ctx->key_size, task);
		ss_ecc_op_mode_set(req_ctx->mode, task);
	}
	else if (req_ctx->type == SS_METHOD_HMAC_SHA1)
		ss_hmac_sha1_last(task);
	else
		ss_aes_mode_set(req_ctx->mode, task);

	if (CE_METHOD_IS_AES(req_ctx->type) && (req_ctx->mode == SS_AES_MODE_CFB))
		ss_cfb_bitwidth_set(req_ctx->bitwidth, task);

	SS_DBG("Flow: %d, Dir: %d, Method: %d, Mode: %d, len: %d \n", flow, req_ctx->dir,
			req_ctx->type, req_ctx->mode, len);

	ss_key_set(ctx->key, ctx->key_size, task);
	dma_map_single(&ss_dev->pdev->dev, ctx->key, ctx->key_size, DMA_MEM_TO_DEV);

	ss_iv_set(ctx->iv, ctx->iv_size, task);
	dma_map_single(&ss_dev->pdev->dev, ctx->iv, ctx->iv_size, DMA_MEM_TO_DEV);

	ss_cnt_set(ctx->next_iv, ctx->iv_size, task);
	dma_map_single(&ss_dev->pdev->dev, ctx->next_iv, ctx->iv_size, DMA_DEV_TO_MEM);
	
	align_size = ss_aes_align_size(req_ctx->type, req_ctx->mode);

	/* Prepare the src scatterlist */
	req_ctx->dma_src.nents = ss_sg_cnt(req_ctx->dma_src.sg, src_len);
	if ((req_ctx->type == SS_METHOD_ECC) || (req_ctx->type == SS_METHOD_HMAC_SHA1))
		src_len = ss_sg_len(req_ctx->dma_src.sg, len);
	dma_map_sg(&ss_dev->pdev->dev, req_ctx->dma_src.sg, req_ctx->dma_src.nents, DMA_MEM_TO_DEV);
	ss_sg_config(task->src, &req_ctx->dma_src, req_ctx->type, req_ctx->mode, src_len%align_size);
	ss_aes_map_padding(task->src, &req_ctx->dma_src, req_ctx->mode, DMA_MEM_TO_DEV);

	/* Prepare the dst scatterlist */
	req_ctx->dma_dst.nents = ss_sg_cnt(req_ctx->dma_dst.sg, len);
	dma_map_sg(&ss_dev->pdev->dev, req_ctx->dma_dst.sg, req_ctx->dma_dst.nents, DMA_DEV_TO_MEM);
	ss_sg_config(task->dst, &req_ctx->dma_dst, req_ctx->type, req_ctx->mode, len%align_size);
	ss_aes_map_padding(task->dst, &req_ctx->dma_dst, req_ctx->mode, DMA_DEV_TO_MEM);

	if ((req_ctx->type == SS_METHOD_AES) && (req_ctx->mode == SS_AES_MODE_CTS)) {
		ss_data_len_set(len, task);
/*		if (len < SZ_4K)  A bad way to determin the last packet of CTS mode. */
			ss_cts_last(task);
	}
	else
		ss_data_len_set(DIV_ROUND_UP(src_len, align_size)*align_size/4, task);

	/* Start CE controller. */
	init_completion(&ss_dev->flows[flow].done);
	dma_map_single(&ss_dev->pdev->dev, task, sizeof(ce_task_desc_t), DMA_MEM_TO_DEV);	

	SS_DBG("Before CE, COMM: 0x%08x, SYM: 0x%08x, ASYM: 0x%08x\n", task->comm_ctl, task->sym_ctl, task->asym_ctl);
	ss_ctrl_start(task);

	ret = wait_for_completion_timeout(&ss_dev->flows[flow].done, msecs_to_jiffies(SS_WAIT_TIME));
	if (ret == 0) {
		SS_ERR("Timed out\n");
		ss_reset();
		ret = -ETIMEDOUT;
	}
	ss_irq_disable(flow);

	dma_unmap_single(&ss_dev->pdev->dev, virt_to_phys(task), sizeof(ce_task_desc_t), DMA_MEM_TO_DEV);

	/* Unpadding and unmap the dst sg. */
	ss_aes_unpadding(task->dst, &req_ctx->dma_dst, req_ctx->mode, len%align_size);
	ss_aes_unmap_padding(task->dst, &req_ctx->dma_dst, req_ctx->mode, DMA_DEV_TO_MEM);
	dma_unmap_sg(&ss_dev->pdev->dev, req_ctx->dma_dst.sg, req_ctx->dma_dst.nents, DMA_DEV_TO_MEM);

	/* Unpadding and unmap the src sg. */
	ss_aes_unpadding(task->src, &req_ctx->dma_src, req_ctx->mode, src_len%align_size);
	ss_aes_unmap_padding(task->src, &req_ctx->dma_src, req_ctx->mode, DMA_MEM_TO_DEV);
	dma_unmap_sg(&ss_dev->pdev->dev, req_ctx->dma_src.sg, req_ctx->dma_src.nents, DMA_MEM_TO_DEV);

	dma_unmap_single(&ss_dev->pdev->dev, virt_to_phys(ctx->iv), ctx->iv_size, DMA_MEM_TO_DEV);
	dma_unmap_single(&ss_dev->pdev->dev, virt_to_phys(ctx->next_iv), ctx->iv_size, DMA_DEV_TO_MEM);
	/* Backup the next IV from ctr_descriptor, except CBC/CTS mode. */
	if (CE_METHOD_IS_AES(req_ctx->type) && (req_ctx->mode != SS_AES_MODE_CBC)
										&& (req_ctx->mode != SS_AES_MODE_CTS))
		memcpy(ctx->iv, ctx->next_iv, ctx->iv_size);

	dma_unmap_single(&ss_dev->pdev->dev, virt_to_phys(ctx->key), ctx->key_size, DMA_MEM_TO_DEV);

	SS_DBG("After CE, TSR: 0x%08x, ERR: 0x%08x\n", ss_reg_rd(CE_REG_TSR), ss_reg_rd(CE_REG_ERR));
	if (ss_flow_err(flow)) {
		SS_ERR("CE return error: %d \n", ss_flow_err(flow));
		return -EINVAL;
	}

	return 0;
}

int ss_aes_key_valid(struct crypto_ablkcipher *tfm, int len)
{
#if 0
	if (unlikely(len > SS_RSA_MAX_SIZE)) {
		SS_ERR("Unsupported key size: %d \n", len);
		tfm->base.crt_flags |= CRYPTO_TFM_RES_BAD_KEY_LEN;
		return -EINVAL;
	}
#endif
	return 0;
}

int ss_aes_crypt(struct ablkcipher_request *req, int dir, int method, int mode)
{
	int err = 0;
	unsigned long flags = 0;
	ss_aes_req_ctx_t *req_ctx = ablkcipher_request_ctx(req);

	SS_DBG("nbytes: %d, dec: %d, method: %d, mode: %d\n", req->nbytes, dir, method, mode);
	if (ss_dev->suspend) {
		SS_ERR("SS has already suspend. \n");
		return -EAGAIN;
	}

	req_ctx->dir  = dir;
	req_ctx->type = method;
	req_ctx->mode = mode;
	req->base.flags |= SS_FLAG_AES;

	spin_lock_irqsave(&ss_dev->lock, flags);
	err = ablkcipher_enqueue_request(&ss_dev->queue, req);
	spin_unlock_irqrestore(&ss_dev->lock, flags);

	queue_work(ss_dev->workqueue, &ss_dev->work);
	return err;
}

static int ss_rng_start(ss_aes_ctx_t *ctx, u8 *rdata, unsigned int dlen)
{
	int ret = 0;
	int flow = ctx->comm.flow;
#ifdef SS_TRNG_ENABLE
	int rng_len = DIV_ROUND_UP(dlen, 32)*32; /* align with 32 Bytes */
#else
	int rng_len = DIV_ROUND_UP(dlen, 20)*20; /* align with 20 Bytes */
#endif
	char *buf = NULL;
	ce_task_desc_t *task = &ss_dev->flows[flow].task;

	buf = kmalloc(rng_len, GFP_KERNEL);
	if (buf == NULL) {
		SS_ERR("Failed to malloc(%d) \n", rng_len);
		return -ENOMEM;
	}

	ss_task_desc_init(task, flow);

	ss_pending_clear(flow);
	ss_irq_enable(flow);

#ifdef SS_TRNG_ENABLE
	if (ctx->comm.flags & SS_FLAG_TRNG) {
		ss_method_set(SS_DIR_ENCRYPT, SS_METHOD_TRNG, task);
		ss_trng_osc_enable();
	}
	else
#endif
		ss_method_set(SS_DIR_ENCRYPT, SS_METHOD_PRNG, task);

#ifndef SS_TRNG_ENABLE
	/* Must set the seed addr in PRNG. */
	ss_key_set(ctx->key, ctx->key_size, task);
	dma_map_single(&ss_dev->pdev->dev, ctx->key, ctx->key_size, DMA_MEM_TO_DEV);
#endif

	/* Prepare the dst scatterlist */
	task->dst[0].addr = virt_to_phys(buf);
	task->dst[0].len  = rng_len/4;
	dma_map_single(&ss_dev->pdev->dev, buf, rng_len, DMA_DEV_TO_MEM);

	ss_data_len_set(rng_len/4, task);
	
	SS_DBG("Flow: %d, Request: %d, Aligned: %d \n", flow, dlen, rng_len);

	/* Start CE controller. */
	init_completion(&ss_dev->flows[flow].done);
	dma_map_single(&ss_dev->pdev->dev, task, sizeof(ce_task_desc_t), DMA_MEM_TO_DEV);

	SS_DBG("Before CE, COMM_CTL: 0x%08x, ICR: 0x%08x\n", task->comm_ctl, ss_reg_rd(CE_REG_ICR));
	ss_ctrl_start(task);

	ret = wait_for_completion_timeout(&ss_dev->flows[flow].done, msecs_to_jiffies(SS_WAIT_TIME));
	if (ret == 0) {
		SS_ERR("Timed out\n");
		ss_reset();
		ret = -ETIMEDOUT;
	}
	SS_DBG("After CE, TSR: 0x%08x, ERR: 0x%08x\n", ss_reg_rd(CE_REG_TSR), ss_reg_rd(CE_REG_ERR));
	SS_DBG("After CE, dst data: \n");

	memcpy(rdata, buf, dlen);

	dma_unmap_single(&ss_dev->pdev->dev, virt_to_phys(task), sizeof(ce_task_desc_t), DMA_MEM_TO_DEV);
	dma_unmap_single(&ss_dev->pdev->dev, virt_to_phys(buf), rng_len, DMA_DEV_TO_MEM);
#ifndef SS_TRNG_ENABLE
	dma_unmap_single(&ss_dev->pdev->dev, virt_to_phys(ctx->key), ctx->key_size, DMA_MEM_TO_DEV);
#endif

	ss_irq_disable(flow);
	ret = dlen;

#ifdef SS_TRNG_ENABLE
	if (ctx->comm.flags & SS_FLAG_TRNG)
		ss_trng_osc_disable();
#endif

	return ret;
}

int ss_rng_get_random(struct crypto_rng *tfm, u8 *rdata, unsigned int dlen)
{
	int ret = 0;
	ss_aes_ctx_t *ctx = crypto_rng_ctx(tfm);

	SS_DBG("flow = %d, rdata = %p, len = %d \n", ctx->comm.flow, rdata, dlen);
	if (ss_dev->suspend) {
		SS_ERR("SS has already suspend. \n");
		return -EAGAIN;
	}

	ss_dev_lock();
	ret = ss_rng_start(ctx, rdata, dlen);
	ss_dev_unlock();

	SS_DBG("Get %d byte random. \n", ret);

	return ret;
}

#ifdef SS_TRNG_ENABLE
int ss_trng_get_random(struct crypto_rng *tfm, u8 *rdata, unsigned int dlen)
{
	ss_aes_ctx_t *ctx = crypto_rng_ctx(tfm);

	ctx->comm.flags |= SS_FLAG_TRNG;
	return ss_rng_get_random(tfm, rdata, dlen);
}
#endif

static int ss_hash_start(ss_hash_ctx_t *ctx, ss_aes_req_ctx_t *req_ctx, int len)
{
	int ret = 0;
	int flow = ctx->comm.flow;
	char *digest = NULL;
	ce_task_desc_t *task = &ss_dev->flows[flow].task;

	/* Total len is too small, so process it in the padding data later. */
	if (len < ss_hash_blk_size(req_ctx->type)) {
		ctx->cnt += len;
		return 0;
	}

	digest = (char *)kmalloc(SHA512_DIGEST_SIZE, GFP_KERNEL);
	if (digest == NULL) {
		SS_ERR("Failed to kmalloc(%d) \n", SHA512_DIGEST_SIZE);
		return -ENOMEM;
	}

	ss_task_desc_init(task, flow);

	ss_pending_clear(flow);
	ss_irq_enable(flow);

	ss_method_set(req_ctx->dir, req_ctx->type, task);

	SS_DBG("Flow: %d, Dir: %d, Method: %d, Mode: %d, len: %d / %d \n", flow,
			req_ctx->dir, req_ctx->type, req_ctx->mode, len, ctx->cnt);
	SS_DBG("IV address = 0x%p, size = %d\n", ctx->md, ctx->md_size);

	ss_iv_set(ctx->md, ctx->md_size, task);
	ss_iv_mode_set(CE_HASH_IV_INPUT, task);
	dma_map_single(&ss_dev->pdev->dev, ctx->md, ctx->md_size, DMA_MEM_TO_DEV);

	ss_data_len_set((len - len%ss_hash_blk_size(req_ctx->type))/4, task);

	/* Prepare the src scatterlist */
	req_ctx->dma_src.nents = ss_sg_cnt(req_ctx->dma_src.sg, len);
	dma_map_sg(&ss_dev->pdev->dev, req_ctx->dma_src.sg, req_ctx->dma_src.nents, DMA_MEM_TO_DEV);
	ss_sg_config(task->src, &req_ctx->dma_src, req_ctx->type, 0, len%ss_hash_blk_size(req_ctx->type));

	/* Prepare the dst scatterlist */
	task->dst[0].addr = virt_to_phys(digest);
	task->dst[0].len  = ctx->md_size/4;
	dma_map_single(&ss_dev->pdev->dev, digest, SHA512_DIGEST_SIZE, DMA_DEV_TO_MEM);

	/* Start CE controller. */
	init_completion(&ss_dev->flows[flow].done);
	dma_map_single(&ss_dev->pdev->dev, task, sizeof(ce_task_desc_t), DMA_MEM_TO_DEV);	
	
	SS_DBG("Before CE, COMM_CTL: 0x%08x, ICR: 0x%08x\n", task->comm_ctl, ss_reg_rd(CE_REG_ICR));
	ss_ctrl_start(task);

	ret = wait_for_completion_timeout(&ss_dev->flows[flow].done, msecs_to_jiffies(SS_WAIT_TIME));
	if (ret == 0) {
		SS_ERR("Timed out\n");
		ss_reset();
		ret = -ETIMEDOUT;
	}
	ss_irq_disable(flow);
	
	dma_unmap_single(&ss_dev->pdev->dev, virt_to_phys(task), sizeof(ce_task_desc_t), DMA_MEM_TO_DEV);
	dma_unmap_single(&ss_dev->pdev->dev, virt_to_phys(digest), SHA512_DIGEST_SIZE, DMA_DEV_TO_MEM);
	dma_unmap_single(&ss_dev->pdev->dev, virt_to_phys(ctx->md), ctx->md_size, DMA_MEM_TO_DEV);
	dma_unmap_sg(&ss_dev->pdev->dev, req_ctx->dma_src.sg, req_ctx->dma_src.nents, DMA_MEM_TO_DEV);

	SS_DBG("After CE, TSR: 0x%08x, ERR: 0x%08x\n", ss_reg_rd(CE_REG_TSR), ss_reg_rd(CE_REG_ERR));
	SS_DBG("After CE, dst data: \n");
	print_hex(digest, SHA512_DIGEST_SIZE, (int)digest);

	if (ss_flow_err(flow)) {
		SS_ERR("CE return error: %d \n", ss_flow_err(flow));
		kfree(digest);
		return -EINVAL;
	}

	/* Backup the MD to ctx->md. */
	memcpy(ctx->md, digest, ctx->md_size);

	ctx->cnt += len;
	kfree(digest);
	return 0;
}

int ss_hash_update(struct ahash_request *req)
{
	int err = 0;
	unsigned long flags = 0;

	if (!req->nbytes) {
		SS_ERR("Invalid length: %d. \n", req->nbytes);
		return 0;
	}

	SS_DBG("Flags: %#x, len = %d \n", req->base.flags, req->nbytes);
	if (ss_dev->suspend) {
		SS_ERR("SS has already suspend. \n");
		return -EAGAIN;
	}

	req->base.flags |= SS_FLAG_HASH;

	spin_lock_irqsave(&ss_dev->lock, flags);
	err = ahash_enqueue_request(&ss_dev->queue, req);
	spin_unlock_irqrestore(&ss_dev->lock, flags);

	queue_work(ss_dev->workqueue, &ss_dev->work);
	return err;
}

int ss_hash_final(struct ahash_request *req)
{
	int pad_len = 0;
	ss_aes_req_ctx_t *req_ctx = ahash_request_ctx(req);
	ss_hash_ctx_t *ctx = crypto_ahash_ctx(crypto_ahash_reqtfm(req));
	struct scatterlist last = {0}; /* make a sg struct for padding data. */

	SS_ENTER();

	if (req->result == NULL) {
		SS_ERR("Invalid result porinter. \n");
		return -EINVAL;
	}
	SS_DBG("Method: %d, cnt: %d\n", req_ctx->type, ctx->cnt);
	if (ss_dev->suspend) {
		SS_ERR("SS has already suspend. \n");
		return -EAGAIN;
	}

	/* Process the padding data. */
	pad_len = ss_hash_padding(ctx, req_ctx->type);
	SS_DBG("Pad len: %d \n", pad_len);
	
	req_ctx->dma_src.sg = &last;
	sg_init_table(&last, 1);
	sg_set_buf(&last, ctx->pad, pad_len);
	SS_DBG("Padding data: \n");
	print_hex(ctx->pad, 128, (int)ctx->pad);

	ss_dev_lock();
	ss_hash_start(ctx, req_ctx, pad_len);

	SS_DBG("Method: %d, cnt: %d\n", req_ctx->type, ctx->cnt);

	memcpy(req->result, ctx->md, ctx->md_size);
	ss_dev_unlock();

	return 0;
}

int ss_hash_finup(struct ahash_request *req)
{
	ss_hash_update(req);
	return ss_hash_final(req);
}

void ss_load_iv(ss_aes_ctx_t *ctx, ss_aes_req_ctx_t *req_ctx, char *buf, int size)
{
	if (buf == NULL)
		return;

	/* Only AES/DES/3DES-ECB don't need IV. */
	if (CE_METHOD_IS_AES(req_ctx->type) && (req_ctx->mode == SS_AES_MODE_ECB))
		return;

	/* CBC/CTS need update the IV eachtime. */
	if ((ctx->cnt == 0)
		|| (CE_METHOD_IS_AES(req_ctx->type) && (req_ctx->mode == SS_AES_MODE_CBC))
		|| (CE_METHOD_IS_AES(req_ctx->type) && (req_ctx->mode == SS_AES_MODE_CTS))) {
		SS_DBG("IV address = %p, size = %d\n", buf, size);
		ctx->iv_size = size;
		memcpy(ctx->iv, buf, ctx->iv_size);
	}

	SS_DBG("The current IV: \n");
	print_hex(ctx->iv, ctx->iv_size, (int)ctx->iv);
}

static int ss_aes_one_req(sunxi_ss_t *sss, struct ablkcipher_request *req)
{
	int ret = 0;
	struct crypto_ablkcipher *tfm = NULL;
	ss_aes_ctx_t *ctx = NULL;
	ss_aes_req_ctx_t *req_ctx = NULL;

	SS_ENTER();
	if (!req->src || !req->dst) {
		SS_ERR("Invalid sg: src = %p, dst = %p\n", req->src, req->dst);
		return -EINVAL;
	}

	ss_dev_lock();

	tfm = crypto_ablkcipher_reqtfm(req);
	req_ctx = ablkcipher_request_ctx(req);
	ctx = crypto_ablkcipher_ctx(tfm);

	ss_load_iv(ctx, req_ctx, req->info, crypto_ablkcipher_ivsize(tfm));

	req_ctx->dma_src.sg = req->src;
	req_ctx->dma_dst.sg = req->dst;

	ret = ss_aes_start(ctx, req_ctx, req->nbytes);
	if (ret < 0)
		SS_ERR("ss_aes_start fail(%d)\n", ret);

	ss_dev_unlock();

#ifdef SS_CTR_MODE_ENABLE
	if (req_ctx->mode == SS_AES_MODE_CTR) {
		SS_DBG("CNT: %08x %08x %08x %08x \n", *(int *)&ctx->iv[0],
			*(int *)&ctx->iv[4], *(int *)&ctx->iv[8], *(int *)&ctx->iv[12]);
	}
#endif

	ctx->cnt += req->nbytes;
	if (req->base.complete)
		req->base.complete(&req->base, ret);

	return ret;
}

static int ss_hash_one_req(sunxi_ss_t *sss, struct ahash_request *req)
{
	int ret = 0;
	ss_aes_req_ctx_t *req_ctx = NULL;
	ss_hash_ctx_t *ctx = crypto_ahash_ctx(crypto_ahash_reqtfm(req));

	SS_ENTER();
	if (!req->src) {
		SS_ERR("Invalid sg: src = %p\n", req->src);
		return -EINVAL;
	}

	ss_dev_lock();

	req_ctx = ahash_request_ctx(req);
	req_ctx->dma_src.sg = req->src;
	
	ss_hash_padding_data_prepare(ctx, req->result, req->nbytes%ss_hash_blk_size(req_ctx->type));

	ret = ss_hash_start(ctx, req_ctx, req->nbytes);
	if (ret < 0)
		SS_ERR("ss_hash_start fail(%d)\n", ret);

	ss_dev_unlock();

	if (req->base.complete)
		req->base.complete(&req->base, ret);

	return ret;
}

void sunxi_ss_work(struct work_struct *work)
{
	int ret = 0;
    unsigned long flags = 0;
	sunxi_ss_t *sss = container_of(work, sunxi_ss_t, work);
	struct crypto_async_request *async_req = NULL;
	struct crypto_async_request *backlog = NULL;

	/* empty the crypto queue and then return */
	do {
		spin_lock_irqsave(&sss->lock, flags);
		backlog = crypto_get_backlog(&sss->queue);
		async_req = crypto_dequeue_request(&sss->queue);
		spin_unlock_irqrestore(&sss->lock, flags);

		if (!async_req) {
			SS_DBG("async_req is NULL! \n");
			break;
		}

		if (backlog)
			backlog->complete(backlog, -EINPROGRESS);

		SS_DBG("async_req->flags = %#x \n", async_req->flags);
		if (async_req->flags & SS_FLAG_AES)
			ret = ss_aes_one_req(sss, ablkcipher_request_cast(async_req));
		else if (async_req->flags & SS_FLAG_HASH)
			ret = ss_hash_one_req(sss, ahash_request_cast(async_req));
	} while (!ret);
}

irqreturn_t sunxi_ss_irq_handler(int irq, void *dev_id)
{
	int i;
	int pending = 0;
	sunxi_ss_t *sss = (sunxi_ss_t *)dev_id;
	unsigned long flags = 0;

	spin_lock_irqsave(&sss->lock, flags);

	pending = ss_pending_get();
	SS_DBG("pending: %#x \n", pending);
	for (i=0; i<SS_FLOW_NUM; i++) {
		if (pending & (CE_CHAN_PENDING<<i)) {
			SS_DBG("The chan %d is completed. (pending: %#x) \n", i, pending);
			ss_pending_clear(i);
			complete(&sss->flows[i].done);
		}
	}
	
	spin_unlock_irqrestore(&sss->lock, flags);
	return IRQ_HANDLED;
}

