/*************************************************************************/ /*!
@File
@Title          PowerVR DRM display interface
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@License        Dual MIT/GPLv2

The contents of this file are subject to the MIT license as set out below.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

Alternatively, the contents of this file may be used under the terms of
the GNU General Public License Version 2 ("GPL") in which case the provisions
of GPL are applicable instead of those above.

If you wish to allow use of your version of this file only under the terms of
GPL, and not to allow others to use your version of this file under the terms
of the MIT license, indicate your decision by deleting the provisions above
and replace them with the notice and other provisions required by GPL as set
out in the file called "GPL-COPYING" included in this distribution. If you do
not delete the provisions above, a recipient may use your version of this file
under the terms of either the MIT license or GPL.

This License is also included in this distribution in the file called
"MIT-COPYING".

EXCEPT AS OTHERWISE STATED IN A NEGOTIATED AGREEMENT: (A) THE SOFTWARE IS
PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING
BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
PURPOSE AND NONINFRINGEMENT; AND (B) IN NO EVENT SHALL THE AUTHORS OR
COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/ /**************************************************************************/

#include <linux/string.h>

#include "pvr_drm.h"
#include "syscommon.h"
#include "pvrsrv.h"
#include "pdump_physmem.h"

#if defined(PVR_RI_DEBUG)
#include "ri_server.h"
#endif

#define PVR_DRM_DISPLAY_IRQ		1
#define PVR_DRM_FLIP_SCP_SIZE_LOG2	12

struct pvr_drm_flip_ready_data
{
	struct pvr_drm_gem_object	*pvr_obj;

	pvr_drm_flip_func		flip_cb;
	void				*data;
};

struct pvr_drm_flip_data
{
	struct drm_device		*dev;
	struct pvr_drm_gem_object	*pvr_obj;

	struct list_head		flip_done_entry;
};

static const IMG_CHAR AllocPoison[] = "^PoIsOn";
static const IMG_CHAR FreePoison[] = "<DEAD-BEEF>";

static void Poison(void *pvKernAddr,
		    IMG_DEVMEM_SIZE_T uiBufferSize,
		    const IMG_CHAR *pacPoisonData,
		    IMG_SIZE_T uiPoisonSize)
{
	IMG_CHAR *pcDest = pvKernAddr;
	IMG_UINT32 uiSrcByteIndex = 0;
	IMG_DEVMEM_SIZE_T uiDestByteIndex;

	for (uiDestByteIndex = 0; uiDestByteIndex < uiBufferSize; uiDestByteIndex++)
	{
		pcDest[uiDestByteIndex] = pacPoisonData[uiSrcByteIndex];
		uiSrcByteIndex++;
		if (uiSrcByteIndex == uiPoisonSize)
		{
			uiSrcByteIndex = 0;
		}
	}
}

/*************************************************************************/ /*!
* DRM GEM Display PMR factory
*/ /**************************************************************************/

typedef struct PMR_GEM_DISPLAY_PRIV_TAG
{
	PHYS_HEAP			*psHeap;
	size_t				uiAllocSize;
	struct pvr_drm_display_buffer	*psBuffer;
	uint64_t			*pauiDevPhysAddr;
	IMG_BOOL			bPoisonOnFree;
	IMG_HANDLE			hPDumpAllocInfo;
} PMR_GEM_DISPLAY_PRIV;


static PVRSRV_ERROR PMRGEMDisplayLockPhysAddress(PMR_IMPL_PRIVDATA pvPriv,
						 IMG_UINT32 uiLog2DevPageSize)
{
	PMR_GEM_DISPLAY_PRIV *psGEMDisplayPriv = pvPriv;

	psGEMDisplayPriv->pauiDevPhysAddr = PVR_DRM_MAKENAME(DISPLAY_CONTROLLER, _buffer_acquire)(psGEMDisplayPriv->psBuffer);
	if (IS_ERR(psGEMDisplayPriv->pauiDevPhysAddr))
	{
		return PVRSRV_ERROR_OUT_OF_MEMORY;
	}

	return PVRSRV_OK;
}

static PVRSRV_ERROR PMRGEMDisplayUnlockPhysAddress(PMR_IMPL_PRIVDATA pvPriv)
{
	PMR_GEM_DISPLAY_PRIV *psGEMDisplayPriv = pvPriv;

	(void)PVR_DRM_MAKENAME(DISPLAY_CONTROLLER, _buffer_release)(psGEMDisplayPriv->psBuffer,
								    psGEMDisplayPriv->pauiDevPhysAddr);

	return PVRSRV_OK;
}

