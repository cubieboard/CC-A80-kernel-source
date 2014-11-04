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
#include <linux/interrupt.h>
#include <linux/clk.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <crypto/hash.h>
#include <crypto/md5.h>
#include <crypto/des.h>
#include <crypto/internal/hash.h>
#include <crypto/internal/rng.h>

#include <linux/dmaengine.h>
#include <linux/dma-mapping.h>
#include <linux/dma/sunxi-dma.h>
#include <linux/clk/sunxi_name.h>

#include "sunxi_ss.h"
#include "sunxi_ss_reg.h"

static u32 gs_debug_mask = 1;
module_param_named(debug_mask, gs_debug_mask, int, S_IRUGO|S_IWUSR);

static sunxi_ss_t *ss_dev = NULL;

static DEFINE_MUTEX(ss_lock);

static void print_hex(char *_data, int _len, int _addr)
{
	int i;

	if ((gs_debug_mask&DBG_INFO) == 0)
		return;

	printk("-------------------------------------------------------------- ");
	for (i=0; i<_len; i++) {
		if (i%16 == 0)
			printk("\n0x%08X: ", i + _addr);
		else if (i%8 == 0)
			printk("   ");

		printk("%02X ", _data[i]);
	}
	printk("\n");
	printk("-------------------------------------------------------------- \n");
}

/* Prepare for padding in Hash. Final() will process the data. */
static void ss_hash_padding_data_prepare(ss_hash_ctx_t *ctx, char *tail, int len)
{
	if (len%SHA1_BLOCK_SIZE != 0)
		memcpy(ctx->pad, tail, len%SHA1_BLOCK_SIZE);
}

/* The tail data will be processed later. */
static void ss_hash_padding_sg_prepare(struct scatterlist *last, int total)
{
	if (total%SHA1_BLOCK_SIZE != 0) {
		SS_DBG("sg len: %d, total: %d \n", sg_dma_len(last), total);
		WARN(sg_dma_len(last) < total%SHA1_BLOCK_SIZE, "sg len: %d, total: %d \n", sg_dma_len(last), total);
		sg_dma_len(last) = sg_dma_len(last) - total%SHA1_BLOCK_SIZE;
	}
	WARN_ON(sg_dma_len(last) > total);
}

static int ss_hash_padding(ss_hash_ctx_t *ctx, int big_endian)
{
	int n = ctx->cnt % 64;
	u8 *p = ctx->pad;
	int len_l = ctx->cnt << 3;  /* total len, in bits. */
	int len_h = ctx->cnt >> 29;

	p[n] = 0x80;
	n++;

	if (n > (SHA1_BLOCK_SIZE-8)) {
		memset(p+n, 0, SS_HASH_PAD_SIZE-n);
		p += SS_HASH_PAD_SIZE-8;
	}
	else {
		memset(p+n, 0, SHA1_BLOCK_SIZE-8-n);
		p += SHA1_BLOCK_SIZE-8;
	}

	if (big_endian == 1) {
		*(int *)p = swab32(len_h);
		*(int *)(p+4) = swab32(len_l);
	}
	else {
		*(int *)p = len_l;
		*(int *)(p+4) = len_h;
	}

	SS_DBG("After padding %d: %02x %02x %02x %02x   %02x %02x %02x %02x\n",
			p + 8 - ctx->pad,
			p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7]);

	return p + 8 - ctx->pad;
}

static int ss_sg_cnt(struct scatterlist *sg, int total)
{
	int cnt = 0;
	int nbyte = 0;
	struct scatterlist *cur = sg;

	while (cur != NULL) {
		cnt++;
		SS_DBG("cnt = %d, cur = %p, len = %d \n", cnt, cur, sg_dma_len(cur));
		nbyte += sg_dma_len(cur);
		if (nbyte >= total)
			return cnt;

		cur = sg_next(cur);
	}

	return cnt;
}

/* Callback of DMA completion. */
static void ss_dma_cb(void *data)
{
	ss_aes_req_ctx_t *req_ctx = (ss_aes_req_ctx_t *)data;

	SS_DBG("DMA transfer data complete!\n");

	complete(&req_ctx->done);
}

/* request dma channel and set callback function */
static int ss_dma_prepare(ss_dma_info_t *info)
{
	dma_cap_mask_t mask;

	/* Try to acquire a generic DMA engine slave channel */
	dma_cap_zero(mask);
#ifdef SS_IDMA_ENABLE
	dma_cap_set(DMA_SG, mask);
	dma_cap_set(DMA_MEMCPY, mask);
#else
	dma_cap_set(DMA_SLAVE, mask);
#endif

	info->chan = dma_request_channel(mask, NULL, NULL);
    if (info->chan == NULL) {
        SS_ERR("Request DMA() failed!\n");
        return -EINVAL;
    }
    return 0;
}

/* set dma start flag, if queue, it will auto restart to transfer next queue */
static void ss_dma_start(ss_dma_info_t *info)
{
	dma_async_issue_pending(info->chan);
}

static void ss_reset(void)
{
	SS_ENTER();
	clk_disable_unprepare(ss_dev->mclk);
	clk_prepare_enable(ss_dev->mclk);
}

#ifdef SS_IDMA_ENABLE

/* Make a sg_table based on sg[] of crypto request. */
static int ss_sg_table_init(struct sg_table *sgt, struct scatterlist *sg,
							int len, char *vbase, dma_addr_t pbase)
{
	int i;
	int npages = 0;
	int offset = 0;
	struct scatterlist *src_sg = sg;
	struct scatterlist *dst_sg = NULL;

	npages = ss_sg_cnt(sg, len);
	WARN_ON(npages == 0);

	if (sg_alloc_table(sgt, npages, GFP_KERNEL)) {
		SS_ERR("sg_alloc_table(%d) failed!\n", npages);
		WARN_ON(1);
	}

	dst_sg = sgt->sgl;
	for (i=0; i<npages; i++) {
		sg_set_buf(dst_sg, vbase + offset, sg_dma_len(src_sg));
		offset += sg_dma_len(src_sg);
		src_sg = sg_next(src_sg);
		dst_sg = sg_next(dst_sg);
	}
	return 0;
}

static int ss_dma_dst_config(sunxi_ss_t *sss, void *ctx, ss_aes_req_ctx_t *req_ctx, int len, int cb)
{
	int flow = ((ss_comm_ctx_t *)ctx)->flow;
	ss_dma_info_t *info = &req_ctx->dma_dst;
	struct dma_slave_config dma_conf = {0};
	struct dma_async_tx_descriptor *dma_desc = NULL;

	info->dir = DMA_MEM_TO_MEM;
	dma_conf.direction = info->dir;
#ifdef SS_CTR_MODE_ENABLE
	if (req_ctx->mode == SS_AES_MODE_CTR)
		dma_conf.src_addr_width = dma_conf.dst_addr_width = DMA_SLAVE_BUSWIDTH_1_BYTE;
	else
#endif
	dma_conf.src_addr_width = dma_conf.dst_addr_width = DMA_SLAVE_BUSWIDTH_4_BYTES;

	dma_conf.src_maxburst = 1;
	dma_conf.dst_maxburst = 1;
	dma_conf.slave_id = sunxi_slave_id(DRQDST_SDRAM, DRQSRC_SDRAM);
	dmaengine_slave_config(info->chan, &dma_conf);

	ss_sg_table_init(&info->sgt_for_cp, info->sg, len, sss->flows[flow].buf_dst, sss->flows[flow].buf_dst_dma);

	info->nents = info->sgt_for_cp.nents;
	SS_DBG("flow: %d, sg num: %d, total len: %d \n", flow, info->nents, len);

	dma_map_sg(&sss->pdev->dev, info->sg, info->nents, info->dir);
	dma_map_sg(&sss->pdev->dev, info->sgt_for_cp.sgl, info->nents, info->dir);

	dma_desc = info->chan->device->device_prep_dma_sg(info->chan,
			info->sg, info->nents,
			info->sgt_for_cp.sgl, info->nents,
			DMA_PREP_INTERRUPT | DMA_CTRL_ACK);
	if (!dma_desc) {
		SS_ERR("dmaengine_prep_slave_sg() failed!\n");
		return -1;
	}

	if (cb == 1) {
		dma_desc->callback = ss_dma_cb;
		dma_desc->callback_param = (void *)req_ctx;
	}
	dmaengine_submit(dma_desc);

	return 0;
}

static int ss_dma_src_config(sunxi_ss_t *sss, void *ctx, ss_aes_req_ctx_t *req_ctx, int len, int cb)
{
	int flow = ((ss_comm_ctx_t *)ctx)->flow;
	ss_dma_info_t *info = &req_ctx->dma_src;
	struct dma_slave_config dma_conf = {0};
	struct dma_async_tx_descriptor *dma_desc = NULL;

	info->dir = DMA_MEM_TO_MEM;
	dma_conf.direction = info->dir;
#ifdef SS_CTR_MODE_ENABLE
	if (req_ctx->mode == SS_AES_MODE_CTR)
		dma_conf.src_addr_width = dma_conf.dst_addr_width = DMA_SLAVE_BUSWIDTH_1_BYTE;
	else
#endif
	dma_conf.src_addr_width = dma_conf.dst_addr_width = DMA_SLAVE_BUSWIDTH_4_BYTES;

	dma_conf.src_maxburst = 1;
	dma_conf.dst_maxburst = 1;
	dma_conf.slave_id = sunxi_slave_id(DRQDST_SDRAM, DRQSRC_SDRAM);
	dmaengine_slave_config(info->chan, &dma_conf);

	ss_sg_table_init(&info->sgt_for_cp, info->sg, len, sss->flows[flow].buf_src, sss->flows[flow].buf_src_dma);
	SS_DBG("chan: 0x%p, info->sgt_for_cp.sgl: 0x%p \n", info->chan, info->sgt_for_cp.sgl);

	info->nents = info->sgt_for_cp.nents;
	SS_DBG("flow: %d, sg num: %d, total len: %d \n", flow, info->nents, len);

	dma_map_sg(&sss->pdev->dev, info->sg, info->nents, info->dir);
	dma_map_sg(&sss->pdev->dev, info->sgt_for_cp.sgl, info->nents, info->dir);

	if (SS_METHOD_IS_HASH(req_ctx->type)) {
		/* Total len is to small, so there is no data for DMA. */
		if (len < SHA1_BLOCK_SIZE)
			return 1;

		ss_hash_padding_sg_prepare(&info->sg[info->nents-1], len);
		ss_hash_padding_sg_prepare(&info->sgt_for_cp.sgl[info->nents-1], len);
	}

	dma_desc = info->chan->device->device_prep_dma_sg(info->chan,
			info->sgt_for_cp.sgl, info->nents,
			info->sg, info->nents, DMA_PREP_INTERRUPT | DMA_CTRL_ACK);
	if (!dma_desc) {
		SS_ERR("dmaengine_prep_slave_sg() failed!\n");
		return -1;
	}

	if (cb == 1) {
		dma_desc->callback = ss_dma_cb;
		dma_desc->callback_param = (void *)req_ctx;
	}
	dmaengine_submit(dma_desc);
	return 0;
}

