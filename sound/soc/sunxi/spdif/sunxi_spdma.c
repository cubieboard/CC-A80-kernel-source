/*
 * sound\soc\sunxi\spdif\sunxi_spdma.c
 * (C) Copyright 2010-2016
 * Reuuimlla Technology Co., Ltd. <www.reuuimllatech.com>
 * chenpailin <chenpailin@Reuuimllatech.com>
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
#include <linux/init.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/dma-mapping.h>
#include <linux/dma/sunxi-dma.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>

#include <asm/dma.h>
#include <mach/hardware.h>
#include <sound/dmaengine_pcm.h>

#include "sunxi_spdif.h"
#include "sunxi_spdma.h"

static const struct snd_pcm_hardware sunxi_pcm_hardware = {
	.info			= SNDRV_PCM_INFO_INTERLEAVED | SNDRV_PCM_INFO_BLOCK_TRANSFER |
				      SNDRV_PCM_INFO_MMAP | SNDRV_PCM_INFO_MMAP_VALID |
				      SNDRV_PCM_INFO_PAUSE | SNDRV_PCM_INFO_RESUME,
	.formats		= SNDRV_PCM_FMTBIT_S16_LE |SNDRV_PCM_FMTBIT_S20_3LE| SNDRV_PCM_FMTBIT_S24_LE | SNDRV_PCM_FMTBIT_S32_LE,
	.rates			= SNDRV_PCM_RATE_8000_192000 | SNDRV_PCM_RATE_KNOT,
	.rate_min		= 8000,
	.rate_max		= 192000,
	.channels_min		= 1,
	.channels_max		= 8,
	.buffer_bytes_max	= 1024*1024,  /* value must be (2^n)Kbyte size */
	.period_bytes_min	= 1024,
	.period_bytes_max	= 1024*32,
	.periods_min		= 1,
	.periods_max		= 8,
	.fifo_size		= 32,
};

static int sunxi_pcm_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params)
{
    struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct device *dev = rtd->platform->dev;
	struct dma_chan *chan = snd_dmaengine_pcm_get_chan(substream);
	struct sunxi_dma_params *dmap;
	struct dma_slave_config slave_config;
	int ret;

	dmap = snd_soc_dai_get_dma_data(rtd->cpu_dai, substream);

	ret = snd_hwparams_to_dma_slave_config(substream, params,
						&slave_config);
	if (ret) {
		dev_err(dev, "hw params config failed with err %d\n", ret);
		return ret;
	}

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		if (SNDRV_PCM_FORMAT_S16_LE == params_format(params)) {
			slave_config.dst_addr_width = DMA_SLAVE_BUSWIDTH_2_BYTES;
			slave_config.src_addr_width = DMA_SLAVE_BUSWIDTH_2_BYTES;
		} else {
			slave_config.dst_addr_width = DMA_SLAVE_BUSWIDTH_4_BYTES;
			slave_config.src_addr_width = DMA_SLAVE_BUSWIDTH_4_BYTES;
		}
		slave_config.dst_addr = dmap->dma_addr;
		slave_config.src_maxburst = 2;
		slave_config.dst_maxburst = 2;
		slave_config.slave_id = sunxi_slave_id(DRQDST_SPDIFRX, DRQSRC_SDRAM);
	} else {
		if (SNDRV_PCM_FORMAT_S16_LE == params_format(params)) {
			slave_config.dst_addr_width = DMA_SLAVE_BUSWIDTH_2_BYTES;
			slave_config.src_addr_width = DMA_SLAVE_BUSWIDTH_2_BYTES;
		} else {
			slave_config.dst_addr_width = DMA_SLAVE_BUSWIDTH_4_BYTES;
			slave_config.src_addr_width = DMA_SLAVE_BUSWIDTH_4_BYTES;
		}
		slave_config.src_addr = dmap->dma_addr;
		slave_config.src_maxburst = 2;
		slave_config.dst_maxburst = 2;
		slave_config.slave_id = sunxi_slave_id(DRQDST_SDRAM, DRQSRC_SPDIFRX);
	}

	ret = dmaengine_slave_config(chan, &slave_config);
	if (ret < 0) {
		dev_err(dev, "dma slave config failed with err %d\n", ret);
		return ret;
	}

	snd_pcm_set_runtime_buffer(substream, &substream->dma_buffer);
	return 0;
}

static int sunxi_pcm_hw_free(struct snd_pcm_substream *substream)
{
	snd_pcm_set_runtime_buffer(substream, NULL);
	return 0;
}

static int sunxi_pcm_trigger(struct snd_pcm_substream *substream, int cmd)
{
	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		return snd_dmaengine_pcm_trigger(substream, SNDRV_PCM_TRIGGER_START);
	case SNDRV_PCM_TRIGGER_SUSPEND:
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		return snd_dmaengine_pcm_trigger(substream, SNDRV_PCM_TRIGGER_STOP);
	default:
		return -EINVAL;
	}
}

static int sunxi_pcm_open(struct snd_pcm_substream *substream)
{	
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct device *dev = rtd->platform->dev;
	int ret = 0;

	/* Set HW params now that initialization is complete */
	snd_soc_set_runtime_hwparams(substream, &sunxi_pcm_hardware);

	ret = snd_dmaengine_pcm_open(substream, NULL, NULL);
	if (ret) {
		dev_err(dev, "dmaengine pcm open failed with err %d\n", ret);
		return ret;
	}

	return 0;
}