static PVRSRV_ERROR PMRGEMDisplayDevPhysAddr(PMR_IMPL_PRIVDATA pvPriv,
					     IMG_DEVMEM_OFFSET_T uiOffset,
					     IMG_DEV_PHYADDR *psDevAddrPtr)
{
	PMR_GEM_DISPLAY_PRIV *psGEMDisplayPriv = pvPriv;
	size_t uiPageNum;
	off_t uiInPageOffset;

	PVR_ASSERT(uiOffset < psGEMDisplayPriv->uiAllocSize);

	uiPageNum	= uiOffset >> PAGE_SHIFT;
	uiInPageOffset	= offset_in_page(uiOffset);

	psDevAddrPtr->uiAddr = psGEMDisplayPriv->pauiDevPhysAddr[uiPageNum];
	psDevAddrPtr->uiAddr += uiInPageOffset;

	return PVRSRV_OK;
}

static PVRSRV_ERROR PMRGEMDisplayAcquireKernelMappingData(PMR_IMPL_PRIVDATA pvPriv,
							  IMG_SIZE_T uiOffset,
							  IMG_SIZE_T uiSize,
							  void **ppvKernelAddressOut,
							  IMG_HANDLE *phHandleOut,
							  PMR_FLAGS_T ulFlags)
{
	PMR_GEM_DISPLAY_PRIV *psGEMDisplayPriv = pvPriv;
	void *pvCPUVirtAddr;

	pvCPUVirtAddr = PVR_DRM_MAKENAME(DISPLAY_CONTROLLER, _buffer_vmap)(psGEMDisplayPriv->psBuffer);
	if (IS_ERR_OR_NULL(pvCPUVirtAddr))
	{
		return PVRSRV_ERROR_OUT_OF_MEMORY;
	}

	*phHandleOut = pvCPUVirtAddr;
	*ppvKernelAddressOut = pvCPUVirtAddr + uiOffset;

	return PVRSRV_OK;
}

static void PMRGEMDisplayReleaseKernelMappingData(PMR_IMPL_PRIVDATA pvPriv,
						  IMG_HANDLE hHandle)
{
	PMR_GEM_DISPLAY_PRIV *psGEMDisplayPriv = pvPriv;
	void *pvCPUVirtAddr = hHandle;

	PVR_DRM_MAKENAME(DISPLAY_CONTROLLER, _buffer_vunmap)(psGEMDisplayPriv->psBuffer,
							     pvCPUVirtAddr);
}

static PVRSRV_ERROR PMRGEMDisplayFinalize(PMR_IMPL_PRIVDATA pvPriv)
{
	PMR_GEM_DISPLAY_PRIV *psGEMDisplayPriv = pvPriv;

	if (psGEMDisplayPriv->bPoisonOnFree)
	{
		void *pvCPUVirtAddr;

		pvCPUVirtAddr = PVR_DRM_MAKENAME(DISPLAY_CONTROLLER, _buffer_vmap)(psGEMDisplayPriv->psBuffer);
		if (IS_ERR_OR_NULL(pvCPUVirtAddr))
		{
			PVR_DPF((PVR_DBG_ERROR, "%s: Failed to poison display memory on free", __FUNCTION__));
		}
		else
		{
			Poison(pvCPUVirtAddr, psGEMDisplayPriv->uiAllocSize, FreePoison, ARRAY_SIZE(FreePoison));

			PVR_DRM_MAKENAME(DISPLAY_CONTROLLER, _buffer_vunmap)(psGEMDisplayPriv->psBuffer, pvCPUVirtAddr);
		}
	}

	PVR_DRM_MAKENAME(DISPLAY_CONTROLLER, _buffer_free)(psGEMDisplayPriv->psBuffer);

	if (psGEMDisplayPriv->hPDumpAllocInfo)
	{
	        PDumpPMRFree(psGEMDisplayPriv->hPDumpAllocInfo);
	}

	kfree(psGEMDisplayPriv);

	return PVRSRV_OK;
}

static const PMR_IMPL_FUNCTAB gsPMRGEMDisplayFuncTab = 
{
	.pfnLockPhysAddresses		= PMRGEMDisplayLockPhysAddress,
	.pfnUnlockPhysAddresses		= PMRGEMDisplayUnlockPhysAddress,
	.pfnDevPhysAddr			= PMRGEMDisplayDevPhysAddr,
	.pfnAcquireKernelMappingData	= PMRGEMDisplayAcquireKernelMappingData,
	.pfnReleaseKernelMappingData	= PMRGEMDisplayReleaseKernelMappingData,
	.pfnFinalize			= PMRGEMDisplayFinalize,
};

