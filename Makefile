TARGET=cdevreporter
OSX_BUNDLE_NAME=CDevReporter

ifdef LIBS_DIR
  LIBSDIR=$(LIBS_DIR)
else
  LIBSDIR=../staticlibs
endif

ifdef LIBWX_DIR
  LIBWXDIR=$(LIBWX_DIR)
endif

ifdef LIBPLIST_DIR
  LIBPLISTDIR=$(LIBPLIST_DIR)
else
  LIBPLISTDIR=../libplist
endif

ifdef LIBIMOBILEDEVICE_DIR
  LIBIMOBILEDEVICEDIR=$(LIBIMOBILEDEVICE_DIR)
else
  LIBIMOBILEDEVICEDIR=../libimobiledevice
endif

ifdef CURL_DIR
  CURLDIR=$(CURL_DIR)
else
  CURLDIR=../curl
endif

CFLAGS=-m32 -Wno-write-strings \
	-I$(LIBPLISTDIR)/include \
	-I$(LIBIMOBILEDEVICEDIR)/include \
	-I$(CURLDIR)/include \
	-DLIBXML_STATIC=1 -DCURL_STATICLIB=1 \
	-O3 -D_FILE_OFFSET_BITS=64 -D_LARGE_FILES -Wall

LDFLAGS=-m32 -static-libgcc -static-libstdc++

UNAME := $(shell uname)

SYSTYPE=
LDAPPEND=

FINALTASKS=
CLEAN_EXTRAS=

additional_objects=