static void ss_dma_release(sunxi_ss_t *sss, ss_dma_info_t *info)
{
	dma_unmap_sg(&sss->pdev->dev, info->sgt_for_cp.sgl, info->nents, info->dir);
	sg_free_table(&info->sgt_for_cp);
	dma_unmap_sg(&sss->pdev->dev, info->sg, info->nents, info->dir);
	dma_release_channel(info->chan);
}

#else

static int ss_dma_dst_config(sunxi_ss_t *sss, void *ctx, ss_aes_req_ctx_t *req_ctx, int len, int cb)
{
	int nents = 0;
	int npages = 0;
	ss_dma_info_t *info = &req_ctx->dma_dst;
	struct dma_slave_config dma_conf = {0};
	struct dma_async_tx_descriptor *dma_desc = NULL;

	info->dir = DMA_DEV_TO_MEM;
	dma_conf.direction = info->dir;
	dma_conf.src_addr = sss->base_addr_phy + SS_REG_TXFIFO;
	dma_conf.src_addr_width = DMA_SLAVE_BUSWIDTH_4_BYTES;
	dma_conf.dst_addr_width = DMA_SLAVE_BUSWIDTH_4_BYTES;
	dma_conf.src_maxburst = 1;
	dma_conf.dst_maxburst = 1;
	dma_conf.slave_id = sunxi_slave_id(DRQDST_SDRAM, DRQSRC_SS);
	dmaengine_slave_config(info->chan, &dma_conf);

	npages = ss_sg_cnt(info->sg, len);
	WARN_ON(npages == 0);

	nents = dma_map_sg(&sss->pdev->dev, info->sg, npages, info->dir);
	SS_DBG("npages = %d, nents = %d, len = %d, sg.len = %d \n", npages, nents, len, sg_dma_len(info->sg));
	if (!nents) {
		SS_ERR("dma_map_sg() error\n");
		return -EINVAL;
	}

	info->nents = nents;
	dma_desc = dmaengine_prep_slave_sg(info->chan, info->sg, nents,
				info->dir,	DMA_PREP_INTERRUPT | DMA_CTRL_ACK);
	if (!dma_desc) {
		SS_ERR("dmaengine_prep_slave_sg() failed!\n");
		return -1;
	}

	if (cb == 1) {
		dma_desc->callback = ss_dma_cb;
		dma_desc->callback_param = (void *)req_ctx;
	}
	dmaengine_submit(dma_desc);
	return 0;
}

/* ctx - only used for HASH. */
static int ss_dma_src_config(sunxi_ss_t *sss, void *ctx, ss_aes_req_ctx_t *req_ctx, int len, int cb)
{
	int nents = 0;
	int npages = 0;
	ss_dma_info_t *info = &req_ctx->dma_src;
	struct dma_slave_config dma_conf = {0};
	struct dma_async_tx_descriptor *dma_desc = NULL;

	info->dir = DMA_MEM_TO_DEV;
	dma_conf.direction = info->dir;
	dma_conf.dst_addr = sss->base_addr_phy + SS_REG_RXFIFO;
	dma_conf.src_addr_width = DMA_SLAVE_BUSWIDTH_4_BYTES;
	dma_conf.dst_addr_width = DMA_SLAVE_BUSWIDTH_4_BYTES;
	dma_conf.src_maxburst = 1;
	dma_conf.dst_maxburst = 1;
	dma_conf.slave_id = sunxi_slave_id(DRQDST_SS, DRQSRC_SDRAM);
	dmaengine_slave_config(info->chan, &dma_conf);

	npages = ss_sg_cnt(info->sg, len);
	WARN_ON(npages == 0);

	nents = dma_map_sg(&sss->pdev->dev, info->sg, npages, info->dir);
	SS_DBG("npages = %d, nents = %d, len = %d, sg.len = %d \n", npages, nents, len, sg_dma_len(info->sg));
	if (!nents) {
		SS_ERR("dma_map_sg() error\n");
		return -EINVAL;
	}

	info->nents = nents;

	if (SS_METHOD_IS_HASH(req_ctx->type)) {
		ss_hash_padding_sg_prepare(&info->sg[nents-1], len);

		/* Total len is too small, so there is no data for DMA. */
		if (len < SHA1_BLOCK_SIZE)
			return 1;
	}

	dma_desc = dmaengine_prep_slave_sg(info->chan, info->sg, nents,
				info->dir,	DMA_PREP_INTERRUPT | DMA_CTRL_ACK);
	if (!dma_desc) {
		SS_ERR("dmaengine_prep_slave_sg() failed!\n");
		return -1;
	}

	if (cb == 1) {
		dma_desc->callback = ss_dma_cb;
		dma_desc->callback_param = (void *)req_ctx;
	}
	dmaengine_submit(dma_desc);
	return 0;
}

/* release dma channel, and set queue status to idle. */
static void ss_dma_release(sunxi_ss_t *sss, ss_dma_info_t *info)
{
	dma_unmap_sg(&sss->pdev->dev, info->sg, info->nents, info->dir);
	dma_release_channel(info->chan);
}
#endif

static int ss_aes_start(ss_aes_ctx_t *ctx, ss_aes_req_ctx_t *req_ctx, int len)
{
	int ret = 0;
	int flow = ctx->comm.flow;

	ss_pending_clear(flow);
#ifdef SS_IDMA_ENABLE
	ss_irq_enable(flow);
	ss_flow_enable(flow);
#else
	ss_dma_enable(flow);
	ss_fifo_init();
#endif

	ss_method_set(req_ctx->dir, req_ctx->type);
	ss_aes_mode_set(req_ctx->mode);

	SS_DBG("Flow: %d, Dir: %d, Method: %d, Mode: %d, len: %d \n", flow, req_ctx->dir,
			req_ctx->type, req_ctx->mode, len);

	init_completion(&req_ctx->done);

#ifdef SS_IDMA_ENABLE
	/* 1. Copy data from user space to sss->flows[flow].buf_src. */
	if (ss_dma_prepare(&req_ctx->dma_src))
		return -EBUSY;
#ifdef SS_CTR_MODE_ENABLE
	if ((req_ctx->mode == SS_AES_MODE_CTR) && ((len%AES_BLOCK_SIZE) != 0))
		memset(&ss_dev->flows[flow].buf_src[len], 0, AES_BLOCK_SIZE);
#endif
	ss_dma_src_config(ss_dev, ctx, req_ctx, len, 1);
	ss_dma_start(&req_ctx->dma_src);
	ret = wait_for_completion_timeout(&req_ctx->done, msecs_to_jiffies(SS_WAIT_TIME));
	if (ret == 0) {
		SS_ERR("Timed out\n");
		return -ETIMEDOUT;
	}

	/* 2. Start the SS. */
	ss_data_src_set(ss_dev->flows[flow].buf_src_dma);
	ss_data_dst_set(ss_dev->flows[flow].buf_dst_dma);
	SS_DBG("ss_dev->buf_dst_dma = %#x\n", ss_dev->flows[flow].buf_dst_dma);
#ifdef SS_CTS_MODE_ENABLE
	if (req_ctx->mode == SS_AES_MODE_CTS) {
		ss_data_len_set(len);
		if (len < SZ_4K) /* A bad way to determin the last packet of CTS mode. */
			ss_cts_last();
	}
	else
#endif
	ss_data_len_set(DIV_ROUND_UP(len, AES_BLOCK_SIZE)*4);

	ss_ctrl_start();

#ifdef SS_RSA_ENABLE
	ret = wait_for_completion_timeout(&ss_dev->flows[flow].done, msecs_to_jiffies(SS_WAIT_TIME*50));
#else
	ret = wait_for_completion_timeout(&ss_dev->flows[flow].done, msecs_to_jiffies(SS_WAIT_TIME));
#endif
	if (ret == 0) {
		SS_ERR("Timed out\n");
		ss_reset();
		return -ETIMEDOUT;
	}

	/* 3. Copy the result from sss->flows[flow].buf_dst to user space. */
	if (ss_dma_prepare(&req_ctx->dma_dst))
		return -EBUSY;
	ss_dma_dst_config(ss_dev, ctx, req_ctx, len, 1);
	ss_dma_start(&req_ctx->dma_dst);
	ret = wait_for_completion_timeout(&req_ctx->done, msecs_to_jiffies(SS_WAIT_TIME));
	if (ret == 0) {
		SS_ERR("Timed out\n");
		return -ETIMEDOUT;
	}
#else /* else of "#ifdef SS_IDMA_ENABLE" */
	if (ss_dma_prepare(&req_ctx->dma_src))
		return -EBUSY;		
	ss_dma_prepare(&req_ctx->dma_dst);
	ss_dma_src_config(ss_dev, ctx, req_ctx, len, 0);
	ss_dma_dst_config(ss_dev, ctx, req_ctx, len, 1);

	ss_dma_start(&req_ctx->dma_dst);
	ss_ctrl_start();
	ss_dma_start(&req_ctx->dma_src);

	ret = wait_for_completion_timeout(&req_ctx->done, msecs_to_jiffies(SS_WAIT_TIME));
	if (ret == 0) {
		SS_ERR("Timed out\n");
		ss_reset();
		return -ETIMEDOUT;
	}
#endif /* end of "#ifdef SS_IDMA_ENABLE" */

	ss_ctrl_stop();
#ifdef SS_IDMA_ENABLE
	ss_irq_disable(flow);
#else
	ss_dma_disable(flow);
#endif
	ss_dma_release(ss_dev, &req_ctx->dma_src);
	ss_dma_release(ss_dev, &req_ctx->dma_dst);

	return 0;
}

