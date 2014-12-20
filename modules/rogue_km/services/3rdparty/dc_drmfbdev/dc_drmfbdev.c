/*************************************************************************/ /*!
@File
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@License        Strictly Confidential.
*/ /**************************************************************************/

#include <linux/version.h>

#if !defined(LMA)

#include <asm/div64.h>
#include <asm/page.h>
#include <linux/console.h>
#include <linux/module.h>
#include <linux/spinlock.h>

#include <drm/drmP.h>

#include <drm/drm_crtc.h>
#include <drm/drm_crtc_helper.h>
#include <drm/drm_fb_helper.h>

#include <pvr_drm_display.h>
#include "pvrmodule.h"
#include "pvrversion.h"

#if defined(__GNUC__)
#define unref__ __attribute__ ((unused))
#else
#define unref__
#endif

#define	MAKESTRING(x)	#x
#define TOSTRING(x)	MAKESTRING(x)

#define DRVNAME		TOSTRING(DISPLAY_CONTROLLER)

#if defined(DC_FBDEV_NUM_PREFERRED_BUFFERS)
#define NUM_PREFERRED_BUFFERS	DC_FBDEV_NUM_PREFERRED_BUFFERS
#else
#define NUM_PREFERRED_BUFFERS	4
#endif

#if 1
#define	FTRACE()
#else
#define	FTRACE()	pr_info(DRVNAME ": %s\n", __FUNCTION__)
#endif

static bool FFBAllocatedBufferMem[NUM_PREFERRED_BUFFERS];
static DEFINE_SPINLOCK(FFBAllocBufferMemLock);

typedef struct PVR_DEVINFO_TAG
{
	struct drm_device		*dev;

	struct fb_info			*ffbInfo;
	uint32_t			pixFormat;
	bool				canFlip;
} PVR_DEVINFO;

typedef struct pvr_drm_display_buffer
{
	PVR_DEVINFO		*devInfo;
	size_t			size;
	int			bufferNum;
	struct kref		refcount;
} PVR_BUFFER;

typedef struct PVR_CRTC_TAG
{
	PVR_DEVINFO			*devInfo;
	struct drm_crtc			base;
	uint32_t			number;
	struct drm_pending_vblank_event	*event;
	struct pvr_drm_flip_data	*activeFlipData;
	struct pvr_drm_flip_data	*finishedFlipData;
} PVR_CRTC;

typedef struct PVR_CONNECTOR_TAG
{
	PVR_DEVINFO			*devInfo;
	struct drm_connector		base;
} PVR_CONNECTOR;

typedef struct PVR_ENCODER_TAG
{
	PVR_DEVINFO			*devInfo;
	struct drm_encoder		base;
} PVR_ENCODER;

typedef struct PVR_FRAMEBUFFER_TAG
{
	struct drm_framebuffer		base;
	struct drm_gem_object		*obj;
	uint64_t			dev_base_addr;
} PVR_FRAMEBUFFER;

#define to_pvr_crtc(crtc)		container_of(crtc, PVR_CRTC, base)
#define to_pvr_connector(connector)	container_of(connector, PVR_CONNECTOR, base)
#define to_pvr_encoder(encoder)		container_of(encoder, PVR_ENCODER, base)
#define to_pvr_framebuffer(framebuffer)	container_of(framebuffer, PVR_FRAMEBUFFER, base)


/******************************************************************************
 * Foreign framebuffer functions
 ******************************************************************************/
/* If we can flip, we need to make sure we have the memory to do so.
 *
 * We'll assume that the fbdev device provides extra space in
 * yres_virtual for panning; xres_virtual is theoretically supported,
 * but it involves more work.
 *
 * If the fbdev device doesn't have yres_virtual > yres, we'll try
 * requesting it before bailing. Userspace applications commonly do
 * this with an FBIOPUT_VSCREENINFO ioctl().
 *
 * We need framebuffers to be page aligned, so we check that
 * the stride * height for a single buffer is page aligned.
 */

static int FFBAllocBufferMem(void)
{
	int buf;

	FTRACE();

	spin_lock(&FFBAllocBufferMemLock);

	for (buf = 0; buf < NUM_PREFERRED_BUFFERS; buf++)
	{
		if (!FFBAllocatedBufferMem[buf])
		{
			FFBAllocatedBufferMem[buf] = true;
			break;
		}
	}
	if (buf >= NUM_PREFERRED_BUFFERS)
	{
		buf = -1;
	}

	spin_unlock(&FFBAllocBufferMemLock);

	return buf;
}

static void FFBFreeBufferMem(int buf)
{
	FTRACE();

	spin_lock(&FFBAllocBufferMemLock);

	BUG_ON(buf < 0 || buf >= NUM_PREFERRED_BUFFERS);
	BUG_ON(!FFBAllocatedBufferMem[buf]);

	FFBAllocatedBufferMem[buf] = false;

	spin_unlock(&FFBAllocBufferMemLock);
}