ifeq ($(UNAME), Darwin)
  check_arch = $(shell if as -arch $(1) -o /dev/null < /dev/null > /dev/null; then echo yes; else echo no; fi)
  ifeq ($(call check_arch,i386), yes)
    CFLAGS+=-arch i386
  endif
  ifeq ($(call check_arch,ppc), yes)
    CFLAGS+=-arch ppc
  endif
  ifndef $(LIBWXDIR)
    LIBWXDIR=../wxWidgets-2.9.2
  endif
  CFLAGS+=-fPIC -DPIC -DHAVE_ASPRINTF -DHAVE_VASPRINTF \
	-isysroot /Developer/SDKs/MacOSX10.6.sdk \
	-DCURL_PULL_SYS_SOCKET_H \
	-D__WXOSX_COCOA__ -DwxDEBUG_LEVEL=0 \
	-I$(LIBWXDIR)/include -I$(LIBWXDIR)/lib/wx/include/osx_cocoa-unicode-static-2.9 \
	-pthread
  LDFLAGS+=-pthread
  LDAPPEND+=$(LIBSDIR)/osx/libwxpng-2.9.a \
	$(LIBSDIR)/osx/libwx_baseu-2.9.a \
	$(LIBSDIR)/osx/libwx_osx_cocoau_core-2.9.a \
	-lpthread -lz -lm -liconv \
	-framework IOKit -framework Carbon -framework Cocoa -framework QuickTime -framework OpenGL -framework System -framework Security
  SYSTYPE=osx
  FINALTASKS=@upx -9 $(TARGET); \
	make -C osx; \
	echo creating bundle; \
	mkdir -p osx/$(OSX_BUNDLE_NAME).app/Contents/MacOS; \
	sudo rm -f osx/$(OSX_BUNDLE_NAME).app/Contents/MacOS/* ; \
	cp $(TARGET) osx/$(OSX_BUNDLE_NAME).app/Contents/MacOS/$(OSX_BUNDLE_NAME)_ ; \
	cp osx/launcher osx/$(OSX_BUNDLE_NAME).app/Contents/MacOS/$(OSX_BUNDLE_NAME) ; \
	mkdir -p osx/$(OSX_BUNDLE_NAME).app/Contents/Resources ; \
	echo "APPL????" > osx/$(OSX_BUNDLE_NAME).app/Contents/PkgInfo ; \
	cp osx/Info.plist osx/$(OSX_BUNDLE_NAME).app/Contents/ ; \
	cp osx/reporter.icns osx/$(OSX_BUNDLE_NAME).app/Contents/Resources/ ; \
	echo done.
  CLEAN_EXTRAS=make -C osx clean
endif

ifeq ($(UNAME), Linux)
  ifndef $(LIBWXDIR)
    LIBWXDIR=../wxWidgets-2.9.2
  endif
  CFLAGS+=-fPIC -DPIC -DHAVE_ASPRINTF -DHAVE_VASPRINTF \
	-DwxDEBUG_LEVEL=0 \
	-I$(LIBWXDIR)/include \
	-I$(LIBWXDIR)/lib/wx/include/i686-linux-gnu-gtk2-unicode-static-2.9 \
	-D__WXGTK__ -pthread
  LDFLAGS+=-pthread -Wl,-Bsymbolic-functions -L/lib32 -L/usr/lib32
  LDAPPEND+=$(LIBSDIR)/linux/libwx_gtk2u_core-2.9.a \
	$(LIBSDIR)/linux/libwx_baseu-2.9.a \
	-lpthread -lz -lm -lglib-2.0 -lgtk-x11-2.0
  SYSTYPE=linux
endif

WIN32=
ifeq ($(findstring CYG,$(UNAME)), CYG)
  WIN32=1
  CC=gcc-3
endif
ifeq ($(findstring MINGW,$(UNAME)), MINGW)
  WIN32=1
  CC=gcc
endif
ifdef WIN32
  ifndef $(LIBWXDIR)
    LIBWXDIR=../wxWidgets-2.8.12
  endif
  CFLAGS+=-DWIN32 -D_WIN32 -D__LITTLE_ENDIAN__=1 \
	-DCURL_PULL_WS2TCPIP_H=1 \
	-I$(LIBWXDIR)/include \
	-I$(LIBWXDIR)/lib/wx/include/msw-unicode-release-static-2.8 \
	-D__WXMSW__ -DwxDEBUG_LEVEL=0 -mthreads
  LDFLAGS+=-mthreads -Wl,--subsystem,windows -mwindows
  LDAPPEND+=$(LIBSDIR)/win32/libwx_mswu_core-2.8.a \
	$(LIBSDIR)/win32/libwx_baseu-2.8.a \
	-lz -lm -lws2_32 -lgdi32 -lole32 -loleaut32 -luuid -lcomctl32 -lsetupapi
  SYSTYPE=win32
  additional_objects+=win32res.o
  FINALTASKS=@upx -9 $(TARGET).exe
endif

LDADD= \
	$(LIBSDIR)/$(SYSTYPE)/libimobiledevice.a \
        $(LIBSDIR)/$(SYSTYPE)/libplist.a \
        $(LIBSDIR)/$(SYSTYPE)/libxml2.a \
	$(LIBSDIR)/$(SYSTYPE)/libcurl.a \
        $(LIBSDIR)/$(SYSTYPE)/libssl.a \
        $(LIBSDIR)/$(SYSTYPE)/libcrypto.a \
        $(LIBSDIR)/$(SYSTYPE)/libusbmuxd.a \
        $(LIBSDIR)/$(SYSTYPE)/libplist.a \
        $(LIBSDIR)/$(SYSTYPE)/libxml2.a \
	$(LDAPPEND)

cxx_objects = main.o CDRMainWnd.o CDRWorker.o CDRFetcher.o

all: $(TARGET)

main.o: main.cpp
	$(CXX) $(CFLAGS) -c $^ -o $@

CDR%.o: CDR%.cpp
	$(CXX) $(CFLAGS) -c $^ -o $@

win32res.o: win32/res.rc win32/reporter.ico
	windres win32/res.rc -O coff $@

$(TARGET): $(cxx_objects) $(additional_objects)
	$(CXX) $(LDFLAGS) -o $@ $^ $(LDADD)
	$(FINALTASKS)

clean:
	rm -f $(TARGET) $(TARGET).exe *.o