PVRSRV_ERROR PVRSRVDRMDisplayCreatePMR(PVRSRV_DEVICE_NODE *psDevNode,
				       struct drm_device *dev,
				       size_t size,
				       PVRSRV_MEMALLOCFLAGS_T uiFlags,
				       PMR **ppsPMR)
{
	struct pvr_drm_dev_priv *psDevPriv = (struct pvr_drm_dev_priv *)dev->dev_private;
	IMG_BOOL bMappingTable = IMG_TRUE;
	PMR_GEM_DISPLAY_PRIV *psGEMDisplayPriv;
	IMG_BOOL bZeroOnAlloc;
	IMG_BOOL bPoisonOnAlloc;
	IMG_BOOL bPoisonOnFree;
	PVRSRV_ERROR eError;
	int iErr;

	bZeroOnAlloc = (uiFlags & PVRSRV_MEMALLOCFLAG_ZERO_ON_ALLOC) ? IMG_TRUE : IMG_FALSE;
	bPoisonOnAlloc = (uiFlags & PVRSRV_MEMALLOCFLAG_POISON_ON_ALLOC) ? IMG_TRUE : IMG_FALSE;
	bPoisonOnFree = (uiFlags & PVRSRV_MEMALLOCFLAG_POISON_ON_FREE) ? IMG_TRUE : IMG_FALSE;

	if (bZeroOnAlloc && bPoisonOnAlloc)
	{
		/* Zero on Alloc and Poison on Alloc are mutually exclusive */
		return PVRSRV_ERROR_INVALID_PARAMS;
	}

	/* Create the private data structure for the PMR */
	psGEMDisplayPriv = kzalloc(sizeof *psGEMDisplayPriv, GFP_KERNEL);
	if (psGEMDisplayPriv == NULL)
	{
		return PVRSRV_ERROR_OUT_OF_MEMORY;
	}

	iErr = PVR_DRM_MAKENAME(DISPLAY_CONTROLLER, _buffer_alloc)(psDevPriv->display_priv,
								   size,
								   &psGEMDisplayPriv->psBuffer);
	if (iErr)
	{
		PVR_DPF((PVR_DBG_ERROR,
			 "%s: Failed to allocate display memory (error %d)",
			 __FUNCTION__, iErr));
		eError = PVRSRV_ERROR_OUT_OF_MEMORY;
		goto ErrorFreeDisplayPriv;
	}

	if (bZeroOnAlloc || bPoisonOnAlloc)
	{
		void *pvCPUVirtAddr;

		pvCPUVirtAddr = PVR_DRM_MAKENAME(DISPLAY_CONTROLLER, _buffer_vmap)(psGEMDisplayPriv->psBuffer);
		if (IS_ERR_OR_NULL(pvCPUVirtAddr))
		{
			PVR_DPF((PVR_DBG_ERROR, "%s: Failed to get display memory kernel mapping", __FUNCTION__));
			eError = PVRSRV_ERROR_PMR_NO_KERNEL_MAPPING;
			//goto ErrorDisplayFree;
			goto DidNotZeroInit;
		}

		if (bZeroOnAlloc)
		{
			memset(pvCPUVirtAddr, 0, size);
		}
		else
		{
			Poison(pvCPUVirtAddr, size, AllocPoison, ARRAY_SIZE(AllocPoison));
		}

		PVR_DRM_MAKENAME(DISPLAY_CONTROLLER, _buffer_vunmap)(psGEMDisplayPriv->psBuffer, pvCPUVirtAddr);
	}
	DidNotZeroInit:
	psGEMDisplayPriv->psHeap = psDevNode->apsPhysHeap[PVR_DRM_PHYS_HEAP];
	psGEMDisplayPriv->uiAllocSize = size;
	psGEMDisplayPriv->bPoisonOnFree = bPoisonOnFree;

	eError = PMRCreatePMR(psGEMDisplayPriv->psHeap,
			      size,
			      size,
			      1,
			      1,
			      &bMappingTable,
			      PAGE_SHIFT,
			      uiFlags,
			      "PMRGEMDISPLAY",
			      &gsPMRGEMDisplayFuncTab,
			      psGEMDisplayPriv,
			      ppsPMR,
			      &psGEMDisplayPriv->hPDumpAllocInfo,
			      IMG_TRUE);
	if (eError != PVRSRV_OK)
	{
		goto ErrorDisplayFree;
	}

#if defined(PVR_RI_DEBUG)
	eError = RIWritePMREntryKM(*ppsPMR,
				   sizeof("GEM DISPLAY"),
				   "GEM DISPLAY",
				   size);
	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_WARNING,
			 "%s: Failed to write PMR entry (%s)",
			 __FUNCTION__, PVRSRVGetErrorStringKM(eError)));
	}
#endif

	return PVRSRV_OK;

ErrorDisplayFree:
	PVR_DRM_MAKENAME(DISPLAY_CONTROLLER, _buffer_free)(psGEMDisplayPriv->psBuffer);

ErrorFreeDisplayPriv:
	kfree(psGEMDisplayPriv);

	return eError;
}


/*************************************************************************/ /*!
* DRM Display callbacks
*/ /**************************************************************************/

static void *PVRSRVGetDisplayDevice(struct drm_device *dev)
{
	if (dev != NULL)
	{
		struct pvr_drm_dev_priv *psDevPriv = (struct pvr_drm_dev_priv *)dev->dev_private;

		return psDevPriv->display_priv;
	}

	return NULL;
}