static int FFBFlip(PVR_DEVINFO *devInfo, uint64_t addr)
{
	struct fb_var_screeninfo sVar = devInfo->ffbInfo->var;
	uint64_t offset = (addr - devInfo->ffbInfo->fix.smem_start);
	uint32_t offset_remainder;
	int err = -EINVAL;

	FTRACE();

	if (!devInfo->canFlip)
	{
		return err;
	}

	offset_remainder = do_div(offset, devInfo->ffbInfo->fix.line_length);
	BUG_ON(offset_remainder != 0);

	sVar.yoffset = offset;

	if (lock_fb_info(devInfo->ffbInfo))
	{
		console_lock();

		/* If we're supposed to be able to flip, but the yres_virtual
		 * has been changed to an unsupported (smaller) value, we need
		 * to change it back (this is a workaround for some Linux fbdev
		 * drivers that seem to lose any modifications to yres_virtual
		 * after a blank.)
		 */
		if (sVar.yres_virtual < sVar.yres * NUM_PREFERRED_BUFFERS)
		{
			sVar.activate = FB_ACTIVATE_NOW;
			sVar.yres_virtual = sVar.yres * NUM_PREFERRED_BUFFERS;

			err = fb_set_var(devInfo->ffbInfo, &sVar);
			if (err)
			{
				pr_err("fb_set_var failed (err=%d)\n", err);
			}
		}
		else
		{
			err = fb_pan_display(devInfo->ffbInfo, &sVar);
			if (err)
			{
				pr_err("fb_pan_display failed (err=%d)\n", err);
			}
		}

		console_unlock();
		unlock_fb_info(devInfo->ffbInfo);
	}

	return err;
}

static bool FFBFlipPossible(struct fb_info *ffbInfo)
{
	struct fb_var_screeninfo sVar = ffbInfo->var;
	int err;

	FTRACE();

	if (!ffbInfo->fix.xpanstep && !ffbInfo->fix.ypanstep &&
	    !ffbInfo->fix.ywrapstep)
	{
		pr_err(DRVNAME " - the fbdev device detected does not support ypan/ywrap. "
			   "Flipping disabled.\n");
		return false;
	}

	if ((ffbInfo->fix.line_length * sVar.yres) % PAGE_SIZE != 0)
	{
		pr_err(DRVNAME " - line length (in bytes) x yres is not a multiple of "
			   "page size. Flipping disabled.\n");
		return false;
	}

	/* We might already have enough space */
	if (sVar.yres * NUM_PREFERRED_BUFFERS <= sVar.yres_virtual)
	{
		return true;
	}

	pr_err(DRVNAME " - no buffer space for flipping; asking for more.\n");

	sVar.activate = FB_ACTIVATE_NOW;
	sVar.yres_virtual = sVar.yres * NUM_PREFERRED_BUFFERS;

	err = fb_set_var(ffbInfo, &sVar);
	if (err)
	{
		pr_err(DRVNAME " - fb_set_var failed (err=%d). Flipping disabled.\n", err);
		return false;
	}

	if (sVar.yres * NUM_PREFERRED_BUFFERS > sVar.yres_virtual)
	{
		pr_err(DRVNAME " - failed to obtain additional buffer space. "
			   "Flipping disabled.\n");
		return false;
	}

	/* Some fbdev drivers allow the yres_virtual modification through,
	 * but don't actually update the fix. We need the fix to be updated
	 * and more memory allocated, so we can actually take advantage of
	 * the increased yres_virtual.
	 */
	if (ffbInfo->fix.smem_len < ffbInfo->fix.line_length * sVar.yres_virtual)
	{
		pr_err(DRVNAME " - 'fix' not re-allocated with sufficient buffer space. "
			   "Flipping disabled.\n");
		return false;
	}

	return true;
}

