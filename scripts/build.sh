#!/bin/bash
#
# scripts/build.h
#
# (c) Copyright 2013
# Allwinner Technology Co., Ltd. <www.allwinnertech.com>
# James Deng <csjamesdeng@allwinnertech.com>
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.

set -e

# Setup common variables
export ARCH=arm
export CROSS_COMPILE=arm-linux-gnueabi-
export AS=${CROSS_COMPILE}as
export LD=${CROSS_COMPILE}ld
export CC=${CROSS_COMPILE}gcc
export AR=${CROSS_COMPILE}ar
export NM=${CROSS_COMPILE}nm
export STRIP=${CROSS_COMPILE}strip
export OBJCOPY=${CROSS_COMPILE}objcopy
export OBJDUMP=${CROSS_COMPILE}objdump

KERNEL_VERSION=`make -s kernelversion -C .`
LICHEE_KDIR=`pwd`
LICHEE_MOD_DIR=${LICHEE_KDIR}/output/lib/modules/${KERNEL_VERSION}

update_kernel_ver()
{
    if [ -r include/generated/utsrelease.h ] ; then
        KERNEL_VERSION=`cat include/generated/utsrelease.h | awk -F\" '{print $2}'`
    fi
    LICHEE_MOD_DIR=${LICHEE_KDIR}/output/lib/modules/${KERNEL_VERSION}
}

NAND_ROOT=${LICHEE_KDIR}/modules/nand

build_nand_lib()
{
    echo "build nand library ${NAND_ROOT}/${LICHEE_CHIP}/lib"
    if [ -d ${NAND_ROOT}/${LICHEE_CHIP}/lib ] ; then
        echo "build nand library now"
        make -C modules/nand/${LICHEE_CHIP}/lib clean 2> /dev/null 
        make -C modules/nand/${LICHEE_CHIP}/lib lib install
    else
        echo "build nand with existing library"
    fi
}


build_gpu()
{
    echo "build gpu module"
    unset OUT
    unset TOP
    make -j16 -C modules/rogue_km/build/linux/sunxi_android \
        LICHEE_MOD_DIR=${LICHEE_MOD_DIR} LICHEE_KDIR=${LICHEE_KDIR} \
        RGX_BVNC=1.75.2.30 \
        KERNELDIR=${LICHEE_KDIR}
    for file in $(find  modules/rogue_km -name "*.ko"); do
        cp $file ${LICHEE_MOD_DIR}
    done
}

clean_gpu()
{
    echo "clean gpu module"
    unset OUT
    unset TOP
    make -C modules/rogue_km/build/linux/sunxi_android \
        LICHEE_MOD_DIR=${LICHEE_MOD_DIR} LICHEE_KDIR=${LICHEE_KDIR} \
        RGX_BVNC=1.75.2.30 \
        KERNELDIR=${LICHEE_KDIR} clean
}

build_kernel()
{
    echo "Building kernel"

    if [ ! -f .config ] ; then
        printf "\n\033[0;31;1mUsing default config ...\033[0m\n\n"
        cp arch/arm/configs/${LICHEE_KERN_DEFCONF} .config
    fi

    make ARCH=arm CROSS_COMPILE=${CROSS_COMPILE} -j${LICHEE_JLEVEL} uImage modules

    update_kernel_ver

    rm -rf output
    mkdir -p ${LICHEE_MOD_DIR}
    cp -vf arch/arm/boot/Image output/bImage
    cp -vf arch/arm/boot/[zu]Image output/
    cp .config output/
	cp ./drivers/arisc/binary/arisc output/
    tar -jcf output/vmlinux.tar.bz2 vmlinux
    for file in $(find drivers sound crypto block fs security net -name "*.ko"); do
        cp $file ${LICHEE_MOD_DIR}
    done
    cp -f Module.symvers ${LICHEE_MOD_DIR}
}

build_modules()
{
    echo "Building modules"

    if [ ! -f include/generated/utsrelease.h ]; then
        printf "Please build kernel first\n"
        exit 1
    fi

    update_kernel_ver

    build_nand_lib
    make -C modules/nand LICHEE_MOD_DIR=${LICHEE_MOD_DIR} LICHEE_KDIR=${LICHEE_KDIR} \
        install
    make -C modules/aw_schw LICHEE_MOD_DIR=${LICHEE_MOD_DIR} LICHEE_KDIR=${LICHEE_KDIR} \
        install

    build_gpu
}

regen_rootfs_cpio()
{
	echo "regenerate rootfs cpio"

    cd ${LICHEE_KDIR}/output
    if [ -x "../scripts/build_rootfs.sh" ]; then
        ../scripts/build_rootfs.sh e ../rootfs.cpio.gz > /dev/null
    else
        echo "No such file: scripts/build_rootfs.sh"
        exit 1
    fi

    mkdir -p ./skel/lib/modules/${KERNEL_VERSION}
    cp ${LICHEE_MOD_DIR}/nand.ko ./skel/lib/modules/${KERNEL_VERSION}
    if [ $? -ne 0 ]; then
        echo "copy nand module error: $?"
        exit 1
    fi

    rm -f rootfs.cpio.gz
    ../scripts/build_rootfs.sh c rootfs.cpio.gz > /dev/null
    rm -rf skel
	cd - > /dev/null
}

build_bootimg()
{
    if [ "x${LICHEE_PLATFORM}" = "xandroid" ] ; then
        return
    fi

    # update rootfs.cpio.gz with new module files
    regen_rootfs_cpio

    echo "Building boot image"
    ${LICHEE_TOOLS_DIR}/pack/pctools/linux/android/mkbootimg \
        --kernel output/bImage \
        --ramdisk output/rootfs.cpio.gz \
        --board '${chip_name}' \
        --base 0x20000000 \
        -o output/boot.img
    echo "Copy boot.img to output directory ..."
    cp output/boot.img ${LICHEE_PLAT_OUT}
    cp output/vmlinux.tar.bz2 ${LICHEE_PLAT_OUT}
	cp output/arisc ${LICHEE_PLAT_OUT}
}

gen_output()
{
    if [ "x${LICHEE_PLATFORM}" = "xandroid" ] ; then
        echo "Copy modules to target ..."
        rm -rf ${LICHEE_PLAT_OUT}/lib
        cp -rf ${LICHEE_KDIR}/output/* ${LICHEE_PLAT_OUT}
        return
    fi

    if [ -d ${LICHEE_BR_OUT}/target ] ; then
        echo "Copy modules to target ..."
        local module_dir="${LICHEE_BR_OUT}/target/lib/modules"
        rm -rf ${module_dir}
        mkdir -p ${module_dir}
        cp -rf ${LICHEE_MOD_DIR} ${module_dir}
    fi
}

clean_kernel()
{
    echo "Cleaning kernel"
    make distclean
    rm -rf output/*
}

clean_modules()
{
    echo "Cleaning modules"
    clean_gpu
}

case "$1" in
    kernel)
        build_kernel
        ;;
    modules)
        build_modules
        ;;
    clean)
        clean_modules
        clean_kernel
        ;;
    *)
        build_kernel
        build_modules
        build_bootimg
        gen_output
        ;;
esac

