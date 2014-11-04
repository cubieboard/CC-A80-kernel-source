#
# Makefile for the SUNXI SS controller
#

obj-$(CONFIG_CRYPTO_SUNXI) += ss.o

ss-y += sunxi_ss.o
ss-$(CONFIG_ARCH_SUN8IW1) += sunxi_ss_reg_gen.o
ss-$(CONFIG_ARCH_SUN8IW3) += sunxi_ss_reg_gen.o
ss-$(CONFIG_ARCH_SUN8IW5) += sunxi_ss_reg_gen.o
ss-$(CONFIG_ARCH_SUN8IW6) += sunxi_ss_reg_sun9i.o
ss-$(CONFIG_ARCH_SUN9IW1) += sunxi_ss_reg_sun9i.o