static int FFBInit(PVR_DEVINFO *devInfo)
{
	struct fb_info *ffbInfo;
	uint32_t pixFormat;
	int err = -ENODEV;

	FTRACE();

	ffbInfo = registered_fb[0];
	if (!ffbInfo)
	{
		pr_err(DRVNAME " - no Linux framebuffer (fbdev) device is registered!\n"
			   "Check you have a framebuffer driver compiled into your "
			   "kernel\nand that it is enabled on the cmdline.\n");
		goto err_out;
	}

	if (!lock_fb_info(ffbInfo))
	{
		goto err_out;
	}

	console_lock();

	/* Filter out broken FB devices */
	if (!ffbInfo->fix.smem_len || !ffbInfo->fix.line_length)
	{
		pr_err(DRVNAME " - the fbdev device detected had a zero smem_len or "
			   "line_length,\nwhich suggests it is a broken driver.\n");
		goto err_unlock;
	}

	if (ffbInfo->fix.type != FB_TYPE_PACKED_PIXELS ||
	    ffbInfo->fix.visual != FB_VISUAL_TRUECOLOR)
	{
		pr_err(DRVNAME " - the fbdev device detected is not truecolor with packed "
			   "pixels.\n");
		goto err_unlock;
	}

	if (ffbInfo->var.bits_per_pixel == 32)
	{
		if (ffbInfo->var.red.length   != 8  ||
		    ffbInfo->var.green.length != 8  ||
		    ffbInfo->var.blue.length  != 8  ||
		    ffbInfo->var.red.offset   != 16 ||
		    ffbInfo->var.green.offset != 8  ||
		    ffbInfo->var.blue.offset  != 0)
		{
			pr_err(DRVNAME " - the fbdev device detected uses an unrecognised "
				   "32bit pixel format (%u/%u/%u, %u/%u/%u)\n",
				   ffbInfo->var.red.length,
				   ffbInfo->var.green.length,
				   ffbInfo->var.blue.length,
				   ffbInfo->var.red.offset,
				   ffbInfo->var.green.offset,
				   ffbInfo->var.blue.offset);
			goto err_unlock;
		}
#if !defined(DC_FBDEV_FORCE_XRGB8888)
		if (ffbInfo->var.transp.length == 8)
		{
			pixFormat = DRM_FORMAT_ARGB8888;
		}
		else
#endif
		{
			pixFormat = DRM_FORMAT_XRGB8888;
		}
	}
	else if (ffbInfo->var.bits_per_pixel == 16)
	{
		if (ffbInfo->var.red.length   != 5  ||
		    ffbInfo->var.green.length != 6  ||
		    ffbInfo->var.blue.length  != 5  ||
		    ffbInfo->var.red.offset   != 11 ||
		    ffbInfo->var.green.offset != 5  ||
		    ffbInfo->var.blue.offset  != 0)
		{
			pr_err(DRVNAME " - the fbdev device detected uses an unrecognized "
				   "16bit pixel format (%u/%u/%u, %u/%u/%u)\n",
				   ffbInfo->var.red.length,
				   ffbInfo->var.green.length,
				   ffbInfo->var.blue.length,
				   ffbInfo->var.red.offset,
				   ffbInfo->var.green.offset,
				   ffbInfo->var.blue.offset);
			goto err_unlock;
		}
		pixFormat = DRM_FORMAT_RGB565;
	}
	else
	{
		pr_err(DRVNAME " - the fbdev device detected uses an unsupported "
			   "bpp (%u).\n", ffbInfo->var.bits_per_pixel);
		goto err_unlock;
	}

	if (!try_module_get(ffbInfo->fbops->owner))
	{
		pr_err(DRVNAME " - try_module_get() failed");
		goto err_unlock;
	}

	if (ffbInfo->fbops->fb_open &&
	    ffbInfo->fbops->fb_open(ffbInfo, 0) != 0)
	{
		pr_err(DRVNAME " - fb_open() failed");
		goto err_module_put;
	}

	devInfo->ffbInfo = ffbInfo;
	devInfo->pixFormat = pixFormat;

	devInfo->canFlip = FFBFlipPossible(ffbInfo);

	pr_info("Found fbdev device (%s):\n"
			"range (physical) = 0x%lx-0x%lx\n"
			"size (bytes)     = 0x%x\n"
			"xres x yres      = %ux%u\n"
			"vxres x vyres    = %ux%u\n"
			"DRM pix fmt      = %u\n"
			"flipping?        = %d\n",
			ffbInfo->fix.id,
			ffbInfo->fix.smem_start,
			ffbInfo->fix.smem_start + ffbInfo->fix.smem_len,
			ffbInfo->fix.smem_len,
			ffbInfo->var.xres, ffbInfo->var.yres,
			ffbInfo->var.xres_virtual, ffbInfo->var.yres_virtual,
			pixFormat, devInfo->canFlip);

	err = 0;
err_unlock:
	console_unlock();
	unlock_fb_info(ffbInfo);
err_out:
	return err;
err_module_put:
	module_put(ffbInfo->fbops->owner);
	goto err_unlock;
}

static void FFBCleanup(PVR_DEVINFO *devInfo)
{
	struct fb_info *ffbInfo = devInfo->ffbInfo;

	FTRACE();

	if (lock_fb_info(ffbInfo))
	{
		console_lock();

		if (ffbInfo->fbops->fb_release)
		{
			ffbInfo->fbops->fb_release(ffbInfo, 0);
		}

		module_put(ffbInfo->fbops->owner);

		console_unlock();
		unlock_fb_info(ffbInfo);
	}
}

/******************************************************************************
 * CRTC functions
 ******************************************************************************/

static void CrtcHelperDpms(struct drm_crtc *crtc, int mode)
{
	FTRACE();

	/* Change the power state of the display/pipe/port/etc. If the mode
	   passed in is unsupported, the provider must use the next lowest
	   power level. */
}

static void CrtcHelperPrepare(struct drm_crtc *crtc)
{
	PVR_CRTC *pvrCrtc = to_pvr_crtc(crtc);

	FTRACE();

	drm_vblank_pre_modeset(crtc->dev, pvrCrtc->number);

	/* Prepare the display/pipe/port/etc for a mode change e.g. put them
	   in a low power state/turn them off */
}

static void CrtcHelperCommit(struct drm_crtc *crtc)
{
	PVR_CRTC *pvrCrtc = to_pvr_crtc(crtc);

	FTRACE();

	/* Turn the display/pipe/port/etc back on */

	drm_vblank_post_modeset(crtc->dev, pvrCrtc->number);
}

static bool CrtcHelperModeFixup(struct drm_crtc *crtc,
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,6,0))
				struct drm_display_mode *mode,
#else
				const struct drm_display_mode *mode,
#endif
				struct drm_display_mode *adjusted_mode)
{
	FTRACE();

	/* Fix up mode so that it's compatible with the hardware. The results
	   should be stored in adjusted_mode (i.e. mode should be untouched). */

	return true;
}

static int CrtcHelperModeSetBaseAtomic(struct drm_crtc *crtc,
				       struct drm_framebuffer *fb,
				       int x, int y,
				       enum mode_set_atomic atomic)
{
	PVR_DEVINFO *devInfo = pvr_drm_get_display_device(crtc->dev);
	PVR_FRAMEBUFFER *pvrFramebuffer = to_pvr_framebuffer(fb);

	FTRACE();

	/* Set the display base address or offset from the base address */

	return FFBFlip(devInfo, pvrFramebuffer->dev_base_addr);
}

static int CrtcModeSetBase(struct drm_crtc *crtc, int x, int y,
			   struct drm_framebuffer *old_fb)
{
	FTRACE();

	return CrtcHelperModeSetBaseAtomic(crtc, crtc->fb, x, y,
					   LEAVE_ATOMIC_MODE_SET);
}