#if defined(SUPPORT_SYSTEM_INTERRUPT_HANDLING)
static int PVRSRVDisplayIRQInstall(struct drm_device *dev,
				   unsigned int irq,
				   bool shared,
				   pvr_drm_irq_handler handler,
				   void **irq_handle_out)
{
	if (dev != NULL && handler != NULL && irq_handle_out != NULL)
	{
		struct pvr_drm_dev_priv *psDevPriv = (struct pvr_drm_dev_priv *)dev->dev_private;
		IMG_HANDLE hLISRData;
		PVRSRV_ERROR eError;

		PVR_ASSERT(irq == PVR_DRM_DISPLAY_IRQ);

		eError = SysInstallDeviceLISR(irq,
					      shared ? IMG_TRUE : IMG_FALSE,
					      TOSTRING(DISPLAY_CONTROLLER),
					      (PFN_LISR)handler,
					      psDevPriv->display_priv,
					      &hLISRData);
		if (eError != PVRSRV_OK)
		{
			if (eError == PVRSRV_ERROR_ISR_ALREADY_INSTALLED)
			{
				return -EEXIST;
			}
			else if (eError == PVRSRV_ERROR_UNABLE_TO_INSTALL_ISR)
			{
				return -ENOENT;
			}
			return -EINVAL;
		}

		*irq_handle_out = (void *)hLISRData;

		return 0;
	}

	return -EINVAL;
}

static int PVRSRVDisplayIRQUninstall(void *irq_handle)
{
	if (irq_handle != NULL)
	{
		PVRSRV_ERROR eError;

		eError = SysUninstallDeviceLISR((IMG_HANDLE)irq_handle);
		if (eError == PVRSRV_OK)
		{
			return 0;
		}
	}

	return -EINVAL;
}
#else
static int PVRSRVDisplayIRQInstall(struct drm_device *dev,
				   unsigned int irq,
				   bool shared,
				   pvr_drm_irq_handler handler,
				   void **irq_handle_out)
{
	struct pvr_drm_dev_priv *psDevPriv = (struct pvr_drm_dev_priv *)dev->dev_private;
	int iRet;

	mutex_lock(&dev->struct_mutex);

	if (dev->irq_enabled)
	{
		mutex_unlock(&dev->struct_mutex);
		return -EEXIST;
	}

	psDevPriv->irq = irq;
	psDevPriv->irq_handler = handler;
	dev->irq_enabled = 1;

	mutex_unlock(&dev->struct_mutex);

	iRet = request_irq(irq,
			   dev->driver->irq_handler,
			   shared ? IRQF_SHARED : 0,
			   TOSTRING(DISPLAY_CONTROLLER),
			   dev);
	if (iRet != 0)
	{
		mutex_lock(&dev->struct_mutex);
		psDevPriv->irq = 0;
		psDevPriv->irq_handler = NULL;
		dev->irq_enabled = 0;
		mutex_unlock(&dev->struct_mutex);

		return iRet;
	}

	*irq_handle_out = dev;

	return 0;
}

static int PVRSRVDisplayIRQUninstall(void *irq_handle)
{
	if (irq_handle)
	{
		struct drm_device *dev = (struct drm_device *)irq_handle;
		struct pvr_drm_dev_priv *psDevPriv = (struct pvr_drm_dev_priv *)dev->dev_private;
		int irq_enabled;
		unsigned int irq;

		mutex_lock(&dev->struct_mutex);

		irq_enabled = dev->irq_enabled;
		dev->irq_enabled = 0;

		irq = psDevPriv->irq;
		psDevPriv->irq = 0;
		psDevPriv->irq_handler = NULL;

		mutex_unlock(&dev->struct_mutex);

		if (irq_enabled)
		{
			unsigned long irq_flags;
			int i;

			spin_lock_irqsave(&dev->vbl_lock, irq_flags);
			for (i = 0; i < dev->num_crtcs; i++)
			{
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,13,0)) && ! defined(CHROMEOS_WORKAROUNDS)
				DRM_WAKEUP(&dev->vbl_queue[i]);
				dev->last_vblank[i] = dev->driver->get_vblank_counter(dev, i);
				dev->vblank_enabled[i] = 0;
#else
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,14,0))
				DRM_WAKEUP(&dev->vblank[i].queue);
#else
				wake_up(&dev->vblank[i].queue);
#endif
				dev->vblank[i].enabled = false;
				dev->vblank[i].last = dev->driver->get_vblank_counter(dev, i);
#endif
			}
			spin_unlock_irqrestore(&dev->vbl_lock, irq_flags);

			free_irq(irq, dev);

			return 0;
		}
	}

	return -EINVAL;
}
#endif /* defined(SUPPORT_SYSTEM_INTERRUPT_HANDLING) */

static int PVRSRVGEMCreate(struct drm_device *dev,
			   size_t size,
			   struct drm_gem_object **obj)
{
	struct drm_gem_object *psObj;

