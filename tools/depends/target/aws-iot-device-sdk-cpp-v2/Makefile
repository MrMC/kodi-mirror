-include ../../Makefile.include
DEPS= ../../Makefile.include Makefile

# lib name
LIBNAME=libaws-crt-cpp
VERSION=v1.7.2

LIBDYLIB=$(PLATFORM)/build/aws-common-runtime/aws-crt-cpp/libaws-crt-cpp.a

CLEAN_FILES=$(PLATFORM)

all: .installed-$(PLATFORM)

$(PLATFORM): $(DEPS)
	rm -rf $(PLATFORM); mkdir -p $(PLATFORM)
	git clone --recursive https://github.com/aws/aws-iot-device-sdk-cpp-v2.git $(PLATFORM)
	cd $(PLATFORM); rm -rf build; mkdir -p build
	cd $(PLATFORM)/build; $(CMAKE) -DCMAKE_INSTALL_PREFIX=$(PREFIX) -DBUILD_SHARED_LIBS=OFF -DBUILD_SAMPLES=OFF -DBUILD_DEPS=ON -DCMAKE_INSTALL_LIBDIR=$(PREFIX)/lib/aws ../

$(LIBDYLIB): $(PLATFORM)
	$(MAKE) -C $(PLATFORM)/build

.installed-$(PLATFORM): $(LIBDYLIB)
	$(MAKE) -C $(PLATFORM)/build install
	touch $@

clean:
	rm -rf .installed-$(PLATFORM)

distclean::
	rm -rf $(PLATFORM) .installed-$(PLATFORM)