static int CrtcHelperModeSet(struct drm_crtc *crtc,
			     struct drm_display_mode *mode,
			     struct drm_display_mode *adjusted_mode,
			     int x, int y, struct drm_framebuffer *old_fb)
{
	FTRACE();

	/* Setup the the new mode and/or framebuffer */
	return CrtcModeSetBase(crtc, x, y, old_fb);
}

static void CrtcHelperLoadLut(struct drm_crtc *crtc)
{
}

static void CrtcDestroy(struct drm_crtc *crtc)
{
	PVR_CRTC *pvrCrtc = to_pvr_crtc(crtc);

	FTRACE();
	if (pvrCrtc->activeFlipData)
	{
		pvr_drm_flip_done(crtc->dev, pvrCrtc->activeFlipData);
		pvrCrtc->activeFlipData = NULL;
	}

	drm_crtc_cleanup(crtc);

	kfree(pvrCrtc);
}

static int CrtcSetConfig(struct drm_mode_set *mode_set)
{
	FTRACE();

	return (mode_set->fb == NULL) ?
		0 :
		drm_crtc_helper_set_config(mode_set);
}

static void DoFlip(struct drm_gem_object *bo, void *data, struct pvr_drm_flip_data *flip_data)
{
	struct drm_crtc		*crtc = (struct drm_crtc *)data;
	struct drm_device	*dev = crtc->dev;
	PVR_CRTC		*pvrCrtc = to_pvr_crtc(crtc);
	PVR_DEVINFO		*devInfo = pvr_drm_get_display_device(dev);
	PVR_FRAMEBUFFER		*pvrFramebuffer = to_pvr_framebuffer(crtc->fb);

	(void)FFBFlip(devInfo, pvrFramebuffer->dev_base_addr);

	/* We can't install an interrupt handler or get vsync information from the fbdev driver.
	   However, FFBFlip will be blocked until the flip has been completed, which takes place
	   during vblank. For this reason report the vblank has been handled and send the event. */
	drm_handle_vblank(dev, pvrCrtc->number);

	pvrCrtc->finishedFlipData = pvrCrtc->activeFlipData;
	pvrCrtc->activeFlipData = flip_data;

	if (pvrCrtc->finishedFlipData)
	{
		pvr_drm_flip_done(dev, pvrCrtc->finishedFlipData);
	}

	if (pvrCrtc->event)
	{
		struct timeval vblanktime;

		pvrCrtc->event->event.sequence = drm_vblank_count_and_time(dev, pvrCrtc->number, &vblanktime);
		pvrCrtc->event->event.tv_sec = vblanktime.tv_sec;
		pvrCrtc->event->event.tv_usec = vblanktime.tv_usec;

		list_add_tail(&pvrCrtc->event->base.link,
			      &pvrCrtc->event->base.file_priv->event_list);
		wake_up_interruptible(&pvrCrtc->event->base.file_priv->event_wait);
	}

	drm_vblank_put(dev, pvrCrtc->number);

	pvrCrtc->event = NULL;
}

#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,12,0))
static int CrtcPageFlip(struct drm_crtc *crtc, struct drm_framebuffer *fb,
			struct drm_pending_vblank_event *event)
#else
static int CrtcPageFlip(struct drm_crtc *crtc, struct drm_framebuffer *fb,
			struct drm_pending_vblank_event *event,
			uint32_t unref__ page_flip_flags)
#endif
{
	struct drm_device	*dev = crtc->dev;
	PVR_CRTC		*pvrCrtc = to_pvr_crtc(crtc);
	PVR_FRAMEBUFFER		*pvrFramebuffer = to_pvr_framebuffer(fb);
	struct drm_framebuffer	*oldFb;
	int			result;

	FTRACE();

	if (pvrCrtc->event)
	{
		return -EBUSY;
	}
	pvrCrtc->event = event;

	/* Set the crtc to point to the new framebuffer */
	oldFb = crtc->fb;
	crtc->fb = fb;

	result = drm_vblank_get(dev, pvrCrtc->number);
	if (result)
	{
		return result;
	}

	/* Schedule a flip */
	result = pvr_drm_flip_schedule(pvrFramebuffer->obj, DoFlip, crtc);
	if (result)
	{
		drm_vblank_put(dev, pvrCrtc->number);
		crtc->fb = oldFb;
	}

	return result;
}

static const struct drm_crtc_helper_funcs sCrtcHelperFuncs =
{
	.dpms			= CrtcHelperDpms,
	.prepare		= CrtcHelperPrepare,
	.commit			= CrtcHelperCommit,
	.mode_fixup		= CrtcHelperModeFixup,
	.mode_set		= CrtcHelperModeSet,
	.mode_set_base		= CrtcModeSetBase,
	.load_lut		= CrtcHelperLoadLut,
	.mode_set_base_atomic	= CrtcHelperModeSetBaseAtomic,
	.disable		= NULL,
};

static const struct drm_crtc_funcs sCrtcFuncs =
{
	.save		= NULL,
	.restore	= NULL,
	.reset		= NULL,
	.cursor_set	= NULL,
	.cursor_move	= NULL,
	.gamma_set	= NULL,
	.destroy	= CrtcDestroy,
	.set_config	= CrtcSetConfig,
	.page_flip	= CrtcPageFlip,
};