static int ss_aes_key_valid(struct crypto_ablkcipher *tfm, int len)
{
#ifdef SS_RSA_ENABLE
	if (unlikely(len > SS_RSA_MAX_SIZE)) {
#else
	if (unlikely(len > AES_MAX_KEY_SIZE)) {
#endif
		SS_ERR("Unsupported key size: %d \n", len);
		tfm->base.crt_flags |= CRYPTO_TFM_RES_BAD_KEY_LEN;
		return -EINVAL;
	}
	return 0;
}

static int ss_aes_setkey(struct crypto_ablkcipher *tfm, const u8 *key, 
				unsigned int keylen)
{
	int ret = 0;
	ss_aes_ctx_t *ctx = crypto_ablkcipher_ctx(tfm);

	SS_DBG("keylen = %d\n", keylen);
	if (ctx->comm.flags & SS_FLAG_NEW_KEY) {
		SS_ERR("The key has already update.\n");
		return -EBUSY;
	}

	ret = ss_aes_key_valid(tfm, keylen);
	if (ret != 0)
		return ret;

	ctx->key_size = keylen;
	memcpy(ctx->key, key, keylen);
	if (keylen < AES_KEYSIZE_256)
		memset(&ctx->key[keylen], 0, AES_KEYSIZE_256 - keylen);

	ctx->comm.flags |= SS_FLAG_NEW_KEY;
	return 0;
}

static int ss_aes_crypt(struct ablkcipher_request *req, int dir, int method, int mode)
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

static int ss_aes_ecb_encrypt(struct ablkcipher_request *req)
{
	return ss_aes_crypt(req, SS_DIR_ENCRYPT, SS_METHOD_AES, SS_AES_MODE_ECB);
}

static int ss_aes_ecb_decrypt(struct ablkcipher_request *req)
{
	return ss_aes_crypt(req, SS_DIR_DECRYPT, SS_METHOD_AES, SS_AES_MODE_ECB);
}

static int ss_aes_cbc_encrypt(struct ablkcipher_request *req)
{
	return ss_aes_crypt(req, SS_DIR_ENCRYPT, SS_METHOD_AES, SS_AES_MODE_CBC);
}

static int ss_aes_cbc_decrypt(struct ablkcipher_request *req)
{
	return ss_aes_crypt(req, SS_DIR_DECRYPT, SS_METHOD_AES, SS_AES_MODE_CBC);
}

#ifdef SS_CTR_MODE_ENABLE
static int ss_aes_ctr_encrypt(struct ablkcipher_request *req)
{
	return ss_aes_crypt(req, SS_DIR_ENCRYPT, SS_METHOD_AES, SS_AES_MODE_CTR);
}

static int ss_aes_ctr_decrypt(struct ablkcipher_request *req)
{
	return ss_aes_crypt(req, SS_DIR_DECRYPT, SS_METHOD_AES, SS_AES_MODE_CTR);
}
#endif

#ifdef SS_CTS_MODE_ENABLE
static int ss_aes_cts_encrypt(struct ablkcipher_request *req)
{
	return ss_aes_crypt(req, SS_DIR_ENCRYPT, SS_METHOD_AES, SS_AES_MODE_CTS);
}

static int ss_aes_cts_decrypt(struct ablkcipher_request *req)
{
	return ss_aes_crypt(req, SS_DIR_DECRYPT, SS_METHOD_AES, SS_AES_MODE_CTS);
}
#endif

static int ss_des_ecb_encrypt(struct ablkcipher_request *req)
{
	return ss_aes_crypt(req, SS_DIR_ENCRYPT, SS_METHOD_DES, SS_AES_MODE_ECB);
}

static int ss_des_ecb_decrypt(struct ablkcipher_request *req)
{
	return ss_aes_crypt(req, SS_DIR_DECRYPT, SS_METHOD_DES, SS_AES_MODE_ECB);
}

static int ss_des_cbc_encrypt(struct ablkcipher_request *req)
{
	return ss_aes_crypt(req, SS_DIR_ENCRYPT, SS_METHOD_DES, SS_AES_MODE_CBC);
}

static int ss_des_cbc_decrypt(struct ablkcipher_request *req)
{
	return ss_aes_crypt(req, SS_DIR_DECRYPT, SS_METHOD_DES, SS_AES_MODE_CBC);
}

static int ss_des3_ecb_encrypt(struct ablkcipher_request *req)
{
	return ss_aes_crypt(req, SS_DIR_ENCRYPT, SS_METHOD_3DES, SS_AES_MODE_ECB);
}

static int ss_des3_ecb_decrypt(struct ablkcipher_request *req)
{
	return ss_aes_crypt(req, SS_DIR_DECRYPT, SS_METHOD_3DES, SS_AES_MODE_ECB);
}

static int ss_des3_cbc_encrypt(struct ablkcipher_request *req)
{
	return ss_aes_crypt(req, SS_DIR_ENCRYPT, SS_METHOD_3DES, SS_AES_MODE_CBC);
}

static int ss_des3_cbc_decrypt(struct ablkcipher_request *req)
{
	return ss_aes_crypt(req, SS_DIR_DECRYPT, SS_METHOD_3DES, SS_AES_MODE_CBC);
}

#ifdef SS_RSA_ENABLE
static int ss_rsa_encrypt(struct ablkcipher_request *req)
{
	return ss_aes_crypt(req, SS_DIR_ENCRYPT, SS_METHOD_RSA, SS_AES_MODE_CBC);
}

static int ss_rsa_decrypt(struct ablkcipher_request *req)
{
	return ss_aes_crypt(req, SS_DIR_DECRYPT, SS_METHOD_RSA, SS_AES_MODE_CBC);
}
#endif

#ifdef SS_PRNG_ENABLE
static int ss_rng_start(ss_aes_ctx_t *ctx, u8 *rdata, unsigned int dlen)
{
	int ret = 0;
	int flow = ctx->comm.flow;

	ss_pending_clear(flow);
#ifndef SS_IDMA_ENABLE
	ss_dma_disable(flow);
#else
	ss_irq_enable(flow);
	ss_flow_enable(flow);
#endif

#ifdef SS_TRNG_ENABLE
	if (ctx->comm.flags & SS_FLAG_TRNG) {
		ss_method_set(SS_DIR_ENCRYPT, SS_METHOD_TRNG);
		ss_trng_osc_enable();
	}
	else
#endif
		ss_method_set(SS_DIR_ENCRYPT, SS_METHOD_PRNG);

	ss_rng_mode_set(SS_RNG_MODE_CONTINUE);

#ifdef SS_IDMA_ENABLE
	ss_data_dst_set(ss_dev->flows[flow].buf_dst_dma);
#ifdef SS_TRNG_ENABLE
	ss_data_len_set(DIV_ROUND_UP(dlen, 32)*(32>>2)); /* align with 32 Bytes */
#else
	ss_data_len_set(DIV_ROUND_UP(dlen, 20)*(20>>2)); /* align with 20 Bytes */
#endif
	SS_DBG("Flow: %d, Request: %d, Aligned: %d \n", flow, dlen, DIV_ROUND_UP(dlen, 20)*5);
	dma_map_single(&ss_dev->pdev->dev, ss_dev->flows[flow].buf_dst, SS_DMA_BUF_SIZE, DMA_DEV_TO_MEM);

	ss_ctrl_start();
	ret = wait_for_completion_timeout(&ss_dev->flows[flow].done, msecs_to_jiffies(SS_WAIT_TIME));
	if (ret == 0) {
		SS_ERR("Timed out\n");
		ss_reset();
		return -ETIMEDOUT;
	}

	memcpy(rdata, ss_dev->flows[flow].buf_dst, dlen);
	dma_unmap_single(&ss_dev->pdev->dev, ss_dev->flows[flow].buf_dst_dma, SS_DMA_BUF_SIZE, DMA_DEV_TO_MEM);
	ss_irq_disable(flow);
	ret = dlen;
#else
	ss_ctrl_start();
	ret = ss_random_rd(rdata, dlen);
#endif

#ifdef SS_TRNG_ENABLE
	if (ctx->comm.flags & SS_FLAG_TRNG)
		ss_trng_osc_disable();
#endif

	ss_ctrl_stop();
	return ret;
}

static int ss_rng_get_random(struct crypto_rng *tfm, u8 *rdata, unsigned int dlen)
{
	int ret = 0;
	ss_aes_ctx_t *ctx = crypto_rng_ctx(tfm);

	SS_DBG("flow = %d, rdata = %p, len = %d \n", ctx->comm.flow, rdata, dlen);
	if (ss_dev->suspend) {
		SS_ERR("SS has already suspend. \n");
		return -EAGAIN;
	}

	mutex_lock(&ss_lock);

	/* Must set the seed addr in PRNG/TRNG. */
	ss_key_set(ctx->key, ctx->key_size);
#ifdef SS_IDMA_ENABLE
	dma_map_single(&ss_dev->pdev->dev, ctx->key, ctx->key_size, DMA_MEM_TO_DEV);
#endif

	ret = ss_rng_start(ctx, rdata, dlen);
	mutex_unlock(&ss_lock);

	SS_DBG("Get %d byte random. \n", ret);

#ifdef SS_IDMA_ENABLE
	dma_unmap_single(&ss_dev->pdev->dev, virt_to_phys(ctx->key), ctx->key_size, DMA_MEM_TO_DEV);
#endif

	return ret;
}

#ifdef SS_TRNG_ENABLE
static int ss_trng_get_random(struct crypto_rng *tfm, u8 *rdata, unsigned int dlen)
{
	ss_aes_ctx_t *ctx = crypto_rng_ctx(tfm);

	ctx->comm.flags |= SS_FLAG_TRNG;
	return ss_rng_get_random(tfm, rdata, dlen);
}
#endif

static int ss_rng_reset(struct crypto_rng *tfm, u8 *seed, unsigned int slen)
{
	ss_aes_ctx_t *ctx = crypto_rng_ctx(tfm);

	SS_DBG("Seed len: %d, ctx->flags = %#x \n", slen, ctx->comm.flags);
	ctx->key_size = slen;
	memcpy(ctx->key, seed, slen);
	ctx->comm.flags |= SS_FLAG_NEW_KEY;

	return 0;
}
#endif

static int ss_flow_request(ss_comm_ctx_t *comm)
{
	int i;
	unsigned long flags = 0;

	spin_lock_irqsave(&ss_dev->lock, flags);
	for (i=0; i<SS_FLOW_NUM; i++) {
		if (ss_dev->flows[i].available == SS_FLOW_AVAILABLE) {
			comm->flow = i;
			ss_dev->flows[i].available = SS_FLOW_UNAVAILABLE;
			SS_DBG("The flow %d is available. \n", i);
			break;
		}
	}
	spin_unlock_irqrestore(&ss_dev->lock, flags);

	if (i == SS_FLOW_NUM) {
		SS_ERR("Failed to get an available flow. \n");
		i = -1;
	}
	return i;
}

static void ss_flow_release(ss_comm_ctx_t *comm)
{
	unsigned long flags = 0;

	spin_lock_irqsave(&ss_dev->lock, flags);
	ss_dev->flows[comm->flow].available = SS_FLOW_AVAILABLE;
	spin_unlock_irqrestore(&ss_dev->lock, flags);
}

static int sunxi_ss_cra_init(struct crypto_tfm *tfm)
{
	if (ss_flow_request(crypto_tfm_ctx(tfm)) < 0)
		return -1;

	tfm->crt_ablkcipher.reqsize = sizeof(ss_aes_req_ctx_t);
	SS_DBG("reqsize = %d \n", tfm->crt_u.ablkcipher.reqsize);
	return 0;
}

static int sunxi_ss_cra_rng_init(struct crypto_tfm *tfm)
{
	if (ss_flow_request(crypto_tfm_ctx(tfm)) < 0)
		return -1;

	return 0;
}

static int sunxi_ss_cra_hash_init(struct crypto_tfm *tfm)
{
	if (ss_flow_request(crypto_tfm_ctx(tfm)) < 0)
		return -1;

	crypto_ahash_set_reqsize(__crypto_ahash_cast(tfm),
				 sizeof(ss_aes_req_ctx_t));

	SS_DBG("reqsize = %d \n", sizeof(ss_aes_req_ctx_t));
	return 0;
}

static void sunxi_ss_cra_exit(struct crypto_tfm *tfm)
{
	SS_ENTER();
	ss_flow_release(crypto_tfm_ctx(tfm));
}

#if defined(SS_SHA_SWAP_PRE_ENABLE) || defined(SS_SHA_SWAP_FINAL_ENABLE)
/* A bug of SS controller, need fix it by software. */
static void ss_hash_swap(char *data, int len)
{
	int i;
	int temp = 0;
	int *cur = (int *)data;

	SS_DBG("Convert the byter-order of digest. len %d\n", len);
	for (i=0; i<len/4; i++, cur++) {
		temp = cpu_to_be32(*cur);
		*cur = temp;
	}
}
#endif

static int ss_hash_start(ss_hash_ctx_t *ctx, ss_aes_req_ctx_t *req_ctx, int len)
{
	int ret = 0;
	int flow = ctx->comm.flow;
#ifdef SS_IDMA_ENABLE
	int md_map_flag = 0;
#endif

	ss_pending_clear(flow);
#ifdef SS_MULTI_FLOW_ENABLE
	ss_irq_enable(flow);
	ss_flow_enable(flow);
#else
	ss_dma_enable(flow);
	ss_fifo_init();
#endif

	ss_method_set(req_ctx->dir, req_ctx->type);

	SS_DBG("Flow: %d, Dir: %d, Method: %d, Mode: %d, len: %d / %d \n", flow,
			req_ctx->dir, req_ctx->type, req_ctx->mode, len, ctx->cnt);

	SS_DBG("IV address = 0x%p, size = %d\n", ctx->md, ctx->md_size);
	ss_iv_set(ctx->md, ctx->md_size);
	ss_iv_mode_set(SS_IV_MODE_ARBITRARY);

	init_completion(&req_ctx->done);

	if (ss_dma_prepare(&req_ctx->dma_src))
		return -EBUSY;

	ret = ss_dma_src_config(ss_dev, ctx, req_ctx, len, 1);
	if (ret == 0) {
#ifdef SS_IDMA_ENABLE
		/* 1. Copy data from user space to sss->flows[flow].buf_src. */
		ss_dma_start(&req_ctx->dma_src);
		ret = wait_for_completion_timeout(&req_ctx->done, msecs_to_jiffies(SS_WAIT_TIME));
		if (ret == 0) {
			SS_ERR("Timed out\n");
			return -ETIMEDOUT;
		}

		/* 2. Start the SS. */
		ss_data_src_set(ss_dev->flows[flow].buf_src_dma);
		ss_data_dst_set(ss_dev->flows[flow].buf_dst_dma);
		SS_DBG("ss_dev->buf_dst_dma = %#x\n", ss_dev->flows[flow].buf_dst_dma);
		ss_data_len_set((len - len%SHA1_BLOCK_SIZE)/4);

#ifdef SS_SHA_SWAP_MID_ENABLE
		if (req_ctx->type != SS_METHOD_MD5)
			ss_hash_swap(ctx->md, ctx->md_size);
#endif

		dma_map_single(&ss_dev->pdev->dev, ctx->md, ctx->md_size, DMA_MEM_TO_DEV);
		md_map_flag = 1;

		SS_DBG("Before SS, CTRL: 0x%08x \n", ss_reg_rd(SS_REG_CTL));
		dma_map_single(&ss_dev->pdev->dev, ss_dev->flows[flow].buf_dst, ctx->md_size, DMA_DEV_TO_MEM);
		ss_ctrl_start();

		ret = wait_for_completion_timeout(&ss_dev->flows[flow].done, msecs_to_jiffies(SS_WAIT_TIME));
		if (ret == 0) {
			SS_ERR("Timed out\n");
			ss_reset();
			return -ETIMEDOUT;
		}
		SS_DBG("After SS, CTRL: 0x%08x \n", ss_reg_rd(SS_REG_CTL));
		SS_DBG("After SS, dst data: \n");
		print_hex(ss_dev->flows[flow].buf_dst, 32, (int)ss_dev->flows[flow].buf_dst);

		/* 3. Copy the MD from sss->buf_dst to ctx->md. */
		memcpy(ctx->md, ss_dev->flows[flow].buf_dst, ctx->md_size);
#else
		ss_ctrl_start();
		ss_dma_start(&req_ctx->dma_src);

		ret = wait_for_completion_timeout(&req_ctx->done, msecs_to_jiffies(SS_WAIT_TIME));
		if (ret == 0) {
			SS_ERR("Timed out\n");
			ss_reset();
			return -ETIMEDOUT;
		}

		ss_md_get(ctx->md, ctx->md_size);
#endif
	}

#ifdef SS_IDMA_ENABLE
	ss_ctrl_stop();
	ss_irq_disable(flow);
	if (md_map_flag == 1) {
		dma_unmap_single(&ss_dev->pdev->dev, ss_dev->flows[flow].buf_dst_dma, ctx->md_size, DMA_DEV_TO_MEM);
		dma_unmap_single(&ss_dev->pdev->dev, virt_to_phys(ctx->md), ctx->md_size, DMA_MEM_TO_DEV);
	}
#else
	ss_dma_disable(flow);
#endif
	ss_dma_release(ss_dev, &req_ctx->dma_src);

	ctx->cnt += len;
	return 0;
}

static int ss_hash_init(struct ahash_request *req, int type, int size, char *iv)
{
	ss_aes_req_ctx_t *req_ctx = ahash_request_ctx(req);
	ss_hash_ctx_t *ctx = crypto_ahash_ctx(crypto_ahash_reqtfm(req));

	SS_DBG("Method: %d \n", type);

	memset(req_ctx, 0, sizeof(ss_aes_req_ctx_t));
	req_ctx->type = type;

	ctx->md_size = size;
	memcpy(ctx->md, iv, size);

	ctx->cnt = 0;
	memset(ctx->pad, 0, SS_HASH_PAD_SIZE);
	return 0;
}

static int ss_md5_init(struct ahash_request *req)
{
	char iv[MD5_DIGEST_SIZE] = {
			0x01, 0x23, 0x45, 0x67,	  0x89, 0xAB, 0xCD, 0xEF,
			0xFE, 0xDC, 0xBA, 0x98,	  0x76, 0x54, 0x32, 0x10};

	return ss_hash_init(req, SS_METHOD_MD5, MD5_DIGEST_SIZE, iv);
}

static int ss_sha1_init(struct ahash_request *req)
{
	char iv[SHA1_DIGEST_SIZE] = {
			0x01, 0x23, 0x45, 0x67,	  0x89, 0xAB, 0xCD, 0xEF,
			0xFE, 0xDC, 0xBA, 0x98,	  0x76, 0x54, 0x32, 0x10,
			0xF0, 0xE1, 0xD2, 0xC3};

#ifdef SS_SHA_SWAP_PRE_ENABLE
#ifdef SS_SHA_NO_SWAP_IV4
	ss_hash_swap(iv, SHA1_DIGEST_SIZE - 4);
#else
	ss_hash_swap(iv, SHA1_DIGEST_SIZE);
#endif
#endif

	return ss_hash_init(req, SS_METHOD_SHA1, SHA1_DIGEST_SIZE, iv);
}

#ifdef SS_SHA224_ENABLE
static int ss_sha224_init(struct ahash_request *req)
{
	char iv[SHA256_DIGEST_SIZE] = {
			0xD8, 0x9E, 0x05, 0xC1,	  0x07, 0xD5, 0x7C, 0x36,
			0x17, 0xDD, 0x70, 0x30,	  0x39, 0x59, 0x0E, 0xF7,
			0x31, 0x0B, 0xC0, 0xFF,   0x11, 0x15, 0x58, 0x68,
			0xA7, 0x8F, 0xF9, 0x64,   0xA4, 0x4F, 0xFA, 0xBE};

#ifdef SS_SHA_SWAP_PRE_ENABLE
	ss_hash_swap(iv, SHA256_DIGEST_SIZE);
#endif

	return ss_hash_init(req, SS_METHOD_SHA224, SHA256_DIGEST_SIZE, iv);
}
#endif

#ifdef SS_SHA256_ENABLE
static int ss_sha256_init(struct ahash_request *req)
{
	char iv[SHA256_DIGEST_SIZE] = {
			0x67, 0xE6, 0x09, 0x6A,	  0x85, 0xAE, 0x67, 0xBB,
			0x72, 0xF3, 0x6E, 0x3C,	  0x3A, 0xF5, 0x4F, 0xA5,
			0x7F, 0x52, 0x0E, 0x51,   0x8C, 0x68, 0x05, 0x9B,
			0xAB, 0xD9, 0x83, 0x1F,   0x19, 0xCD, 0xE0, 0x5B};

#ifdef SS_SHA_SWAP_PRE_ENABLE
	ss_hash_swap(iv, SHA256_DIGEST_SIZE);
#endif

	return ss_hash_init(req, SS_METHOD_SHA256, SHA256_DIGEST_SIZE, iv);
}
#endif

static int ss_hash_update(struct ahash_request *req)
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

static int ss_hash_final(struct ahash_request *req)
{
	int pad_len = 0;
	ss_aes_req_ctx_t *req_ctx = ahash_request_ctx(req);
	ss_hash_ctx_t *ctx = crypto_ahash_ctx(crypto_ahash_reqtfm(req));
	struct scatterlist last = {0}; /* make a sg struct for padding data. */

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
	pad_len = ss_hash_padding(ctx, req_ctx->type == SS_METHOD_MD5 ? 0 : 1);
	SS_DBG("Pad len: %d \n", pad_len);
	req_ctx->dma_src.sg = &last;
	sg_init_table(&last, 1);
	sg_set_buf(&last, ctx->pad, pad_len);
	SS_DBG("Padding data: \n");
	print_hex(ctx->pad, 128, (int)ctx->pad);

	mutex_lock(&ss_lock);
	ss_hash_start(ctx, req_ctx, pad_len);

	ss_sha_final();

	SS_DBG("Method: %d, cnt: %d\n", req_ctx->type, ctx->cnt);

	ss_check_sha_end();
#ifdef SS_IDMA_ENABLE
	memcpy(req->result, ctx->md, ctx->md_size);
#else
	ss_md_get(req->result, ctx->md_size);
#endif
	ss_ctrl_stop();
	mutex_unlock(&ss_lock);

#ifdef SS_SHA_SWAP_FINAL_ENABLE
	if (req_ctx->type != SS_METHOD_MD5)
		ss_hash_swap(req->result, ctx->md_size);
#endif

	return 0;
}

static int ss_hash_finup(struct ahash_request *req)
{
	ss_hash_update(req);
	return ss_hash_final(req);
}

static struct crypto_alg sunxi_ss_algs[] = {
	{
		.cra_name		 = "ecb(aes)",
		.cra_driver_name = "ss-ecb-aes",
		.cra_flags	  	 = CRYPTO_ALG_TYPE_ABLKCIPHER | CRYPTO_ALG_ASYNC,
		.cra_type 		 = &crypto_ablkcipher_type,
		.cra_blocksize	 = AES_BLOCK_SIZE,
		.cra_alignmask	 = 3,
		.cra_u.ablkcipher = {
			.setkey		 = ss_aes_setkey,
			.encrypt	 = ss_aes_ecb_encrypt,
			.decrypt	 = ss_aes_ecb_decrypt,
			.min_keysize = AES_MIN_KEY_SIZE,
			.max_keysize = AES_MAX_KEY_SIZE,
		}
	},
	{
		.cra_name		 = "cbc(aes)",
		.cra_driver_name = "ss-cbc-aes",
		.cra_flags	  	 = CRYPTO_ALG_TYPE_ABLKCIPHER | CRYPTO_ALG_ASYNC,
		.cra_type 		 = &crypto_ablkcipher_type,
		.cra_blocksize	 = AES_BLOCK_SIZE,
		.cra_alignmask	 = 3,
		.cra_u.ablkcipher = {
			.setkey		 = ss_aes_setkey,
			.encrypt	 = ss_aes_cbc_encrypt,
			.decrypt	 = ss_aes_cbc_decrypt,
			.min_keysize = AES_MIN_KEY_SIZE,
			.max_keysize = AES_MAX_KEY_SIZE,
			.ivsize		 = AES_MIN_KEY_SIZE,
		}
	},
#ifdef SS_CTR_MODE_ENABLE
	{
		.cra_name		 = "ctr(aes)",
		.cra_driver_name = "ss-ctr-aes",
		.cra_flags	 	 = CRYPTO_ALG_TYPE_ABLKCIPHER | CRYPTO_ALG_ASYNC,
		.cra_type 		 = &crypto_ablkcipher_type,
		.cra_blocksize	 = 1,
		.cra_alignmask	 = 3,
		.cra_u.ablkcipher = {
			.setkey		 = ss_aes_setkey,
			.encrypt	 = ss_aes_ctr_encrypt,
			.decrypt	 = ss_aes_ctr_decrypt,
			.min_keysize = AES_MIN_KEY_SIZE,
			.max_keysize = AES_MAX_KEY_SIZE,
			.ivsize		 = AES_MIN_KEY_SIZE,
		},
	},
#endif
#ifdef SS_CTS_MODE_ENABLE
	{
		.cra_name		 = "cts(aes)",
		.cra_driver_name = "ss-cts-aes",
		.cra_flags	 	 = CRYPTO_ALG_TYPE_ABLKCIPHER | CRYPTO_ALG_ASYNC,
		.cra_type 		 = &crypto_ablkcipher_type,
		.cra_blocksize	 = 1,
		.cra_alignmask	 = 1,
		.cra_u.ablkcipher = {
			.setkey		 = ss_aes_setkey,
			.encrypt	 = ss_aes_cts_encrypt,
			.decrypt	 = ss_aes_cts_decrypt,
			.min_keysize = AES_MIN_KEY_SIZE,
			.max_keysize = AES_MAX_KEY_SIZE,
			.ivsize		 = AES_MIN_KEY_SIZE,
		},
	},
#endif
	{
		.cra_name		 = "ecb(des)",
		.cra_driver_name = "ss-ecb-des",
		.cra_flags	  	 = CRYPTO_ALG_TYPE_ABLKCIPHER | CRYPTO_ALG_ASYNC,
		.cra_type 		 = &crypto_ablkcipher_type,
		.cra_blocksize	 = DES_BLOCK_SIZE,
		.cra_alignmask	 = 3,
		.cra_u.ablkcipher = {
			.setkey		 = ss_aes_setkey,
			.encrypt	 = ss_des_ecb_encrypt,
			.decrypt	 = ss_des_ecb_decrypt,
			.min_keysize = DES_KEY_SIZE,
			.max_keysize = DES_KEY_SIZE,
		}
	},
	{
		.cra_name		 = "cbc(des)",
		.cra_driver_name = "ss-cbc-des",
		.cra_flags	  	 = CRYPTO_ALG_TYPE_ABLKCIPHER | CRYPTO_ALG_ASYNC,
		.cra_type 		 = &crypto_ablkcipher_type,
		.cra_blocksize	 = DES_BLOCK_SIZE,
		.cra_alignmask	 = 3,
		.cra_u.ablkcipher = {
			.setkey		 = ss_aes_setkey,
			.encrypt	 = ss_des_cbc_encrypt,
			.decrypt	 = ss_des_cbc_decrypt,
			.min_keysize = DES_KEY_SIZE,
			.max_keysize = DES_KEY_SIZE,
			.ivsize		 = DES_KEY_SIZE,
		}
	},
	{
		.cra_name		 = "ecb(des3)",
		.cra_driver_name = "ss-ecb-3des",
		.cra_flags	  	 = CRYPTO_ALG_TYPE_ABLKCIPHER | CRYPTO_ALG_ASYNC,
		.cra_type 		 = &crypto_ablkcipher_type,
		.cra_blocksize	 = DES3_EDE_BLOCK_SIZE,
		.cra_alignmask	 = 3,
		.cra_u.ablkcipher = {
			.setkey		 = ss_aes_setkey,
			.encrypt	 = ss_des3_ecb_encrypt,
			.decrypt	 = ss_des3_ecb_decrypt,
			.min_keysize = DES3_EDE_KEY_SIZE,
			.max_keysize = DES3_EDE_KEY_SIZE,
		}
	},
	{
		.cra_name		 = "cbc(des3)",
		.cra_driver_name = "ss-cbc-3des",
		.cra_flags	  	 = CRYPTO_ALG_TYPE_ABLKCIPHER | CRYPTO_ALG_ASYNC,
		.cra_type 		 = &crypto_ablkcipher_type,
		.cra_blocksize	 = DES3_EDE_BLOCK_SIZE,
		.cra_alignmask	 = 3,
		.cra_u.ablkcipher = {
			.setkey		 = ss_aes_setkey,
			.encrypt	 = ss_des3_cbc_encrypt,
			.decrypt	 = ss_des3_cbc_decrypt,
			.min_keysize = DES3_EDE_KEY_SIZE,
			.max_keysize = DES3_EDE_KEY_SIZE,
			.ivsize		 = DES_KEY_SIZE,
		}
	},
#ifdef SS_RSA512_ENABLE
	{
		.cra_name		 = "rsa(512)",
		.cra_driver_name = "ss-rsa-512",
		.cra_flags		 = CRYPTO_ALG_TYPE_ABLKCIPHER | CRYPTO_ALG_ASYNC,
		.cra_type		 = &crypto_ablkcipher_type,
		.cra_blocksize	 = AES_BLOCK_SIZE,
		.cra_alignmask	 = 31,
		.cra_u.ablkcipher = {
			.setkey 	 = ss_aes_setkey,
			.encrypt	 = ss_rsa_encrypt,
			.decrypt	 = ss_rsa_decrypt,
			.min_keysize = SS_RSA_MIN_SIZE, /* RSA 512bits */
			.max_keysize = SS_RSA_MAX_SIZE, /* RSA 3072bits */
			.ivsize 	 = SS_RSA_MIN_SIZE,
		},
	},
#endif
#ifdef SS_RSA1024_ENABLE
	{
		.cra_name		 = "rsa(1024)",
		.cra_driver_name = "ss-rsa-1024",
		.cra_flags		 = CRYPTO_ALG_TYPE_ABLKCIPHER | CRYPTO_ALG_ASYNC,
		.cra_type		 = &crypto_ablkcipher_type,
		.cra_blocksize	 = AES_BLOCK_SIZE,
		.cra_alignmask	 = 31,
		.cra_u.ablkcipher = {
			.setkey 	 = ss_aes_setkey,
			.encrypt	 = ss_rsa_encrypt,
			.decrypt	 = ss_rsa_decrypt,
			.min_keysize = SS_RSA_MIN_SIZE, /* RSA 512bits */
			.max_keysize = SS_RSA_MAX_SIZE, /* RSA 3072bits */
			.ivsize 	 = SS_RSA_MIN_SIZE*2,
		},
	},
#endif
#ifdef SS_RSA2048_ENABLE
	{
		.cra_name		 = "rsa(2048)",
		.cra_driver_name = "ss-rsa-2048",
		.cra_flags		 = CRYPTO_ALG_TYPE_ABLKCIPHER | CRYPTO_ALG_ASYNC,
		.cra_type		 = &crypto_ablkcipher_type,
		.cra_blocksize	 = AES_BLOCK_SIZE,
		.cra_alignmask	 = 31,
		.cra_u.ablkcipher = {
			.setkey 	 = ss_aes_setkey,
			.encrypt	 = ss_rsa_encrypt,
			.decrypt	 = ss_rsa_decrypt,
			.min_keysize = SS_RSA_MIN_SIZE, /* RSA 512bits */
			.max_keysize = SS_RSA_MAX_SIZE, /* RSA 3072bits */
			.ivsize 	 = SS_RSA_MIN_SIZE*4,
		},
	},
#endif
#ifdef SS_RSA3072_ENABLE
	{
		.cra_name		 = "rsa(3072)",
		.cra_driver_name = "ss-rsa-3072",
		.cra_flags		 = CRYPTO_ALG_TYPE_ABLKCIPHER | CRYPTO_ALG_ASYNC,
		.cra_type		 = &crypto_ablkcipher_type,
		.cra_blocksize	 = AES_BLOCK_SIZE,
		.cra_alignmask	 = 31,
		.cra_u.ablkcipher = {
			.setkey 	 = ss_aes_setkey,
			.encrypt	 = ss_rsa_encrypt,
			.decrypt	 = ss_rsa_decrypt,
			.min_keysize = SS_RSA_MIN_SIZE, /* RSA 512bits */
			.max_keysize = SS_RSA_MAX_SIZE, /* RSA 3072bits */
			.ivsize 	 = SS_RSA_MIN_SIZE*6,
		},
	},
#endif
#ifndef SS_TRNG_ENABLE
	{
		.cra_name = "prng",
		.cra_driver_name = "ss-prng",
		.cra_flags = CRYPTO_ALG_TYPE_RNG,
		.cra_type = &crypto_rng_type,
		.cra_u.rng = {
			.rng_make_random = ss_rng_get_random,
			.rng_reset = ss_rng_reset,
			.seedsize = SS_SEED_SIZE,
		}
	},
#else
	{
		.cra_name = "prng",
		.cra_driver_name = "ss-trng",
		.cra_flags = CRYPTO_ALG_TYPE_RNG,
		.cra_type = &crypto_rng_type,
		.cra_u.rng = {
			.rng_make_random = ss_trng_get_random,
			.rng_reset = ss_rng_reset,
			.seedsize = SS_SEED_SIZE,
		}
	}
#endif
};

static struct ahash_alg sunxi_ss_algs_hash[] = {
	{
		.init		= ss_md5_init,
		.update		= ss_hash_update,
		.final		= ss_hash_final,
		.finup		= ss_hash_finup,
		.halg.digestsize	= MD5_DIGEST_SIZE,
		.halg.base	= {
			.cra_name		= "md5",
			.cra_driver_name= "ss-md5",
			.cra_flags		= CRYPTO_ALG_TYPE_AHASH|CRYPTO_ALG_ASYNC,
			.cra_blocksize	= SHA1_BLOCK_SIZE,
			.cra_ctxsize	= sizeof(ss_hash_ctx_t),
			.cra_alignmask	= 3,
			.cra_module		= THIS_MODULE,
			.cra_init		= sunxi_ss_cra_hash_init,
			.cra_exit		= sunxi_ss_cra_exit,
		}
	},
	{
		.init		= ss_sha1_init,
		.update		= ss_hash_update,
		.final		= ss_hash_final,
		.finup		= ss_hash_finup,
		.halg.digestsize	= SHA1_DIGEST_SIZE,
		.halg.base	= {
			.cra_name		= "sha1",
			.cra_driver_name= "ss-sha1",
			.cra_flags		= CRYPTO_ALG_TYPE_AHASH|CRYPTO_ALG_ASYNC,
			.cra_blocksize	= SHA1_BLOCK_SIZE,
			.cra_ctxsize	= sizeof(ss_hash_ctx_t),
			.cra_alignmask	= 3,
			.cra_module		= THIS_MODULE,
			.cra_init		= sunxi_ss_cra_hash_init,
			.cra_exit		= sunxi_ss_cra_exit,
		}
	},
#ifdef SS_SHA224_ENABLE
	{
		.init		= ss_sha224_init,
		.update		= ss_hash_update,
		.final		= ss_hash_final,
		.finup		= ss_hash_finup,
		.halg.digestsize	= SHA256_DIGEST_SIZE,
		.halg.base	= {
			.cra_name		= "sha224",
			.cra_driver_name= "ss-sha224",
			.cra_flags		= CRYPTO_ALG_TYPE_AHASH|CRYPTO_ALG_ASYNC,
			.cra_blocksize	= SHA256_BLOCK_SIZE,
			.cra_ctxsize	= sizeof(ss_hash_ctx_t),
			.cra_alignmask	= 3,
			.cra_module		= THIS_MODULE,
			.cra_init		= sunxi_ss_cra_hash_init,
			.cra_exit		= sunxi_ss_cra_exit,
		}
	},
#endif
#ifdef SS_SHA256_ENABLE
	{
		.init		= ss_sha256_init,
		.update		= ss_hash_update,
		.final		= ss_hash_final,
		.finup		= ss_hash_finup,
		.halg.digestsize	= SHA256_DIGEST_SIZE,
		.halg.base	= {
			.cra_name		= "sha256",
			.cra_driver_name= "ss-sha256",
			.cra_flags		= CRYPTO_ALG_TYPE_AHASH|CRYPTO_ALG_ASYNC,
			.cra_blocksize	= SHA256_BLOCK_SIZE,
			.cra_ctxsize	= sizeof(ss_hash_ctx_t),
			.cra_alignmask	= 3,
			.cra_module		= THIS_MODULE,
			.cra_init		= sunxi_ss_cra_hash_init,
			.cra_exit		= sunxi_ss_cra_exit,
		}
	},
#endif
};

static int ss_aes_one_req(sunxi_ss_t *sss, struct ablkcipher_request *req)
{
	int ret = 0;
	struct crypto_ablkcipher *tfm = NULL;
	ss_aes_ctx_t *ctx = NULL;
	ss_aes_req_ctx_t *req_ctx = NULL;
#ifdef SS_IDMA_ENABLE
	int key_map_flag = 0;
	int iv_map_flag = 0;
#endif

	SS_ENTER();
	if (!req->src || !req->dst) {
		SS_ERR("Invalid sg: src = %p, dst = %p\n", req->src, req->dst);
		return -EINVAL;
	}

	mutex_lock(&ss_lock);

	tfm = crypto_ablkcipher_reqtfm(req);
	req_ctx = ablkcipher_request_ctx(req);
	ctx = crypto_ablkcipher_ctx(tfm);

	/* A31 SS need update key each cycle in decryption. */
	if ((ctx->comm.flags & SS_FLAG_NEW_KEY) || (req_ctx->dir == SS_DIR_DECRYPT)) {
		SS_DBG("KEY address = %p, size = %d\n", ctx->key, ctx->key_size);
		ss_key_set(ctx->key, ctx->key_size);
#ifdef SS_IDMA_ENABLE
		dma_map_single(&sss->pdev->dev, ctx->key, ctx->key_size, DMA_MEM_TO_DEV);
		key_map_flag = 1;
#endif
		ctx->comm.flags &= ~SS_FLAG_NEW_KEY;
	}

#ifdef SS_CTS_MODE_ENABLE
	if (((req_ctx->mode == SS_AES_MODE_CBC)
			|| (req_ctx->mode == SS_AES_MODE_CTS)) && (req->info != NULL)) {
#else
	if ((req_ctx->mode == SS_AES_MODE_CBC) && (req->info != NULL)) {
#endif
		SS_DBG("IV address = %p, size = %d\n", req->info, crypto_ablkcipher_ivsize(tfm));
#ifdef SS_IDMA_ENABLE
		memcpy(ctx->iv, req->info, crypto_ablkcipher_ivsize(tfm));
		ss_iv_set(ctx->iv, crypto_ablkcipher_ivsize(tfm));
		dma_map_single(&sss->pdev->dev, ctx->iv, crypto_ablkcipher_ivsize(tfm), DMA_MEM_TO_DEV);
		iv_map_flag = 1;
#else
		ss_iv_set(req->info, crypto_ablkcipher_ivsize(tfm));
#endif
	}

#ifdef SS_CTR_MODE_ENABLE
	if (req_ctx->mode == SS_AES_MODE_CTR) {
		SS_DBG("Cnt address = %p, size = %d\n", req->info, crypto_ablkcipher_ivsize(tfm));
		if (ctx->cnt == 0)
			memcpy(ctx->iv, req->info, crypto_ablkcipher_ivsize(tfm));

		SS_DBG("CNT: %08x %08x %08x %08x \n", *(int *)&ctx->iv[0],
			*(int *)&ctx->iv[4], *(int *)&ctx->iv[8], *(int *)&ctx->iv[12]);
		ss_cnt_set(ctx->iv, crypto_ablkcipher_ivsize(tfm));
#ifdef SS_IDMA_ENABLE
		dma_map_single(&sss->pdev->dev, ctx->iv, crypto_ablkcipher_ivsize(tfm), DMA_MEM_TO_DEV);
		iv_map_flag = 1;
#endif
	}
#endif

#ifdef SS_RSA_ENABLE
	if (req_ctx->type == SS_METHOD_RSA)
		ss_rsa_width_set(crypto_ablkcipher_ivsize(tfm));
#endif

	req_ctx->dma_src.sg = req->src;
	req_ctx->dma_dst.sg = req->dst;

	ret = ss_aes_start(ctx, req_ctx, req->nbytes);
	if (ret < 0)
		SS_ERR("ss_aes_start fail(%d)\n", ret);

	mutex_unlock(&ss_lock);
	if (req->base.complete)
		req->base.complete(&req->base, ret);

#ifdef SS_IDMA_ENABLE
	if (key_map_flag == 1)
		dma_unmap_single(&ss_dev->pdev->dev, virt_to_phys(ctx->key), ctx->key_size, DMA_MEM_TO_DEV);
	if (iv_map_flag == 1)
		dma_unmap_single(&sss->pdev->dev, virt_to_phys(ctx->iv), crypto_ablkcipher_ivsize(tfm), DMA_MEM_TO_DEV);
#endif

#ifdef SS_CTR_MODE_ENABLE
	if (req_ctx->mode == SS_AES_MODE_CTR) {
		ss_cnt_get(ctx->comm.flow, ctx->iv, crypto_ablkcipher_ivsize(tfm));
		SS_DBG("CNT: %08x %08x %08x %08x \n", *(int *)&ctx->iv[0],
			*(int *)&ctx->iv[4], *(int *)&ctx->iv[8], *(int *)&ctx->iv[12]);
	}
#endif
	ctx->cnt += req->nbytes;
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

	mutex_lock(&ss_lock);

	req_ctx = ahash_request_ctx(req);
	req_ctx->dma_src.sg = req->src;

	ss_hash_padding_data_prepare(ctx, req->result, req->nbytes);

	ret = ss_hash_start(ctx, req_ctx, req->nbytes);
	if (ret < 0)
		SS_ERR("ss_hash_start fail(%d)\n", ret);

	mutex_unlock(&ss_lock);

	if (req->base.complete)
		req->base.complete(&req->base, ret);

	return ret;
}

static void sunxi_ss_work(struct work_struct *work)
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

static irqreturn_t sunxi_ss_irq_handler(int irq, void *dev_id)
{
	sunxi_ss_t *sss = (sunxi_ss_t *)dev_id;
	unsigned long flags = 0;
	int pending = 0;

	spin_lock_irqsave(&sss->lock, flags);

	pending = ss_pending_get();
	SS_DBG("SS pending %#x\n", pending);
#ifdef SS_IDMA_ENABLE
	if (pending&SS_REG_ICR_FLOW0_PENDING_MASK) {
		ss_pending_clear(0);
		complete(&sss->flows[0].done);
	}
	if (pending&SS_REG_ICR_FLOW1_PENDING_MASK) {
		ss_pending_clear(1);
		complete(&sss->flows[1].done);
	}
#endif
	spin_unlock_irqrestore(&sss->lock, flags);
#ifdef SS_IDMA_ENABLE
	SS_DBG("SS pending %#x, CTRL: 0x%08x\n", ss_pending_get(), ss_reg_rd(SS_REG_CTL));
#endif

	return IRQ_HANDLED;
}

/* Requeset the resource: IRQ, mem */
static int __devinit sunxi_ss_res_request(struct platform_device *pdev)
{
	int irq = 0;
	int ret = 0;
	struct resource	*mem_res = NULL;
	sunxi_ss_t *sss = platform_get_drvdata(pdev);

#ifdef SS_IDMA_ENABLE
	int i;
	for (i=0; i<SS_FLOW_NUM; i++) {
		sss->flows[i].buf_src = (char *)kmalloc(SS_DMA_BUF_SIZE, GFP_KERNEL);
		if (sss->flows[i].buf_src == NULL) {
			SS_ERR("Can not allocate DMA source buffer\n");
			return -ENOMEM;
		}
		sss->flows[i].buf_src_dma = virt_to_phys(sss->flows[i].buf_src);

		sss->flows[i].buf_dst = (char *)kmalloc(SS_DMA_BUF_SIZE, GFP_KERNEL);
		if (sss->flows[i].buf_dst == NULL) {
			SS_ERR("Can not allocate DMA source buffer\n");
			return -ENOMEM;
		}
		sss->flows[i].buf_dst_dma = virt_to_phys(sss->flows[i].buf_dst);
		init_completion(&sss->flows[i].done);
	}
#endif

	mem_res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (mem_res == NULL) {
		SS_ERR("Unable to get SS MEM resource\n");
		return -ENXIO;
	}

	irq = platform_get_irq(pdev, 0);
	if (irq < 0) {
		SS_ERR("No SS IRQ specified\n");
		return irq;
	}

	sss->irq = irq;

	ret = request_irq(sss->irq, sunxi_ss_irq_handler, IRQF_DISABLED, sss->dev_name, sss);
	if (ret != 0) {
		SS_ERR("Cannot request IRQ\n");
		return ret;
	}

	if (request_mem_region(mem_res->start,
			resource_size(mem_res), pdev->name) == NULL) {
		SS_ERR("Req mem region failed\n");
		return -ENXIO;
	}

	sss->base_addr = ioremap(mem_res->start, resource_size(mem_res));
	if (sss->base_addr == NULL) {
		SS_ERR("Unable to remap IO\n");
		return -ENXIO;
	}
	sss->base_addr_phy = mem_res->start;

	return 0;
}

/* Release the resource: IRQ, mem */
static int __devexit sunxi_ss_res_release(sunxi_ss_t *sss)
{
#ifdef SS_IDMA_ENABLE
	int i;
#endif
	struct resource	*mem_res = NULL;

	iounmap(sss->base_addr);
	mem_res = platform_get_resource(sss->pdev, IORESOURCE_MEM, 0);
	if (mem_res != NULL)
		release_mem_region(mem_res->start, resource_size(mem_res));

#ifdef SS_IDMA_ENABLE
	for (i=0; i<SS_FLOW_NUM; i++) {
		kfree(sss->flows[i].buf_src);
		kfree(sss->flows[i].buf_dst);
	}
#endif

	free_irq(sss->irq, sss);
	return 0;
}

#ifdef CONFIG_EVB_PLATFORM

static int sunxi_ss_hw_init(sunxi_ss_t *sss)
{
	int ret = 0;

	sss->pclk = clk_get(&sss->pdev->dev, SS_PLL_CLK);
	if (IS_ERR_OR_NULL(sss->pclk)) {
		SS_ERR("Unable to acquire module clock '%s', return %x\n",
				SS_PLL_CLK, PTR_RET(sss->pclk));
		return PTR_RET(sss->pclk);
	}

	sss->mclk = clk_get(&sss->pdev->dev, sss->dev_name);
	if (IS_ERR_OR_NULL(sss->mclk)) {
		SS_ERR("Unable to acquire module clock '%s', return %x\n",
				sss->dev_name, PTR_RET(sss->mclk));
		return PTR_RET(sss->mclk);
	}

	ret = clk_set_parent(sss->mclk, sss->pclk);
	if (ret != 0) {
		SS_ERR("clk_set_parent() failed! return %d\n", ret);
		return ret;
	}

	ret = clk_set_rate(sss->mclk, SS_CLK_RATE);
	if (ret != 0) {
		SS_ERR("clk_set_rate(%d) failed! return %d\n", SS_CLK_RATE, ret);
		return ret;
	}

	SS_DBG("SS mclk %luMHz, pclk %luMHz\n", clk_get_rate(sss->mclk)/1000000,
			clk_get_rate(sss->pclk)/1000000);

	if (clk_prepare_enable(sss->mclk)) {
		SS_ERR("Couldn't enable module clock\n");
		return -EBUSY;
	}

//	sunxi_periph_reset_deassert(sss->mclk);
	return 0;
}

static int sunxi_ss_hw_exit(sunxi_ss_t *sss)
{
//	sunxi_periph_reset_assert(sss->mclk);
	clk_disable_unprepare(sss->mclk);
	clk_put(sss->mclk);
	clk_put(sss->pclk);
	sss->mclk = NULL;
	sss->pclk = NULL;
	return 0;
}

#else

static int sunxi_ss_hw_init(sunxi_ss_t *sss)
{
	sss->mclk = clk_get(&sss->pdev->dev, sss->dev_name);
	if (IS_ERR_OR_NULL(sss->mclk)) {
		SS_ERR("Unable to acquire module clock '%s', return %x\n",
				sss->dev_name, PTR_RET(sss->mclk));
		return PTR_RET(sss->mclk);
	}

	SS_DBG("SS mclk %luMHz\n", clk_get_rate(sss->mclk)/1000000);

	if (clk_prepare_enable(sss->mclk)) {
		SS_ERR("Couldn't enable module clock\n");
		return -EBUSY;
	}
	return 0;
}

static int sunxi_ss_hw_exit(sunxi_ss_t *sss)
{
	clk_disable_unprepare(sss->mclk);
	clk_put(sss->mclk);
	sss->mclk = NULL;
	return 0;
}

#endif

static int sunxi_ss_alg_register(void)
{
	int i;
	int ret = 0;

	for (i=0; i<ARRAY_SIZE(sunxi_ss_algs); i++) {
		INIT_LIST_HEAD(&sunxi_ss_algs[i].cra_list);

		sunxi_ss_algs[i].cra_priority = 300;
		sunxi_ss_algs[i].cra_ctxsize = sizeof(ss_aes_ctx_t);
		sunxi_ss_algs[i].cra_module = THIS_MODULE;
		sunxi_ss_algs[i].cra_exit = sunxi_ss_cra_exit;
		if (strncmp(sunxi_ss_algs[i].cra_name, "prng", 4) == 0)
			sunxi_ss_algs[i].cra_init = sunxi_ss_cra_rng_init;
		else
			sunxi_ss_algs[i].cra_init = sunxi_ss_cra_init;

		ret = crypto_register_alg(&sunxi_ss_algs[i]);
		if (ret != 0) {
			SS_ERR("crypto_register_alg(%s) failed! return %d \n",
				sunxi_ss_algs[i].cra_name, ret);
			return ret;
		}
	}

	for (i=0; i<ARRAY_SIZE(sunxi_ss_algs_hash); i++) {
		sunxi_ss_algs_hash[i].halg.base.cra_priority = 300;
		ret = crypto_register_ahash(&sunxi_ss_algs_hash[i]);
		if (ret != 0) {
			SS_ERR("crypto_register_ahash(%s) failed! return %d \n",
				sunxi_ss_algs_hash[i].halg.base.cra_name, ret);
			return ret;
		}
	}

	return 0;
}

static void sunxi_ss_alg_unregister(void)
{
	int i;

	for (i=0; i<ARRAY_SIZE(sunxi_ss_algs); i++)
		crypto_unregister_alg(&sunxi_ss_algs[i]);

	for (i=0; i<ARRAY_SIZE(sunxi_ss_algs_hash); i++)
		crypto_unregister_ahash(&sunxi_ss_algs_hash[i]);
}

static int __devinit sunxi_ss_probe(struct platform_device *pdev)
{
	int ret = 0;
	sunxi_ss_t *sss = NULL;

	sss = devm_kzalloc(&pdev->dev, sizeof(sunxi_ss_t), GFP_KERNEL);
	if (sss == NULL) {
		SS_ERR("Unable to allocate sunxi_ss_t\n");
		return -ENOMEM;
	}

	snprintf(sss->dev_name, sizeof(sss->dev_name), SUNXI_SS_DEV_NAME);
	platform_set_drvdata(pdev, sss);

	ret = sunxi_ss_res_request(pdev);
	if (ret != 0) {
		goto err0;
	}

    sss->pdev = pdev;

	ret = sunxi_ss_hw_init(sss);
	if (ret != 0) {
		SS_ERR("SS hw init failed!\n");
		goto err1;
	}

	spin_lock_init(&sss->lock);
	INIT_WORK(&sss->work, sunxi_ss_work);
	crypto_init_queue(&sss->queue, 16);

	sss->workqueue = create_singlethread_workqueue(sss->dev_name);
	if (sss->workqueue == NULL) {
		SS_ERR("Unable to create workqueue\n");
		ret = -EPERM;
		goto err2;
	}

	ret = sunxi_ss_alg_register();
	if (ret != 0) {
		SS_ERR("sunxi_ss_alg_register() failed! return %d \n", ret);
		goto err3;
	}

	ss_dev = sss;
	SS_DBG("SS driver probe succeed, base %p, irq %d!\n", sss->base_addr, sss->irq);
	return 0;

err3:
	destroy_workqueue(sss->workqueue);
err2:
	sunxi_ss_hw_exit(sss);
err1:
	sunxi_ss_res_release(sss);
err0:
	platform_set_drvdata(pdev, NULL);

	return ret;
}

static int __devexit sunxi_ss_remove(struct platform_device *pdev)
{
	sunxi_ss_t *sss = platform_get_drvdata(pdev);

	ss_wait_idle();

	cancel_work_sync(&sss->work);
	flush_workqueue(sss->workqueue);
	destroy_workqueue(sss->workqueue);

	sunxi_ss_alg_unregister();
	sunxi_ss_hw_exit(sss);
	sunxi_ss_res_release(sss);

	platform_set_drvdata(pdev, NULL);
	ss_dev = NULL;
	return 0;
}

static void sunxi_ss_release(struct device *dev)
{
	SS_ENTER();
}

#ifdef CONFIG_PM
static int sunxi_ss_suspend(struct device *dev)
{
#ifdef CONFIG_EVB_PLATFORM
	struct platform_device *pdev = to_platform_device(dev);
	sunxi_ss_t *sss = platform_get_drvdata(pdev);
	unsigned long flags = 0;

	SS_ENTER();

	/* Wait for the completion of SS operation. */
	mutex_lock(&ss_lock);

	spin_lock_irqsave(&ss_dev->lock, flags);
	sss->suspend = 1;
	spin_unlock_irqrestore(&sss->lock, flags);

	sunxi_ss_hw_exit(sss);
	mutex_unlock(&ss_lock);
#endif

	return 0;
}

static int sunxi_ss_resume(struct device *dev)
{
	int ret = 0;
#ifdef CONFIG_EVB_PLATFORM
	struct platform_device *pdev = to_platform_device(dev);
	sunxi_ss_t *sss = platform_get_drvdata(pdev);
	unsigned long flags = 0;

	SS_ENTER();
	ret = sunxi_ss_hw_init(sss);
	spin_lock_irqsave(&ss_dev->lock, flags);
	sss->suspend = 0;
	spin_unlock_irqrestore(&sss->lock, flags);
#endif
	return ret;
}

static const struct dev_pm_ops sunxi_ss_dev_pm_ops = {
	.suspend = sunxi_ss_suspend,
	.resume  = sunxi_ss_resume,
};

#define SUNXI_SS_DEV_PM_OPS (&sunxi_ss_dev_pm_ops)
#else
#define SUNXI_SS_DEV_PM_OPS NULL
#endif /* CONFIG_PM */

static struct platform_driver sunxi_ss_driver = {
	.probe   = sunxi_ss_probe,
	.remove  = sunxi_ss_remove,
	.driver = {
        .name	= SUNXI_SS_DEV_NAME,
		.owner	= THIS_MODULE,
		.pm		= SUNXI_SS_DEV_PM_OPS,
	},
};

static struct resource sunxi_ss_resources[] = {
	{
		.start	= SUNXI_SS_MEM_START,
		.end 	= SUNXI_SS_MEM_END,
		.flags	= IORESOURCE_MEM,
	},
	{
		.start	= SUNXI_SS_IRQ,
		.end 	= SUNXI_SS_IRQ,
		.flags	= IORESOURCE_IRQ,
	},
};

static struct platform_device sunxi_ss_device = {
	.name = SUNXI_SS_DEV_NAME,
	.resource = sunxi_ss_resources,
	.num_resources = 2,
	.dev.release = sunxi_ss_release,
};

static ssize_t sunxi_ss_info_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct platform_device *pdev = container_of(dev, struct platform_device, dev);
	sunxi_ss_t *sss = platform_get_drvdata(pdev);

	return snprintf(buf, PAGE_SIZE,
		"pdev->id   = %d \n"
		"pdev->name = %s \n"
		"pdev->num_resources = %u \n"
		"pdev->resource.mem = [0x%08x, 0x%08x] \n"
		"pdev->resource.irq = %d \n"
		"SS module clk rate = %ld Mhz \n",
		pdev->id, pdev->name, pdev->num_resources,
		pdev->resource[0].start, pdev->resource[0].end, pdev->resource[1].start,
		clk_get_rate(sss->mclk)/1000000);
}
static struct device_attribute sunxi_ss_info_attr =
	__ATTR(info, S_IRUGO, sunxi_ss_info_show, NULL);

static ssize_t sunxi_ss_status_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int i;
	struct platform_device *pdev = container_of(dev, struct platform_device, dev);
	sunxi_ss_t *sss = platform_get_drvdata(pdev);
	char *avail[] = {"Available", "Unavailable"};

	if (sss == NULL)
		return snprintf(buf, PAGE_SIZE, "%s\n", "sunxi_ss_t is NULL!");

	for (i=0; i<SS_FLOW_NUM; i++) {
		printk("The flow %d state: %s \n", i, avail[sss->flows[i].available]);
#ifdef SS_IDMA_ENABLE
		printk("    Src: 0x%p / 0x%08x \n", sss->flows[i].buf_src, sss->flows[i].buf_src_dma);
		printk("    Dst: 0x%p / 0x%08x \n", sss->flows[i].buf_dst, sss->flows[i].buf_dst_dma);
#endif
	}

	return ss_reg_print(buf, PAGE_SIZE);
}

