#!/bin/bash

CB_ROOT=${PWD}
CB_KDIR=${PWD}/linux-3.4
CB_TOOLS=${PWD}/tools
CB_ROOTFS=${PWD}/rootfs
CB_OUTPUT=${PWD}/output
CB_PRODUCT_DIR=${PWD}/products/cb4/cb4-lubuntu-desktop
CB_BOARD=cubieboard8
CB_KCONFIG=${CB_BOARD}_defconfig
CB_ROOTFS_IMAGE=lubuntu_20140821.ext4

ARCH=arm
CROSS_COMPILE=arm-linux-gnueabihf-

build_prepare()
{
    echo "Preparing build enviroment"
    sudo rm -rf ${CB_OUTPUT}
    mkdir ${CB_OUTPUT}
    cp ${CB_ROOTFS}/${CB_ROOTFS_IMAGE} ${CB_OUTPUT}/rootfs.ext4
    mkdir ${CB_OUTPUT}/stage
    sudo mount -o loop ${CB_OUTPUT}/rootfs.ext4 ${CB_OUTPUT}/stage
}

build_clean()
{
    sudo umount ${CB_OUTPUT}/stage
}

build_kernel()
{
    cd $CB_KDIR
    make ARCH=arm CROSS_COMPILE=${CROSS_COMPILE} ${CB_KCONFIG}
    make ARCH=arm CROSS_COMPILE=${CROSS_COMPILE} -j8 uImage  modules
	sudo make ARCH=arm CROSS_COMPILE=${CROSS_COMPILE} INSTALL_FW_PATH=${CB_OUTPUT}/stage/lib/firmware -j8 firmware_install
    sudo make ARCH=arm CROSS_COMPILE=${CROSS_COMPILE} INSTALL_MOD_PATH=${CB_OUTPUT}/stage -j8 modules_install
	sudo cp -vf Module.symvers ${CB_OUTPUT}/stage/lib/modules/*/
    cd $CB_ROOT
}

build_rootfs()
{
    (
    cd $CB_KDIR
    tar -jcf ${CB_OUTPUT}/vmlinux.tar.bz2 vmlinux
    )
	(cd ${CB_PRODUCT_DIR}/overlay; tar -c *) |sudo tar -C ${CB_OUTPUT}/stage  -x --no-same-owner
#	cp -v ${CB_KDIR}/arch/arm/boot/uImage ${CB_OUTPUT}/boot.img
	cp -v ${CB_KDIR}/arch/arm/boot/uImage ${CB_TOOLS}/pack/chips/sun9iw1p1/boot-resource/boot-resource/
    ${CB_TOOLS}/pack/pctools/linux/android/mkbootimg \
        --kernel ${CB_KDIR}/arch/arm/boot/Image \
        --ramdisk ${CB_KDIR}/rootfs.cpio.gz \
        --board 'sun9i' \
        --base 0x20000000 \
        -o ${CB_OUTPUT}/boot.img
	sync
}

build_pack()
{
    (
    cd ${CB_TOOLS}/pack
    cp ${CB_KDIR}/drivers/arisc/binary/arisc ${CB_OUTPUT}/
    LICHEE_OUT=${CB_OUTPUT} ./pack -c sun9iw1p1 -p linux -b ${CB_BOARD}
    )
}

build_prepare
build_kernel
build_rootfs
build_pack
build_clean