	psObj = PVRSRVGEMObjectCreate(dev,
				      PVR_DRM_GEM_DISPLAY_PMR,
				      size,
				      PVRSRV_MEMALLOCFLAG_WRITE_COMBINE);
	if (IS_ERR(psObj))
	{
		PVR_DPF((PVR_DBG_ERROR,
			 "%s: Failed to create display GEM object (error %ld)",
			 __FUNCTION__, (long)PTR_ERR(psObj)));

		return PTR_ERR(psObj);
	}

	*obj = psObj;

	return 0;
}

static int PVRSRVGEMMap(struct drm_gem_object *obj)
{
	struct pvr_drm_gem_object *psPVRObj = to_pvr_drm_gem_object(obj);
	PVRSRV_ERROR eError;

	if (psPVRObj == NULL || psPVRObj->type != PVR_DRM_GEM_DISPLAY_PMR)
	{
		PVR_DPF((PVR_DBG_ERROR, "%s: Invalid GEM object", __FUNCTION__));
		return -EINVAL;
	}

	eError = PMRLockSysPhysAddresses(psPVRObj->pmr, PAGE_SHIFT);
	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR,
			 "%s: Failed to lock PMR (error = %u)",
			 __FUNCTION__, eError));
		return (eError == PVRSRV_ERROR_OUT_OF_MEMORY) ? -ENOMEM : -EINVAL;
	}

	return 0;
}

static int PVRSRVGEMUnmap(struct drm_gem_object *obj)
{
	struct pvr_drm_gem_object *psPVRObj = to_pvr_drm_gem_object(obj);
	PVRSRV_ERROR eError;

	if (psPVRObj == NULL || psPVRObj->type != PVR_DRM_GEM_DISPLAY_PMR)
	{
		PVR_DPF((PVR_DBG_ERROR, "%s: Invalid GEM object", __FUNCTION__));
		return -EINVAL;
	}

	eError = PMRUnlockSysPhysAddresses(psPVRObj->pmr);
	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR,
			 "%s: Failed to unlock PMR (error = %u)",
			 __FUNCTION__, eError));
		return -EINVAL;
	}

	return 0;
}

static int PVRSRVGEMCPUAddr(struct drm_gem_object *obj,
			    off_t offset,
			    uint64_t *cpu_addr_out)
{
	struct pvr_drm_gem_object *psPVRObj = to_pvr_drm_gem_object(obj);
	IMG_CPU_PHYADDR sCPUPAddr;
	IMG_BOOL bValid;
	PVRSRV_ERROR eError;

	if (psPVRObj == NULL || psPVRObj->type != PVR_DRM_GEM_DISPLAY_PMR)
	{
		PVR_DPF((PVR_DBG_ERROR, "%s: Invalid GEM object", __FUNCTION__));
		return -EINVAL;
	}

	if (cpu_addr_out == NULL)
	{
		return -EINVAL;
	}

	eError = PMR_CpuPhysAddr(psPVRObj->pmr, offset, &sCPUPAddr, &bValid);
	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR,
			 "%s: Failed to get PMR CPU physical address (error = %u)",
			 __FUNCTION__, eError));
		return -EINVAL;
	}
	PVR_ASSERT(bValid);

	*cpu_addr_out = sCPUPAddr.uiAddr;

	return 0;
}

static int PVRSRVGEMDevAddr(struct drm_gem_object *obj,
			    off_t offset,
			    uint64_t *dev_addr_out)
{
	struct pvr_drm_gem_object *psPVRObj = to_pvr_drm_gem_object(obj);
	IMG_DEV_PHYADDR sDevPAddr;
	IMG_BOOL bValid;
	PVRSRV_ERROR eError;

	if (psPVRObj == NULL || psPVRObj->type != PVR_DRM_GEM_DISPLAY_PMR)
	{
		PVR_DPF((PVR_DBG_ERROR, "%s: Invalid GEM object", __FUNCTION__));
		return -EINVAL;
	}

	if (dev_addr_out == NULL)
	{
		return -EINVAL;
	}

	eError = PMR_DevPhysAddr(psPVRObj->pmr, offset, &sDevPAddr, &bValid);
	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR,
			 "%s: Failed to get PMR device physical address (error = %u)",
			 __FUNCTION__, eError));
		return -EINVAL;
	}
	PVR_ASSERT(bValid);

	*dev_addr_out = sDevPAddr.uiAddr;

	return 0;
}

static IMG_BOOL DisplayFlipReady(IMG_PVOID hFlipReadyData)
{
	PVR_UNREFERENCED_PARAMETER(hFlipReadyData);

	return IMG_TRUE;
}

static void DisplayFlip(IMG_PVOID hFlipReadyData,
			    IMG_PVOID hFlipData)
{
	struct pvr_drm_flip_ready_data *psFlipReadyData = hFlipReadyData;

	psFlipReadyData->flip_cb(&psFlipReadyData->pvr_obj->base, psFlipReadyData->data, hFlipData);
}