static struct device_attribute sunxi_ss_status_attr =
	__ATTR(status, S_IRUGO, sunxi_ss_status_show, NULL);

static void sunxi_ss_sysfs_create(struct platform_device *_pdev)
{
	device_create_file(&_pdev->dev, &sunxi_ss_info_attr);
	device_create_file(&_pdev->dev, &sunxi_ss_status_attr);
}

static void sunxi_ss_sysfs_remove(struct platform_device *_pdev)
{
	device_remove_file(&_pdev->dev, &sunxi_ss_info_attr);
	device_remove_file(&_pdev->dev, &sunxi_ss_status_attr);
}

static int __init sunxi_ss_init(void)
{
    int ret = 0;

	SS_ERR("[%s %s]Sunxi SS init ... \n", __DATE__, __TIME__);

	ret = platform_driver_register(&sunxi_ss_driver);
	if (ret < 0) {
		SS_ERR("platform_driver_register() failed, return %d\n", ret);
		return ret;
	}

	ret = platform_device_register(&sunxi_ss_device);
	if (ret < 0) {
		SS_ERR("platform_device_register() failed, return %d\n", ret);
		return ret;
	}
	sunxi_ss_sysfs_create(&sunxi_ss_device);

	return ret;
}

static void __exit sunxi_ss_exit(void)
{
	sunxi_ss_sysfs_remove(&sunxi_ss_device);
	platform_device_unregister(&sunxi_ss_device);
    platform_driver_unregister(&sunxi_ss_driver);
}

module_init(sunxi_ss_init);
module_exit(sunxi_ss_exit);

MODULE_AUTHOR("mintow");
MODULE_DESCRIPTION("SUNXI SS Controller Driver");
MODULE_ALIAS("platform:"SUNXI_SS_DEV_NAME);
MODULE_LICENSE("GPL");

