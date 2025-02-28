#!/bin/bash
# Script outline to install and build kernel.
# Author: Siddhant Jajoo.

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
if [ ! -e ${OUTDIR}/linux-stable/arch/${ARCH}/boot/Image ]; then
	cd linux-stable
	echo "Checking out version ${KERNEL_VERSION}"
	git checkout ${KERNEL_VERSION}
	# TODO: Add your kernel build steps here
	make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} mrproper
	make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} defconfig
	make -j$(nproc) ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} all
	make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} modules
	make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} dtbs
	echo "Adding the Image in outdir"
	cp ${OUTDIR}/linux-stable/arch/${ARCH}/boot/Image ${OUTDIR}/Image
fi

echo "Creating the staging directory for the root filesystem"
cd "$OUTDIR"
if [ -d "${OUTDIR}/rootfs" ]
then
	echo "Deleting rootfs directory at ${OUTDIR}/rootfs and starting over"
    	sudo rm  -rf ${OUTDIR}/rootfs
fi

# TODO: Create necessary base directories
echo "Creating necessary base directories"
mkdir -p rootfs
cd rootfs
mkdir -p bin dev etc home lib lib64 proc sbin sys tmp usr var
mkdir -p usr/bin usr/sbin
mkdir -p var/log
mkdir -p home/conf

cd "$OUTDIR"
if [ ! -d "${OUTDIR}/busybox" ]
then
	git clone git://busybox.net/busybox.git
	cd busybox
	git checkout ${BUSYBOX_VERSION}
	# TODO:  Configure busybox
	echo "Configuring Busybox"
else
    cd busybox
fi

# TODO: Make and install busybox
make distclean
make defconfig
make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} -j$(nproc)
make CONFIG_PREFIX=${OUTDIR}/rootfs ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} install  

# Crosss compiling bash so that sh works for finder.sh: https://wiki.beyondlogic.org/index.php?title=Cross_Compiling_BusyBox_for_ARM
mkdir -p ${OUTDIR}/bash
cd ${OUTDIR}/bash
BASH_REPO='https://ftp.gnu.org/gnu/bash/bash-5.2.tar.gz'
BASH_VERSION=5.2
wget ${BASH_REPO} -O bash-${BASH_VERSION}.tar.gz
tar -xvf bash-${BASH_VERSION}.tar.gz
cd bash-${BASH_VERSION}
./configure --prefix=${OUTDIR}/bash-install --host=${CROSS_COMPILE%?}
make -j$(nproc)
make install
cp ${OUTDIR}/bash-install/bin/bash ${OUTDIR}/rootfs/bin/

echo "Library dependencies"
cd ${OUTDIR}/rootfs
${CROSS_COMPILE}readelf -a bin/busybox | grep "program interpreter"
${CROSS_COMPILE}readelf -a bin/busybox | grep "Shared library"

echo "Add library dependencies to rootfs"
# TODO: Add library dependencies to rootfs
SYSROOT=$(${CROSS_COMPILE}gcc --print-sysroot)
cp -a ${SYSROOT}/lib/. ${OUTDIR}/rootfs/lib/
cp -a ${SYSROOT}/lib64/. ${OUTDIR}/rootfs/lib64/

# cp -a ${SYSROOT}/lib/ld-linux-aarch64.so.1 ${OUTDIR}/rootfs/lib/
# cp -a ${SYSROOT}/lib64/libm.so.6 ${OUTDIR}/rootfs/lib64/
# cp -a ${SYSROOT}/lib64/libresolv.so.2 ${OUTDIR}/rootfs/lib64/
# cp -a ${SYSROOT}/lib64/libc.so.6 ${OUTDIR}/rootfs/lib64/


echo "Make device nodes"
# TODO: Make device nodes
sudo mknod -m 666 dev/null c 1 3
sudo mknod -m 666 dev/console c 5 1
sudo mknod -m 666 dev/tty c 5 0

echo "Clean and build the writer utility"
# TODO: Clean and build the writer utility
cd ${FINDER_APP_DIR}
make clean
make CROSS_COMPILE=${CROSS_COMPILE}
cp -a writer ${OUTDIR}/rootfs/home/

echo "Copy the finder related scripts and executables to the /home directory on the target rootfs"
# TODO: Copy the finder related scripts and executables to the /home directory
# on the target rootfs
cp ${FINDER_APP_DIR}/finder.sh ${OUTDIR}/rootfs/home/
cp ${FINDER_APP_DIR}/finder-test.sh ${OUTDIR}/rootfs/home/
cp ${FINDER_APP_DIR}/conf/username.txt ${OUTDIR}/rootfs/home/conf/
cp ${FINDER_APP_DIR}/conf/assignment.txt ${OUTDIR}/rootfs/home/conf/
cp ${FINDER_APP_DIR}/autorun-qemu.sh ${OUTDIR}/rootfs/home/
# 

echo "Chown the root directory"
# TODO: Chown the root directory
cd "${OUTDIR}"
sudo chown -R root:root "${OUTDIR}/rootfs"

echo "Create initramfs.cpio.gz"
# TODO: Create initramfs.cpio.gz
cd "${OUTDIR}/rootfs"
find . | cpio -H newc -ov --owner root:root > "${OUTDIR}/initramfs.cpio"
gzip -f "${OUTDIR}/initramfs.cpio"