static int PVRSRVFlipSchedule(struct drm_gem_object *obj,
			      pvr_drm_flip_func flip_cb,
			      void *data)
{
	struct pvr_drm_gem_object *psPVRObj = to_pvr_drm_gem_object(obj);
	struct pvr_drm_dev_priv *psDevPriv;
	struct pvr_drm_flip_ready_data *psFlipReadyData;
	struct pvr_drm_flip_data *psFlipData;
	SERVER_SYNC_PRIMITIVE *apsSyncPrims[2];
	IMG_BOOL abUpdate[2];
	PVRSRV_ERROR eError;

	if (psPVRObj == NULL || flip_cb == NULL)
	{
		return -EINVAL;
	}
	psDevPriv = obj->dev->dev_private;

	BUG_ON(psPVRObj->type != PVR_DRM_GEM_DISPLAY_PMR);

	drm_gem_object_reference(obj);

	apsSyncPrims[0] = psPVRObj->apsSyncPrim[PVRSRV_GEM_SYNC_TYPE_WRITE];
	abUpdate[0] = IMG_FALSE;

	apsSyncPrims[1] = psPVRObj->apsSyncPrim[PVRSRV_GEM_SYNC_TYPE_READ_DISPLAY];
	abUpdate[1] = IMG_TRUE;

	eError = SCPAllocCommand(psDevPriv->display_flip_context,
				 ARRAY_SIZE(apsSyncPrims),
				 apsSyncPrims,
				 abUpdate,
				 -1,
				 DisplayFlipReady,
				 DisplayFlip,
				 sizeof *psFlipReadyData,
				 sizeof *psFlipData,
				 (IMG_PVOID *)&psFlipReadyData,
				 (IMG_PVOID *)&psFlipData,
				 NULL);
	if (eError != PVRSRV_OK)
	{
		drm_gem_object_unreference_unlocked(obj);
		return -ENOMEM;
	}

	psFlipReadyData->pvr_obj	= psPVRObj;
	psFlipReadyData->flip_cb	= flip_cb;
	psFlipReadyData->data		= data;

	INIT_LIST_HEAD(&psFlipData->flip_done_entry);
	psFlipData->dev		= obj->dev;
	psFlipData->pvr_obj	= psPVRObj;

	/* This should never fail */
	eError = SCPSubmitCommand(psDevPriv->display_flip_context);
	BUG_ON(eError != PVRSRV_OK);

	OSScheduleMISR(psDevPriv->display_misr);

	return 0;
}

static int PVRSRVFlipDone(struct pvr_drm_flip_data *psFlipData)
{
	WARN_ON(psFlipData == NULL);

	if (psFlipData != NULL)
	{
		struct pvr_drm_dev_priv *psDevPriv;
		unsigned long ulFlags;

		BUG_ON(psFlipData->dev == NULL);

		psDevPriv = psFlipData->dev->dev_private;

		spin_lock_irqsave(&psDevPriv->flip_done_lock, ulFlags);
		list_add_tail(&psDevPriv->flip_done_head, &psFlipData->flip_done_entry);
		spin_unlock_irqrestore(&psDevPriv->flip_done_lock, ulFlags);

		OSScheduleMISR(psDevPriv->display_misr);

		return 0;
	}

	return -EINVAL;
}

#if defined(LMA)
static int PVRDRMHeapAcquire(uint32_t heap_id, void **heap_out)
{
	PHYS_HEAP *psPhysHeap;
	PVRSRV_ERROR eError;

	if (heap_out == NULL)
	{
		return -EINVAL;
	}

	eError = PhysHeapAcquire(heap_id, &psPhysHeap);
	if (eError != PVRSRV_OK)
	{
		return -EINVAL;
	}

	PVR_ASSERT(PhysHeapGetType(psPhysHeap) == PHYS_HEAP_TYPE_LMA);

	*heap_out = (void *)psPhysHeap;

	return 0;
}

static void PVRDRMHeapRelease(void *heap)
{
	if (heap)
	{
		PhysHeapRelease((PHYS_HEAP *)heap);
	}
}

static int PVRDRMHeapInfo(void *heap,
			  uint64_t *cpu_phys_base,
			  uint64_t *dev_phys_base,
			  size_t *size)
{
	PHYS_HEAP *psPhysHeap = (PHYS_HEAP *)heap;
	IMG_CPU_PHYADDR sCpuPAddr;
	IMG_DEV_PHYADDR sDevPAddr;
	IMG_UINT64 uiSize;
	PVRSRV_ERROR eError;

	if (cpu_phys_base == NULL || dev_phys_base == NULL || size == NULL)
	{
		return -EINVAL;
	}

	eError = PhysHeapGetAddress(psPhysHeap, &sCpuPAddr);
	if (eError != PVRSRV_OK)
	{
		return -EINVAL;
	}
	PhysHeapCpuPAddrToDevPAddr(psPhysHeap, &sDevPAddr, &sCpuPAddr);

	eError = PhysHeapGetSize(psPhysHeap, &uiSize);
	if (eError != PVRSRV_OK)
	{
		return -EINVAL;
	}

	*cpu_phys_base = sCpuPAddr.uiAddr;
	*dev_phys_base = sDevPAddr.uiAddr;
	*size = uiSize;

	return 0;
}
#else
static int PVRDRMHeapAcquire(uint32_t unref__ heap_id, void unref__ **heap_out)
{
	return -EPERM;
}

