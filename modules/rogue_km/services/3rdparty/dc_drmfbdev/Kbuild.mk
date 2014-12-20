########################################################################### ###
#@Copyright     Copyright (c) Imagination Technologies Ltd. All Rights Reserved
#@License       Strictly Confidential.
### ###########################################################################

ccflags-y += \
 -I$(TOP)/services/3rdparty/dc_drmfbdev

dc_drmfbdev-y += \
	services/3rdparty/dc_drmfbdev/dc_drmfbdev.o

