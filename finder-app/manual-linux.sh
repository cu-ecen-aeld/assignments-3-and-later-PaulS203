#!/bin/bash
# Script outline to install and build kernel.
# Author: Siddhant Jajoo.


# Pauls203: added to speed up: determine whether the kernal image exists already and copy is later
# down the script to save hours of compilation time causing GitHub Actions to timeout.
IMAGE_PATH="$(realpath "$(dirname "$0")")/Image"


### original script starts here

set -e
set -u

OUTDIR=/tmp/aeld
KERNEL_REPO=git://git.kernel.org/pub/scm/linux/kernel/git/stable/linux-stable.git
KERNEL_VERSION=v5.15.163
BUSYBOX_VERSION=1_33_1
FINDER_APP_DIR=$(realpath $(dirname $0))
ARCH=arm64
CROSS_COMPILE=aarch64-none-linux-gnu-

if [ $# -lt 1 ]
then
	echo "Using default directory ${OUTDIR} for output"
else
	OUTDIR=$1
	echo "Using passed directory ${OUTDIR} for output"
fi

mkdir -p ${OUTDIR}

cd "$OUTDIR"
if [ ! -d "${OUTDIR}/linux-stable" ]; then
    #Clone only if the repository does not exist.
	echo "CLONING GIT LINUX STABLE VERSION ${KERNEL_VERSION} IN ${OUTDIR}"
	git clone ${KERNEL_REPO} --depth 1 --single-branch --branch ${KERNEL_VERSION}
fi


# PaulS203: copy the kernal image if it exists to save compile time.
if [ -f "$IMAGE_PATH" ]; then
	echo "file exists, copy operation started"
    cp "$IMAGE_PATH" "${OUTDIR}/linux-stable/arch/${ARCH}/boot/Image"
else 
	echo "file does NOT exist, will build kernel"
fi

if [ ! -e ${OUTDIR}/linux-stable/arch/${ARCH}/boot/Image ]; then
    cd linux-stable
    echo "Checking out version ${KERNEL_VERSION}"
    git checkout ${KERNEL_VERSION}

    # TODO: Add your kernel build steps here
    echo "Kernel build steps"
    make ARCH=$ARCH CROSS_COMPILE=$CROSS_COMPILE mrproper
    echo "Kernel build: Configure for our “virt” arm dev board we will simulate in QEMU."
    make ARCH=$ARCH CROSS_COMPILE=$CROSS_COMPILE defconfig
    echo "Kernel build: Build a kernel image for booting with QEMU."
    make -j4 ARCH=$ARCH CROSS_COMPILE=$CROSS_COMPILE all
    echo "Kernel build: Build any kernel modules"
    make ARCH=$ARCH CROSS_COMPILE=$CROSS_COMPILE modules
    echo "Kernel build: Build the devicetree."
    make ARCH=$ARCH CROSS_COMPILE=$CROSS_COMPILE dtbs
    echo "Kernel build done."

fi

echo "Adding the Image in outdir"
cp ${OUTDIR}/linux-stable/arch/$ARCH/boot/Image ${OUTDIR}

echo "Creating the staging directory for the root filesystem"
cd "$OUTDIR"
if [ -d "${OUTDIR}/rootfs" ]
then
	echo "Deleting rootfs directory at ${OUTDIR}/rootfs and starting over"
    sudo rm  -rf ${OUTDIR}/rootfs
fi
# TODO: Create necessary base directories
mkdir -p ${OUTDIR}/rootfs/{bin,sbin,etc,proc,sys,usr/{bin,sbin},lib,lib64,dev,home,tmp,var}

echo "########## ********** creating busybox"
cd "$OUTDIR"
if [ ! -d "${OUTDIR}/busybox" ]
then
git clone git://busybox.net/busybox.git
    cd busybox
    git checkout ${BUSYBOX_VERSION}
    # TODO:  Configure busybox
    make distclean
	make defconfig

else
    cd busybox
fi

# TODO: Make and install busybox
echo "Make and install busybox"
make ARCH=$ARCH CROSS_COMPILE=$CROSS_COMPILE CONFIG_PREFIX=${OUTDIR}/rootfs install
cd "${OUTDIR}/rootfs"

echo "Library dependencies"
${CROSS_COMPILE}readelf -a bin/busybox | grep "program interpreter"
${CROSS_COMPILE}readelf -a bin/busybox | grep "Shared library"

# TODO: Add library dependencies to rootfs
echo "Add library dependencies to rootfs"
SYSROOT=`${CROSS_COMPILE}gcc --print-sysroot`
cp -a "${SYSROOT}"/lib/* lib/
cp -a "${SYSROOT}"/lib64 .

# TODO: Make device nodes
echo "Make device nodes"
sudo mknod -m 666 dev/null c 1 3
sudo mknod -m 666 dev/console c 5 1

# TODO: Clean and build the writer utility
echo "Clean and build the writer utility"
cd $FINDER_APP_DIR
make clean
make CROSS_COMPILE=$CROSS_COMPILE


# TODO: Copy the finder related scripts and executables to the /home directory
# on the target rootfs
echo "Copy the finder stuff"
cd $FINDER_APP_DIR
cp -a writer autorun-qemu.sh finder-test.sh finder.sh "${OUTDIR}/rootfs/home"
mkdir "${OUTDIR}/rootfs/home/conf"
cp -a conf/username.txt "${OUTDIR}/rootfs/home/conf"

# TODO: Chown the root directory
echo "Chown the root directory"
sudo chown -R root:root *

# TODO: Create initramfs.cpio.gz
echo "Create initramfs.cpio.gz"
cd "${OUTDIR}/rootfs"
find . | cpio -o -H newc > ${OUTDIR}/initramfs.cpio
cd "${OUTDIR}"
gzip -f initramfs.cpio