static PVR_CRTC *CrtcCreate(PVR_DEVINFO *devInfo, uint32_t num)
{
	PVR_CRTC *pvrCrtc;

	FTRACE();

	pvrCrtc = (PVR_CRTC *)kzalloc(sizeof *pvrCrtc, GFP_KERNEL);
	if (pvrCrtc)
	{
		drm_crtc_init(devInfo->dev, &pvrCrtc->base, &sCrtcFuncs);
		drm_crtc_helper_add(&pvrCrtc->base, &sCrtcHelperFuncs);

		pvrCrtc->number = num;
		pvrCrtc->devInfo = devInfo;

	}

	return pvrCrtc;
}


/******************************************************************************
 * Connector functions
 ******************************************************************************/

static int ConnectorHelperGetModes(struct drm_connector *connector)
{
	PVR_CONNECTOR *pvrConnector = to_pvr_connector(connector);
	PVR_DEVINFO *devInfo = pvrConnector->devInfo;
	struct drm_display_mode *mode = drm_mode_create(connector->dev);

	FTRACE();

	if (!mode)
	{
		return 0;
	}

	mode->type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED;

	mode->clock = 100000;
	mode->hdisplay = devInfo->ffbInfo->var.xres;
	mode->hsync_start = mode->hdisplay;
	mode->hsync_end = mode->hdisplay;
	mode->htotal = mode->hdisplay;
	mode->hskew = 0;

	mode->vdisplay = devInfo->ffbInfo->var.yres;
	mode->vsync_start = mode->vdisplay;
	mode->vsync_end = mode->vdisplay;
	mode->vtotal = mode->vdisplay;
	mode->vscan = 0;

	mode->flags = 0;

	drm_mode_set_name(mode);
	drm_mode_probed_add(connector, mode);

	return 1;
}

static int ConnectorHelperModeValid(struct drm_connector *connector,
				    struct drm_display_mode *mode)
{
	PVR_CONNECTOR *pvrConnector = to_pvr_connector(connector);
	PVR_DEVINFO *devInfo = pvrConnector->devInfo;

	FTRACE();

	if (mode->hdisplay != devInfo->ffbInfo->var.xres)
	{
		return MODE_ONE_WIDTH;
	}

	if (mode->vdisplay != devInfo->ffbInfo->var.yres)
	{
		return MODE_ONE_HEIGHT;
	}

	return MODE_OK;
}

static struct drm_encoder *ConnectorHelperBestEncoder(struct drm_connector *connector)
{
	FTRACE();

	/* Pick the first encoder we find */
	if (connector->encoder_ids[0] != 0)
	{
		struct drm_mode_object *modeObject;

		modeObject = drm_mode_object_find(connector->dev,
						  connector->encoder_ids[0],
						  DRM_MODE_OBJECT_ENCODER);
		if (modeObject)
		{
			return obj_to_encoder(modeObject);
		}
	}

	return NULL;
}

static enum drm_connector_status ConnectorDetect(struct drm_connector *connector, 
						 bool force)
{
	FTRACE();

	/* Return whether or not a monitor is attached to the connector */

	return connector_status_connected;
}

static int ConnectorSetProperty(struct drm_connector *connector,
				struct drm_property *property, uint64_t value)
{
	FTRACE();

	return -ENOSYS;
}

static void ConnectorDestroy(struct drm_connector *connector)
{
	PVR_CONNECTOR *pvrConnector = to_pvr_connector(connector);

	FTRACE();

	drm_connector_cleanup(connector);

	kfree(pvrConnector);
}

static void ConnectorForce(struct drm_connector *connector)
{
	FTRACE();
}

static struct drm_connector_helper_funcs sConnectorHelperFuncs =
{
	.get_modes	= ConnectorHelperGetModes,
	.mode_valid	= ConnectorHelperModeValid,
	.best_encoder	= ConnectorHelperBestEncoder,
};

static const struct drm_connector_funcs sConnectorFuncs =
{
	.dpms		= drm_helper_connector_dpms,
	.save		= NULL,
	.restore	= NULL,
	.reset		= NULL,
	.detect		= ConnectorDetect,
	.fill_modes	= drm_helper_probe_single_connector_modes,
	.set_property	= ConnectorSetProperty,
	.destroy	= ConnectorDestroy,
	.force		= ConnectorForce,
};

static PVR_CONNECTOR *ConnectorCreate(PVR_DEVINFO *devInfo, int type)
{
	PVR_CONNECTOR *pvrConnector;

	FTRACE();

	pvrConnector = (PVR_CONNECTOR *)kzalloc(sizeof *pvrConnector, GFP_KERNEL);
	if (pvrConnector)
	{
		drm_connector_init(devInfo->dev, &pvrConnector->base, &sConnectorFuncs, type);
		drm_connector_helper_add(&pvrConnector->base, &sConnectorHelperFuncs);

		pvrConnector->base.dpms = DRM_MODE_DPMS_OFF;
		pvrConnector->base.interlace_allowed = false;
		pvrConnector->base.doublescan_allowed = false;
		pvrConnector->base.display_info.subpixel_order = SubPixelUnknown;

		pvrConnector->devInfo = devInfo;
	}

	return pvrConnector;
}


/******************************************************************************
 * Encoder functions
 ******************************************************************************/

static void EncoderHelperDpms(struct drm_encoder *encoder, int mode)
{
	FTRACE();

	/* Set the display power state or active encoder based on the mode. If
	   the mode passed in is unsupported, the provider must use the next
	   lowest power level. */
}

static bool EncoderHelperModeFixup(struct drm_encoder *encoder,
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,6,0))
				   struct drm_display_mode *mode,
#else
				   const struct drm_display_mode *mode,
#endif
				   struct drm_display_mode *adjusted_mode)
{
	FTRACE();

