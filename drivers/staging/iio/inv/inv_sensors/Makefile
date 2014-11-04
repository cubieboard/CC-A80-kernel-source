#
# Makefile for Invensense inv-mpu-iio device.
#

ifeq ($(CONFIG_INV_SENSORS), y)

#----------------------------------------------------------------------------------------------
#inv_sensors-y := inv_sensors_iio.o inv_sensors_control.o inv_sensors_sm.o inv_sensors_common.o
#inv_sensors-$(CONFIG_IIO_BUFFER) += inv_sensors_buffer.o inv_sensors_aux.o
#inv_sensors-$(CONFIG_INV_SENSORS_MPU5400) += inv_sensors_mpu5400.o
#inv_sensors-$(CONFIG_INV_SENSORS_DMP) += inv_dmp_base.o
#inv_sensors-$(CONFIG_INV_SENSORS_DMP) += inv_dmp_fw.o
#inv_sensors-$(CONFIG_INV_SENSORS_DMP_K520S) += inv_dmp_k520s.o
#inv_sensors-$(CONFIG_INV_SENSORS_DTS_SUPPORT) += inv_sensors_dts.o
#obj-$(CONFIG_INV_MPU5400_IIO) += inv_sensors.o
#-----------------------------------------------------------------------------

obj-$(CONFIG_INV_MPU5400_IIO) += mpu5400.o
mpu5400-objs := inv_sensors_buffer.o inv_sensors_aux.o inv_sensors_common.o inv_sensors_mpu5400.o inv_dmp_base.o inv_dmp_fw.o inv_dmp_k520s.o inv_sensors_control.o inv_sensors_sm.o inv_sensors_iio.o  
endif