static int sunxi_pcm_close(struct snd_pcm_substream *substream)
{
	snd_dmaengine_pcm_close(substream);
	
	return 0;
}

static int sunxi_pcm_mmap(struct snd_pcm_substream *substream,
	struct vm_area_struct *vma)
{
	struct snd_pcm_runtime *runtime = NULL;
	if (substream->runtime!=NULL) {
		runtime = substream->runtime;

		return dma_mmap_writecombine(substream->pcm->card->dev, vma,
					     runtime->dma_area,
					     runtime->dma_addr,
					     runtime->dma_bytes);
	} else {
		return -1;
	}
}

static struct snd_pcm_ops sunxi_pcm_ops = {
	.open			= sunxi_pcm_open,
	.close			= sunxi_pcm_close,
	.ioctl			= snd_pcm_lib_ioctl,
	.hw_params		= sunxi_pcm_hw_params,
	.hw_free		= sunxi_pcm_hw_free,
	.trigger		= sunxi_pcm_trigger,
	.pointer		= snd_dmaengine_pcm_pointer,
	.mmap			= sunxi_pcm_mmap,
};

static int sunxi_pcm_preallocate_dma_buffer(struct snd_pcm *pcm, int stream)
{
	struct snd_pcm_substream *substream = pcm->streams[stream].substream;
	struct snd_dma_buffer *buf = &substream->dma_buffer;
	size_t size = 0;
	
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		size = sunxi_pcm_hardware.buffer_bytes_max;
	} else {
		size = sunxi_pcm_hardware.buffer_bytes_max;
	}	
	buf->dev.type = SNDRV_DMA_TYPE_DEV;
	buf->dev.dev = pcm->card->dev;
	buf->private_data = NULL;
	buf->area = dma_alloc_writecombine(pcm->card->dev, size,
					   &buf->addr, GFP_KERNEL);	
	if (!buf->area)
		return -ENOMEM;
	buf->bytes = size;
	
	return 0;
}

static void sunxi_pcm_free_dma_buffers(struct snd_pcm *pcm)
{
	struct snd_pcm_substream *substream;
	struct snd_dma_buffer *buf;
	int stream;
	 
	for (stream = 0; stream < 2; stream++) {
		substream = pcm->streams[stream].substream;
		if (!substream)
			continue;

		buf = &substream->dma_buffer;
		if (!buf->area)
			continue;

		dma_free_writecombine(pcm->card->dev, buf->bytes,
				      buf->area, buf->addr);
		buf->area = NULL;
	}
}

static u64 sunxi_pcm_mask = DMA_BIT_MASK(32);

static int sunxi_pcm_new(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_card *card = rtd->card->snd_card;
	struct snd_pcm *pcm = rtd->pcm;	
	int ret = 0;

	if (!card->dev->dma_mask)
		card->dev->dma_mask = &sunxi_pcm_mask;
	if (!card->dev->coherent_dma_mask)
		card->dev->coherent_dma_mask = 0xffffffff;

	if (pcm->streams[SNDRV_PCM_STREAM_PLAYBACK].substream) {	
		ret = sunxi_pcm_preallocate_dma_buffer(pcm,
			SNDRV_PCM_STREAM_PLAYBACK);
		if (ret)
			goto out;
	}
	
	if (pcm->streams[SNDRV_PCM_STREAM_CAPTURE].substream) {
		ret = sunxi_pcm_preallocate_dma_buffer(pcm,
			SNDRV_PCM_STREAM_CAPTURE);
		if (ret)
			goto out;
	}
 out:
	return ret;
}

static struct snd_soc_platform_driver sunxi_soc_platform = {
	.ops  		=   &sunxi_pcm_ops,
	.pcm_new	=	sunxi_pcm_new,
	.pcm_free	=	sunxi_pcm_free_dma_buffers,
};

static int __init sunxi_spdif_pcm_probe(struct platform_device *pdev)
{	
	return snd_soc_register_platform(&pdev->dev, &sunxi_soc_platform);
}

static int __exit sunxi_spdif_pcm_remove(struct platform_device *pdev)
{
	snd_soc_unregister_platform(&pdev->dev);
	return 0;
}

/*data relating*/
static struct platform_device sunxi_spdif_pcm_device = {
	.name = "sunxi-spdif-pcm-audio",
};

/*method relating*/
static struct platform_driver sunxi_spdif_pcm_driver = {
	.probe = sunxi_spdif_pcm_probe,
	.remove = __exit_p(sunxi_spdif_pcm_remove),
	.driver = {
		.name = "sunxi-spdif-pcm-audio",
		.owner = THIS_MODULE,
	},
};

static int __init sunxi_soc_platform_spdif_init(void)
{
	int err = 0;	
	if((err = platform_device_register(&sunxi_spdif_pcm_device)) < 0)
		return err;

	if ((err = platform_driver_register(&sunxi_spdif_pcm_driver)) < 0)
		return err;
	return 0;	
}
module_init(sunxi_soc_platform_spdif_init);

static void __exit sunxi_soc_platform_spdif_exit(void)
{
	return platform_driver_unregister(&sunxi_spdif_pcm_driver);
}
module_exit(sunxi_soc_platform_spdif_exit);

MODULE_AUTHOR("All winner");
MODULE_DESCRIPTION("SUNXI SPDIF DMA module");
MODULE_LICENSE("GPL");