	/* Fix up mode so that it's compatible with the hardware. The results
	   should be stored in adjusted_mode (i.e. mode should be untouched). */

	return true;
}

static void EncoderHelperPrepare(struct drm_encoder *encoder)
{
	FTRACE();

	/* Prepare the encoder for a mode change e.g. set the active encoder
	   accordingly/turn the encoder off */
}

static void EncoderHelperCommit(struct drm_encoder *encoder)
{
	FTRACE();

	/* Turn the encoder back on/set the active encoder */
}

static void EncoderHelperModeSet(struct drm_encoder *encoder,
				 struct drm_display_mode *mode,
				 struct drm_display_mode *adjusted_mode)
{
	FTRACE();

	/* Setup the encoder for the new mode */
}

static void EncoderDestroy(struct drm_encoder *encoder)
{
	PVR_ENCODER *pvrEncoder = to_pvr_encoder(encoder);

	FTRACE();

	drm_encoder_cleanup(encoder);

	kfree(pvrEncoder);
}

static const struct drm_encoder_helper_funcs sEncoderHelperFuncs =
{
	.dpms		= EncoderHelperDpms,
	.save		= NULL,
	.restore	= NULL,
	.mode_fixup	= EncoderHelperModeFixup,
	.prepare	= EncoderHelperPrepare,
	.commit		= EncoderHelperCommit,
	.mode_set	= EncoderHelperModeSet,
	.get_crtc	= NULL,
	.detect		= NULL,
	.disable	= NULL,
};

static const struct drm_encoder_funcs sEncoderFuncs =
{
	.reset		= NULL,
	.destroy	= EncoderDestroy,
};

static PVR_ENCODER *EncoderCreate(PVR_DEVINFO *devInfo, int type)
{
	PVR_ENCODER *pvrEncoder;

	FTRACE();

	pvrEncoder = (PVR_ENCODER *)kzalloc(sizeof *pvrEncoder, GFP_KERNEL);
	if (pvrEncoder)
	{
		drm_encoder_init(devInfo->dev, &pvrEncoder->base, &sEncoderFuncs, type);
		drm_encoder_helper_add(&pvrEncoder->base, &sEncoderHelperFuncs);

		pvrEncoder->devInfo = devInfo;

		/* This is a bit field that's used to determine which
		   CRTCs can drive this encoder. */
		pvrEncoder->base.possible_crtcs = 0x1;
	}

	return pvrEncoder;
}


/******************************************************************************
 * Framebuffer functions
 ******************************************************************************/

static void FramebufferDestroy(struct drm_framebuffer *framebuffer)
{
	PVR_FRAMEBUFFER *pvrFramebuffer = to_pvr_framebuffer(framebuffer);

	FTRACE();

	drm_framebuffer_cleanup(framebuffer);

	BUG_ON(!pvrFramebuffer->obj);

	pvr_drm_gem_unmap(pvrFramebuffer->obj);

	drm_gem_object_unreference_unlocked(pvrFramebuffer->obj);

	kfree(pvrFramebuffer);
}

static int FramebufferCreateHandle(struct drm_framebuffer *framebuffer,
				   struct drm_file *file_priv,
				   unsigned int *handle)
{
	PVR_FRAMEBUFFER *pvrFramebuffer = to_pvr_framebuffer(framebuffer);

	FTRACE();

	return drm_gem_handle_create(file_priv, pvrFramebuffer->obj, handle);
}

static int FramebufferDirty(struct drm_framebuffer *framebuffer,
			    struct drm_file *file_priv, unsigned flags,
			    unsigned color, struct drm_clip_rect *clips,
			    unsigned num_clips)
{
	FTRACE();

	return -ENOSYS;
}

static const struct drm_framebuffer_funcs sFramebufferFuncs =
{
	.destroy	= FramebufferDestroy,
	.create_handle	= FramebufferCreateHandle,
	.dirty		= FramebufferDirty,
};

static int FramebufferInit(struct drm_device *dev, struct drm_mode_fb_cmd2 *mode_cmd, 
			   PVR_FRAMEBUFFER *pvrFramebuffer, struct drm_gem_object *obj)
{
	int result;

	FTRACE();

	result = drm_framebuffer_init(dev, &pvrFramebuffer->base, &sFramebufferFuncs);
	if (result)
	{
		pr_err(DRVNAME " - %s: failed to initialise framebuffer structure (%d)\n", 
		       __FUNCTION__, result);

		return result;
	}

	result = drm_helper_mode_fill_fb_struct(&pvrFramebuffer->base, mode_cmd);
	if (result)
	{
		pr_err(DRVNAME " - %s: failed to fill in framebuffer mode information (%d)\n", 
		       __FUNCTION__, result);

		return result;
	}

	pvrFramebuffer->obj = obj;

	result = pvr_drm_gem_map(pvrFramebuffer->obj);
	if (result)
	{
		return result;
	}

	result = pvr_drm_gem_dev_addr(pvrFramebuffer->obj, 0, &pvrFramebuffer->dev_base_addr);
	if (result)
	{
		pvr_drm_gem_unmap(pvrFramebuffer->obj);
		return result;
	}

	return 0;
}

static struct drm_framebuffer *FbCreate(struct drm_device *dev, struct drm_file *file_priv, struct drm_mode_fb_cmd2 *mode_cmd)
{
	struct drm_gem_object *obj = NULL;
	PVR_FRAMEBUFFER	*pvrFramebuffer = NULL;
	int result = 0;

	FTRACE();