static void PVRDRMHeapRelease(void unref__ *heap)
{
}

static int PVRDRMHeapInfo(void unref__ *heap,
			  uint64_t unref__ *cpu_phys_base,
			  uint64_t unref__ *dev_phys_base,
			  size_t unref__ *size)
{
	return -EPERM;
}
#endif /* defined(LMA) */

static void DisplayMISR(void *pvData)
{
	struct drm_device *dev = (struct drm_device *)pvData;
	struct pvr_drm_dev_priv *psDevPriv = dev->dev_private;
	struct pvr_drm_flip_data *psFlipData;
	IMG_BOOL bCheckStatus = IMG_FALSE;
	unsigned long ulFlags;

	spin_lock_irqsave(&psDevPriv->flip_done_lock, ulFlags);
	while (!list_empty(&psDevPriv->flip_done_head))
	{
		psFlipData = list_first_entry(&psDevPriv->flip_done_head, struct pvr_drm_flip_data, flip_done_entry);

		list_del(&psFlipData->flip_done_entry);
		spin_unlock_irqrestore(&psDevPriv->flip_done_lock, ulFlags);

		drm_gem_object_unreference_unlocked(&psFlipData->pvr_obj->base);

		SCPCommandComplete(psDevPriv->display_flip_context);

		bCheckStatus = IMG_TRUE;

		spin_lock_irqsave(&psDevPriv->flip_done_lock, ulFlags);
	}
	spin_unlock_irqrestore(&psDevPriv->flip_done_lock, ulFlags);

	SCPRun(psDevPriv->display_flip_context);

	if (bCheckStatus)
	{
		PVRSRVCheckStatus(NULL);
	}
}

static void DisplayNotifierHandler(PVRSRV_CMDCOMP_HANDLE hCmdCompHandle)
{
	struct drm_device *psDev = (struct drm_device *)hCmdCompHandle;
	struct pvr_drm_dev_priv *psDevPriv = psDev->dev_private;

	OSScheduleMISR(psDevPriv->display_misr);
}


/*************************************************************************/ /*!
* DRM Display interface
*/ /**************************************************************************/

int PVRSRVDRMDisplayInit(struct drm_device *dev)
{
	struct pvr_drm_dev_priv *psDevPriv;
	PVRSRV_ERROR eError;
	int iRes;

	if (dev == NULL)
	{
		PVR_DPF((PVR_DBG_ERROR, "%s: No DRM device", __FUNCTION__));
		return -ENODEV;
	}
	psDevPriv = dev->dev_private;

	if (psDevPriv == NULL)
	{
		PVR_DPF((PVR_DBG_ERROR, "%s: DRM device not initialised", __FUNCTION__));
		return -ENODEV;
	}

	if (psDevPriv->display_priv != NULL)
	{
		PVR_DPF((PVR_DBG_ERROR, "%s: Display module already initialised", __FUNCTION__));
		return -EEXIST;
	}

	spin_lock_init(&psDevPriv->flip_done_lock);
	INIT_LIST_HEAD(&psDevPriv->flip_done_head);

	eError = SCPCreate(PVR_DRM_FLIP_SCP_SIZE_LOG2, &psDevPriv->display_flip_context);
	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR,
			 "%s: Failed to create scp context (error = %d)",
			 __FUNCTION__, eError));
		return -EPERM;
	}

	eError = OSInstallMISR(&psDevPriv->display_misr,
			       DisplayMISR,
			       dev);
	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR,
			 "%s: Failed to install MISR (error = %d)",
			 __FUNCTION__, eError));
		iRes = -ENOMEM;
		goto error_scp_destroy;
	}

	eError = PVRSRVRegisterCmdCompleteNotify(&psDevPriv->display_notify, DisplayNotifierHandler, dev);
	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR,
			 "%s: Failed to register for command complete events (error = %d)",
			 __FUNCTION__, eError));
		iRes = -EPERM;
		goto error_uninstall_misr;
	}

	psDevPriv->funcs.pvr_drm_get_display_device	= PVRSRVGetDisplayDevice;
	psDevPriv->funcs.pvr_drm_display_irq_install	= PVRSRVDisplayIRQInstall;
	psDevPriv->funcs.pvr_drm_display_irq_uninstall	= PVRSRVDisplayIRQUninstall;
	psDevPriv->funcs.pvr_drm_gem_create		= PVRSRVGEMCreate;
	psDevPriv->funcs.pvr_drm_gem_map		= PVRSRVGEMMap;
	psDevPriv->funcs.pvr_drm_gem_unmap		= PVRSRVGEMUnmap;
	psDevPriv->funcs.pvr_drm_gem_cpu_addr		= PVRSRVGEMCPUAddr;
	psDevPriv->funcs.pvr_drm_gem_dev_addr		= PVRSRVGEMDevAddr;
	psDevPriv->funcs.pvr_drm_flip_schedule		= PVRSRVFlipSchedule;
	psDevPriv->funcs.pvr_drm_flip_done		= PVRSRVFlipDone;
	psDevPriv->funcs.pvr_drm_heap_acquire		= PVRDRMHeapAcquire;
	psDevPriv->funcs.pvr_drm_heap_release		= PVRDRMHeapRelease;
	psDevPriv->funcs.pvr_drm_heap_info		= PVRDRMHeapInfo;

	iRes = PVR_DRM_MAKENAME(DISPLAY_CONTROLLER, _init)(dev, &psDevPriv->display_priv);
	if (iRes != 0)
	{
		PVR_DPF((PVR_DBG_ERROR,
			 "%s: Failed to initialise display module (error = %d)",
			 __FUNCTION__, iRes));
		goto error_uninstall_funcs;
	}

	iRes = PVR_DRM_MAKENAME(DISPLAY_CONTROLLER, _configure)(psDevPriv->display_priv);
	if (iRes != 0)
	{
		PVR_DPF((PVR_DBG_ERROR,
			 "%s: Failed to configure display module (error = %d)",
			 __FUNCTION__, iRes));
		goto error_display_cleanup;
	}

	return 0;

