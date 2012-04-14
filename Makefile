OBJS = hemp.o dbus.o playlists.o

hemp : $(OBJS) 
	gcc -ldbus-glib-1 -ldbus-1 -lgstreamer-0.10 -lgobject-2.0 -lglib-2.0 $(OBJS) -o hemp 

%.o : %.c 
	gcc -std=gnu99 -c `pkg-config --cflags glib-2.0 dbus-glib-1 gstreamer-0.10` -o $@ $<

install : hemp
	install -d ${DESTDIR}/usr/bin
	install -d ${DESTDIR}/etc/pm/sleep.d
	install -d ${DESTDIR}/usr/lib/mime/packages
	install -d ${DESTDIR}/usr/share/man/man1
	install hemp ${DESTDIR}/usr/bin/hemp
	install 01hemp ${DESTDIR}/etc/pm/sleep.d
	install hemp.1 ${DESTDIR}/usr/share/man/man1
	install --mode=0644 hemp.mime ${DESTDIR}/usr/lib/mime/packages/hemp

clean :
	(cd debian && xargs rm -rf < .gitignore)
	rm -f *.swp *-stamp $(OBJS) hemp 
	rm -f ../hemp_*

.PHONY : clean