	obj = drm_gem_object_lookup(dev, file_priv, mode_cmd->handles[0]);
	if (!obj)
	{
		pr_err(DRVNAME " - %s: failed to find buffer with handle %u\n",
		       __FUNCTION__, mode_cmd->handles[0]);

		goto exit;
	}

	pvrFramebuffer = kzalloc(sizeof *pvrFramebuffer, GFP_KERNEL);
	if (!pvrFramebuffer)
	{
		goto exit;
	}

	result = FramebufferInit(dev, mode_cmd, pvrFramebuffer, obj);
	if (result)
	{
		goto exit;
	}

	return &pvrFramebuffer->base;

exit:
	if (!obj)
	{
		return ERR_PTR(-ENOENT);
	}

	drm_gem_object_unreference_unlocked(obj);

	if (!pvrFramebuffer)
	{
		return ERR_PTR(-ENOMEM);
	}

	BUG_ON(!result);

	kfree(pvrFramebuffer);

	return ERR_PTR(result);
}

static const struct drm_mode_config_funcs sModeConfigFuncs =
{
	.fb_create = FbCreate,
	.output_poll_changed = NULL,
};


int dc_drmfbdev_init(struct drm_device *dev, void **display_priv_out)
{
	PVR_DEVINFO	*devInfo;
	int		result;

	FTRACE();

	if (dev == NULL || display_priv_out == NULL)
	{
		return -EINVAL;
	}

	devInfo = kzalloc(sizeof *devInfo, GFP_KERNEL);
	if (!devInfo)
	{
		pr_err(DRVNAME " - %s: failed to allocate the device info\n",
		       __FUNCTION__);

		return -ENOMEM;
	}
	devInfo->dev = dev;

	result = FFBInit(devInfo);
	if (result)
	{
		goto ErrorFreeDevInfo;
	}

	*display_priv_out = devInfo;

	return 0;

ErrorFreeDevInfo:
	kfree(devInfo);

	return result;
}
EXPORT_SYMBOL(dc_drmfbdev_init);

int dc_drmfbdev_configure(void *display_priv)
{
	PVR_DEVINFO		*devInfo = display_priv;
	struct drm_device	*dev;
	PVR_CRTC		*pvrCrtc;
	PVR_CONNECTOR		*pvrConnector;
	PVR_ENCODER		*pvrEncoder;
	int			result;

	FTRACE();

	if (display_priv == NULL)
	{
		return -EINVAL;
	}
	dev = devInfo->dev;

	result = drm_vblank_init(dev, 1);
	if (result)
	{
		return result;
	}

	drm_mode_config_init(dev);

	dev->mode_config.funcs		= (void *)&sModeConfigFuncs;
	dev->mode_config.min_width	= devInfo->ffbInfo->var.xres;
	dev->mode_config.max_width	= devInfo->ffbInfo->var.xres;
	dev->mode_config.min_height	= devInfo->ffbInfo->var.yres;
	dev->mode_config.max_height	= devInfo->ffbInfo->var.yres;
	dev->mode_config.fb_base	= 0;

	pvrCrtc = CrtcCreate(devInfo, 0);
	if (!pvrCrtc)
	{
		pr_err(DRVNAME " - %s: failed to create a CRTC.\n", __FUNCTION__);

		result = -ENOMEM;
		goto ErrorVBlankCleanup;
	}

	pvrConnector = ConnectorCreate(devInfo, DRM_MODE_CONNECTOR_VIRTUAL);
	if (!pvrConnector)
	{
		pr_err(DRVNAME " - %s: failed to create a connector.\n", __FUNCTION__);

		result = -ENOMEM;
		goto ErrorConfigCleanup;
	}

	pvrEncoder = EncoderCreate(devInfo, DRM_MODE_ENCODER_VIRTUAL);
	if (!pvrEncoder)
	{
		pr_err(DRVNAME " - %s: failed to create an encoder.\n", __FUNCTION__);

		result = -ENOMEM;
		goto ErrorConfigCleanup;
	}

	/*
	 * It seems we don't always get a valid CRTC configuration.
	 * To work around this, we set encoder->crtc and connector->encoder
	 * here.
	 * A better solution may be to call drm_mode_setcrtc via the
	 * DRM_IOCTL_MODE_SETCRTC ioctl from user space (e.g. via
	 * drmModeSetCrtc), as this will call drm_crtc_set_config.
	 * For example, if the X Server notices there isn't a valid
	 * configuration, it could set one up, by calling drmModeSetCrtc.
	 */
	pvrEncoder->base.crtc = &pvrCrtc->base;
	pvrConnector->base.encoder = &pvrEncoder->base;
	drm_mode_connector_attach_encoder(&pvrConnector->base, &pvrEncoder->base);

	return 0;

ErrorConfigCleanup:
	drm_mode_config_cleanup(dev);

ErrorVBlankCleanup:
	drm_vblank_cleanup(dev);

	return result;
}
EXPORT_SYMBOL(dc_drmfbdev_configure);

void dc_drmfbdev_cleanup(void *display_priv)
{
	PVR_DEVINFO *devInfo = display_priv;

	FTRACE();

	if (devInfo)
	{
		drm_mode_config_cleanup(devInfo->dev);

		(void) FFBFlip(devInfo, devInfo->ffbInfo->fix.smem_start);

		drm_vblank_cleanup(devInfo->dev);

		FFBCleanup(devInfo);

		kfree(devInfo);
	}
}
EXPORT_SYMBOL(dc_drmfbdev_cleanup);