error_display_cleanup:
	PVR_DRM_MAKENAME(DISPLAY_CONTROLLER, _cleanup)(psDevPriv->display_priv);
	psDevPriv->display_priv = NULL;

error_uninstall_funcs:
	memset(&psDevPriv->funcs, 0, sizeof(psDevPriv->funcs));

	PVRSRVUnregisterCmdCompleteNotify(psDevPriv->display_notify);
	psDevPriv->display_notify = NULL;

error_uninstall_misr:
	OSUninstallMISR(psDevPriv->display_misr);
	psDevPriv->display_misr = NULL;

error_scp_destroy:
	SCPDestroy(psDevPriv->display_flip_context);
	psDevPriv->display_flip_context = NULL;

	return iRes;
}

int PVRSRVDRMDisplayDeinit(struct drm_device *dev)
{
	struct pvr_drm_dev_priv *psDevPriv;

	if (dev == NULL)
	{
		PVR_DPF((PVR_DBG_ERROR, "%s: No DRM device", __FUNCTION__));
		return -ENODEV;
	}
	psDevPriv = dev->dev_private;

	if (psDevPriv == NULL)
	{
		PVR_DPF((PVR_DBG_ERROR, "%s: DRM device not initialised", __FUNCTION__));
		return -ENODEV;
	}

	if (psDevPriv->display_priv == NULL)
	{
		PVR_DPF((PVR_DBG_ERROR, "%s: Display module not initialised", __FUNCTION__));
		return -ENODEV;
	}

	PVR_DRM_MAKENAME(DISPLAY_CONTROLLER, _cleanup)(psDevPriv->display_priv);
	psDevPriv->display_priv = NULL;

	memset(&psDevPriv->funcs, 0, sizeof(psDevPriv->funcs));

	PVRSRVUnregisterCmdCompleteNotify(psDevPriv->display_notify);
	psDevPriv->display_notify = NULL;

	OSUninstallMISR(psDevPriv->display_misr);
	psDevPriv->display_misr = NULL;

	SCPDestroy(psDevPriv->display_flip_context);
	psDevPriv->display_flip_context = NULL;

	return 0;
}

u32 PVRSRVDRMDisplayGetVBlankCounter(struct drm_device *dev, int crtc)
{
	struct pvr_drm_dev_priv *psDevPriv = (struct pvr_drm_dev_priv *)dev->dev_private;

	return PVR_DRM_MAKENAME(DISPLAY_CONTROLLER, _get_vblank_counter)(psDevPriv->display_priv, crtc);
}

int PVRSRVDRMDisplayEnableVBlank(struct drm_device *dev, int crtc)
{
	struct pvr_drm_dev_priv *psDevPriv = (struct pvr_drm_dev_priv *)dev->dev_private;

	return PVR_DRM_MAKENAME(DISPLAY_CONTROLLER, _enable_vblank)(psDevPriv->display_priv, crtc);
}

void PVRSRVDRMDisplayDisableVBlank(struct drm_device *dev, int crtc)
{
	struct pvr_drm_dev_priv *psDevPriv = (struct pvr_drm_dev_priv *)dev->dev_private;
	int iErr;

	iErr = PVR_DRM_MAKENAME(DISPLAY_CONTROLLER, _disable_vblank)(psDevPriv->display_priv, crtc);
	if (iErr != 0)
	{
		PVR_DPF((PVR_DBG_ERROR,
			 "%s: Failed to disable vblank interrupts for crtc %d (error = %d)",
			 __FUNCTION__, crtc, iErr));
	}
}
