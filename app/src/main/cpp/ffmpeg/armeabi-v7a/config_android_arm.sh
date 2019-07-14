#!/bin/bash
NDK=/home/hechol/android-ndk-r15c
SYSROOT=$NDK/platforms/android-19/arch-arm/
TOOLCHAIN=$NDK/toolchains/arm-linux-androideabi-4.9/prebuilt/linux-x86_64
function build_one
{
./configure \
--prefix=$PREFIX \
--enable-shared \
--disable-static \
--disable-doc \
--disable-ffplay \
--disable-ffprobe \
--disable-doc \
--disable-symver \
--enable-protocol=concat \
--enable-protocol=file \
--enable-muxer=mp4 \
--enable-demuxer=mpegts \
--cross-prefix=$TOOLCHAIN/bin/arm-linux-androideabi- \
--target-os=linux \
--arch=$ARCH \
--enable-cross-compile \
--sysroot=$SYSROOT \
--extra-cflags="-Os -fpic $ADDI_CFLAGS" \
--extra-ldflags="$ADDI_LDFLAGS" \
$ADDITIONAL_CONFIGURE_FLAG
}
ARCH=arm
PREFIX=$(pwd)/android/$ARCH
ADDI_CFLAGS="-marm"
build_one
