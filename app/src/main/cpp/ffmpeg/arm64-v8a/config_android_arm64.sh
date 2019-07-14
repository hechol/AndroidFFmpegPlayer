#!/bin/bash
NDK=/home/hechol/android-ndk-r15c
SYSROOT=$NDK/platforms/android-21/arch-arm64/
TOOLCHAIN=$NDK/toolchains/aarch64-linux-android-4.9/prebuilt/linux-x86_64
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
--cross-prefix=$TOOLCHAIN/bin/aarch64-linux-android- \
--target-os=linux \
--arch=$ARCH \
--enable-cross-compile \
--sysroot=$SYSROOT \
--extra-cflags="-Os -fpic $ADDI_CFLAGS" \
--extra-ldflags="$ADDI_LDFLAGS" \
$ADDITIONAL_CONFIGURE_FLAG
}
ARCH=arm64
PREFIX=$(pwd)/android/$ARCH
build_one
