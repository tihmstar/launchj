CFILES=daemonloader.c
SDK=~/SDK/iPhoneOS10.3.sdk
ARCH=armv7

all: daemonloader.dylib launchj loader

.PHONY: all clean

daemonloader.dylib: daemonloader.c
	xcrun -sdk iphoneos clang -arch $(ARCH) -shared $^ -o $@ -isysroot $(SDK) -framework CoreFoundation
	ldid -S $@

launchj: launchj.c
	xcrun -sdk iphoneos clang -arch $(ARCH) $^ -o $@ -isysroot $(SDK)
	ldid -S $@

loader: loader.c
	xcrun -sdk iphoneos clang -arch $(ARCH) $^ -o $@ -isysroot $(SDK)
	ldid -S $@

clean:
	rm -f daemonloader.dylib launchj loader