static void display_buffer_destroy(struct kref *kref)
{
	struct pvr_drm_display_buffer *buffer = container_of(kref, struct pvr_drm_display_buffer, refcount);

	FTRACE();

	BUG_ON(buffer->bufferNum >= NUM_PREFERRED_BUFFERS);

	FFBFreeBufferMem(buffer->bufferNum);

	kfree(buffer);
}

static inline void display_buffer_ref(struct pvr_drm_display_buffer *buffer)
{
	FTRACE();

	kref_get(&buffer->refcount);
}

static inline void display_buffer_unref(struct pvr_drm_display_buffer *buffer)
{
	FTRACE();

	kref_put(&buffer->refcount, display_buffer_destroy);
}

int dc_drmfbdev_buffer_alloc(void *display_priv, size_t size, struct pvr_drm_display_buffer **buffer_out)
{
	PVR_DEVINFO *devInfo = display_priv;
	struct fb_var_screeninfo sVar = devInfo->ffbInfo->var;
	PVR_BUFFER *buffer;

	FTRACE();

	if (!display_priv || !buffer_out)
	{
		return -EINVAL;
	}

	if ((devInfo->ffbInfo->fix.line_length * sVar.yres) != size)
	{
		pr_err(DRVNAME " - %s: cannot allocate buffer due to invalid size (expected %llu but got %zu)\n",
		       __FUNCTION__, (unsigned long long)(devInfo->ffbInfo->fix.line_length * sVar.yres), size);
		return -EINVAL;
	}

	buffer = kmalloc(sizeof *buffer, GFP_KERNEL);
	if (!buffer)
	{
		pr_err(DRVNAME " - %s: failed to allocate buffer\n", __FUNCTION__);
		return -ENOMEM;
	}

	buffer->bufferNum = FFBAllocBufferMem();
	if (buffer->bufferNum < 0)
	{
		pr_err(DRVNAME " - %s: failed to allocate buffer backing memory\n", __FUNCTION__);

		kfree(buffer);
		return -ENOMEM;
	}

	kref_init(&buffer->refcount);
	buffer->devInfo = devInfo;
	buffer->size = size;

	*buffer_out = buffer;

	return 0;
}
EXPORT_SYMBOL(dc_drmfbdev_buffer_alloc);

int dc_drmfbdev_buffer_free(struct pvr_drm_display_buffer *buffer)
{
	FTRACE();

	if (!buffer)
	{
		return -EINVAL;
	}

	display_buffer_unref(buffer);

	return 0;
}
EXPORT_SYMBOL(dc_drmfbdev_buffer_free);

uint64_t *dc_drmfbdev_buffer_acquire(struct pvr_drm_display_buffer *buffer)
{
	uint64_t *addr_array;
	uint64_t addr;
	off_t offset;

	FTRACE();

	if (!buffer)
	{
		return ERR_PTR(-EINVAL);
	}
	BUG_ON(buffer->bufferNum >= NUM_PREFERRED_BUFFERS);

	display_buffer_ref(buffer);

	addr_array = kmalloc(sizeof *addr_array * (buffer->size >> PAGE_SHIFT), GFP_KERNEL);
	if (!addr_array)
	{
		display_buffer_unref(buffer);
		return ERR_PTR(-ENOMEM);
	}

	addr = buffer->devInfo->ffbInfo->fix.smem_start + (buffer->bufferNum * buffer->size);

	for (offset = 0; offset < buffer->size; offset += PAGE_SIZE)
	{
		addr_array[offset >> PAGE_SHIFT] = addr + offset;
	}

	return addr_array;
}
EXPORT_SYMBOL(dc_drmfbdev_buffer_acquire);

int dc_drmfbdev_buffer_release(struct pvr_drm_display_buffer *buffer, uint64_t *dev_paddr_array)
{
	FTRACE();

	if (!buffer || !dev_paddr_array)
	{
		return -EINVAL;
	}

	display_buffer_unref(buffer);

	kfree(dev_paddr_array);

	return 0;
}
EXPORT_SYMBOL(dc_drmfbdev_buffer_release);

void *dc_drmfbdev_buffer_vmap(struct pvr_drm_display_buffer *buffer)
{
	FTRACE();

	return NULL;
}
EXPORT_SYMBOL(dc_drmfbdev_buffer_vmap);

void dc_drmfbdev_buffer_vunmap(struct pvr_drm_display_buffer *buffer, void *vaddr)
{
	FTRACE();
}
EXPORT_SYMBOL(dc_drmfbdev_buffer_vunmap);

u32 dc_drmfbdev_get_vblank_counter(void *display_priv, int crtc)
{
	WARN_ON(!display_priv);
	WARN_ON(crtc != 0);

	return 0;
}
EXPORT_SYMBOL(dc_drmfbdev_get_vblank_counter);

int dc_drmfbdev_enable_vblank(void *display_priv, int crtc)
{
	WARN_ON(!display_priv);
	WARN_ON(crtc != 0);

	return 0;
}
EXPORT_SYMBOL(dc_drmfbdev_enable_vblank);

int dc_drmfbdev_disable_vblank(void *display_priv, int crtc)
{
	WARN_ON(!display_priv);
	WARN_ON(crtc != 0);

	return 0;
}
EXPORT_SYMBOL(dc_drmfbdev_disable_vblank);

static int __init fbdev_init(void)
{
	FTRACE();

	return 0;
}

static void __exit fbdev_exit(void)
{
	FTRACE();
}

module_init(fbdev_init);
module_exit(fbdev_exit);

#else
#error LMA unsupported
#endif /* !defined(LMA) */
