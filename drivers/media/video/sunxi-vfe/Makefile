obj-$(CONFIG_CSI_VFE) += vfe_os.o
obj-$(CONFIG_CSI_VFE) += vfe_subdev.o
obj-$(CONFIG_CSI_VFE) += device/
obj-$(CONFIG_CSI_VFE) += actuator/
obj-$(CONFIG_CSI_VFE) += csi_cci/
#obj-$(CONFIG_CSI_VFE) += flash_light/
#obj-$(CONFIG_CSI_VFE) += camera_detector/

ifneq ($(strip $(CONFIG_ARCH_SUN9I)),)
obj-$(CONFIG_CSI_VFE) += vfe_v4l2.o
vfe_v4l2-y					:= csi/csi_reg.o
vfe_v4l2-y					+= bsp_csi.o 
vfe_v4l2-y					+= bsp_common.o
vfe_v4l2-y					+= config.o
vfe_v4l2-y					+= sensor_info.o
vfe_v4l2-y					+= cfg_op.o
vfe_v4l2-y					+= vfe.o
vfe_v4l2-y					+= lib/libisp
vfe_v4l2-y					+= lib/lib_mipicsi2_v2

else
obj-$(CONFIG_CSI_VFE) += vfe_v4l2.o
vfe_v4l2-y					:= csi/csi_reg.o
vfe_v4l2-y					+= bsp_csi.o 
vfe_v4l2-y					+= bsp_common.o
vfe_v4l2-y					+= config.o
vfe_v4l2-y					+= sensor_info.o
vfe_v4l2-y					+= cfg_op.o
vfe_v4l2-y					+= vfe.o
vfe_v4l2-y					+= lib/libisp
vfe_v4l2-y					+= lib/lib_mipicsi2_v1

endif