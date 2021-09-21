#!/bin/bash
TOOLCHAIN=/opt/$target-toolchain
if [ $target != "gcw0" ]; then
	SYSROOT=$TOOLCHAIN/mipsel-$target-linux-musl
else
	SYSROOT=$TOOLCHAIN/mipsel-$target-linux-uclibc
fi

export PATH=$TOOLCHAIN/usr/bin:$SYSROOT/usr/include:$TOOLCHAIN/bin:$PATH
export CXX=mipsel-linux-g++
export CXXFLAGS="-funsigned-char" # workaround for a scummvm tolower() bug when adding games

if [ $target != "rs90" ]; then
	export LDFLAGS="-Wl,-zcommon-page-size=2097152,-zmax-page-size=2097152 -lhugetlbfs"
fi

CONFIG+="./configure --host=opendingux --enable-release --disable-detection-full"
CONFIG+=" --disable-alsa --disable-vorbis --disable-mt32emu --disable-nuked-opl --disable-tts"
CONFIG+=" --disable-cloud --disable-sdlnet --disable-libcurl"
# alsa crackles when system is under stress, and vorbis is much slower than tremor

case $target in
	
	gcw0)
	;;

	lepus)
	CONFIG+=" --disable-highres"
	;;

	rs90)
	CONFIG+=" --disable-highres --disable-16bit"
	;;
	
	*)
	echo "please provide a valid target for the build: gcw0, lepus or rs90"
	exit 0
	;;
esac

$CONFIG && make -j12 od-make-opk && ls -lh scummvm_$target.opk
