########################################################################### ###
#@File
#@Title         Select a window system
#@Copyright     Copyright (c) Imagination Technologies Ltd. All Rights Reserved
#@License       Dual MIT/GPLv2
# 
# The contents of this file are subject to the MIT license as set out below.
# 
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
# 
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
# 
# Alternatively, the contents of this file may be used under the terms of
# the GNU General Public License Version 2 ("GPL") in which case the provisions
# of GPL are applicable instead of those above.
# 
# If you wish to allow use of your version of this file only under the terms of
# GPL, and not to allow others to use your version of this file under the terms
# of the MIT license, indicate your decision by deleting the provisions above
# and replace them with the notice and other provisions required by GPL as set
# out in the file called "GPL-COPYING" included in this distribution. If you do
# not delete the provisions above, a recipient may use your version of this file
# under the terms of either the MIT license or GPL.
# 
# This License is also included in this distribution in the file called
# "MIT-COPYING".
# 
# EXCEPT AS OTHERWISE STATED IN A NEGOTIATED AGREEMENT: (A) THE SOFTWARE IS
# PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING
# BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
# PURPOSE AND NONINFRINGEMENT; AND (B) IN NO EVENT SHALL THE AUTHORS OR
# COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
# IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
# CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
### ###########################################################################

# Set the default window system.
# If you want to override the default, create a default_window_system.mk file
# that sets WINDOW_SYSTEM appropriately.  (There is a suitable example in 
# ../config/default_window_system_xorg.mk)
-include ../config/default_window_system.mk
WINDOW_SYSTEM ?= ews

# Check for WINDOW_SYSTEMs that aren't understood by the 
# common/window_systems/*.mk files. This should be kept in 
# sync with all the tests on WINDOW_SYSTEMs in those files
_window_systems := nullws nulldrmws ews ews_drm wayland xorg

_unrecognised_window_system := $(strip $(filter-out $(_window_systems),$(WINDOW_SYSTEM)))
ifneq ($(_unrecognised_window_system),)
$(warning *** Unrecognised WINDOW_SYSTEM: $(_unrecognised_window_system))
$(warning *** WINDOW_SYSTEM was set via: $(origin WINDOW_SYSTEM))
$(error Supported Window Systems are: $(_window_systems))
endif

# If we get rid of OPK_DEFAULT and OPK_FALLBACK
# and just have a symlink to the correct one
# we can get rid of most of this.
ifeq ($(WINDOW_SYSTEM),xorg)
 LWS_DIST ?= tarballs-xorg
 MESA_EGL := 1
 SUPPORT_OPENGLES1_V1_ONLY := 1
 SUPPORT_DRM := 1
 PVR_HANDLE_BACKEND := idr
else ifeq ($(WINDOW_SYSTEM),wayland)
 LWS_DIST ?= tarballs-wayland
 MESA_EGL := 1
 SUPPORT_OPENGLES1_V1_ONLY := 1
 SUPPORT_DRM := 1
 PVR_HANDLE_BACKEND := idr
else ifeq ($(WINDOW_SYSTEM),ews)
 OPK_DEFAULT  := libpvrEWS_WSEGL.so
 OPK_FALLBACK := libpvrNULL_WSEGL.so
else ifeq ($(WINDOW_SYSTEM),nullws)
 OPK_DEFAULT  := libpvrNULL_WSEGL.so
 OPK_FALLBACK := libpvrNULL_WSEGL.so
else ifeq ($(WINDOW_SYSTEM),ews_drm)
 LWS_DIST ?= tarballs-nulldrm
 OPK_DEFAULT  := libpvrEWS_WSEGL.so
 OPK_FALLBACK := libpvrNULLDRM_WSEGL.so
 SUPPORT_DRM := 1
 PVR_HANDLE_BACKEND := idr
else ifeq ($(WINDOW_SYSTEM),nulldrmws)
 LWS_DIST ?= tarballs-nulldrm
 OPK_DEFAULT  := libpvrNULLDRM_WSEGL.so
 OPK_FALLBACK := libpvrNULLDRM_WSEGL.so
 SUPPORT_DRM := 1
 PVR_HANDLE_BACKEND := idr
endif

