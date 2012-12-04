CFLAGS=$(shell pkg-config --cflags gstreamer-0.10) -DGST_PACKAGE='"GStreamer"' -DGST_ORIGIN='"http://gstreamer.net"' -DVERSION='"0.0"' -DHAVE_USER_MTU -Wall -Wimplicit -g

libgsthttpsink.la: libgsthttpsink.lo
	libtool --mode=link gcc -module -avoid-version -rpath /usr/local/lib/gstreamer-0.10/ -export-symbols-regex gst_plugin_desc\
	 -o libgsthttpsink.la  libgsthttpsink.lo $(pkg-config --libs gstreamer-0.10)  -lgstbase-0.10

libgsthttpsink.lo: gsthttpsink.c
	libtool --mode=compile gcc $(CFLAGS) -o libgsthttpsink.lo -c $<

.PHONY: install

install: libgsthttpsink.la
	libtool --mode=install install libgsthttpsink.la /usr/local/lib/gstreamer-0.10/

clean:
	rm -rf *.o *.lo *.a *.la .libs