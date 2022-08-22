 
CC=gcc -g

CFLAGS= -c -I/usr/local/include/  -I/Users/radmin/MacPiserver/libvncserver-master/ -I/Users/radmin/MacPiserver/libvncserver-master/common/ -I/Users/radmin/MacPiserver/libimobiledevice-master/include/ -I/Users/radmin/MacPiserver/libimobiledevice-master/common -I/Users/radmin/MacPiserver/json-c/ -I/Users/radmin/MacPiserver/libxml2/

LDFLAGS=-L/usr/local/lib -L/usr/local/opt/openssl@1.1/lib


LDLIBS=-lpng -lz -lusbmuxd -lssl  -lcrypto -lgcrypt -lpthread  -lresolv -lcurl -lxml2
#-ljson-c -ljpeg 
#-limobiledevice -lvncserver

STATICLIBS=/usr/local/lib/libimobiledevice-1.0.a ../libvncserver-master/libvncserver/.libs/libvncserver.a /usr/local/lib/libjpeg.a  /usr/local/lib/libjson-c.dylib /usr/local/lib/libplist-2.0.a
SOURCES1=pevent.c pqueue.c instruments.c curlPost.c xmlparse.c decode.c
SOURCES2=offlineautomationserver.c instruments.c decode.c curlPost.c
SOURCES3=launchApp.c instruments.c curlPost.c xmlparse.c xcuitest.c
SOURCES4=idevicescreenshot.c decode.c curlPost.c
SOURCES5=idevicelocation.c
SOURCES6=idevicedebug.c
SOURCES7=touch.c curlPost.c xmlparse.c xcuitest.c

OBJECTS1=$(SOURCES1:.c=.o)
OBJECTS2=$(SOURCES2:.c=.o)
OBJECTS3=$(SOURCES3:.c=.o)
OBJECTS4=$(SOURCES4:.c=.o)
OBJECTS5=$(SOURCES5:.c=.o)
OBJECTS6=$(SOURCES6:.c=.o)
OBJECTS7=$(SOURCES7:.c=.o)

EXECUTABLE1=piserver
EXECUTABLE2=offlineautomationserver
EXECUTABLE3=launchApp
EXECUTABLE4=idevicescreenshot
EXECUTABLE5=idevicelocation
EXECUTABLE6=idevicelaunchdebug
EXECUTABLE7=touch

all: $(SOURCES1) $(SOURCES2) $(SOURCES3) $(SOURCES4) $(SOURCES5) $(EXECUTABLE1) $(EXECUTABLE2) $(EXECUTABLE3) $(EXECUTABLE4) $(EXECUTABLE5) $(SOURCES6) $(EXECUTABLE6) $(SOURCES7) $(EXECUTABLE7)

$(EXECUTABLE1): $(OBJECTS1)
	$(CC) $(OBJECTS1) $(STATICLIBS) $(LDFLAGS) $(LDLIBS) -o $@

$(EXECUTABLE2): $(OBJECTS2)
	$(CC) $(OBJECTS2) $(STATICLIBS) $(LDFLAGS) $(LDLIBS) -o $@
#	rm -f $(OBJECTS2) $(OBJECTS1)

$(EXECUTABLE3): $(OBJECTS3)
	$(CC) $(OBJECTS3) $(STATICLIBS) $(LDFLAGS) $(LDLIBS) -o $@
	rm -f $(OBJECTS3)

$(EXECUTABLE4): $(OBJECTS4)
	$(CC) $(OBJECTS4) $(STATICLIBS) $(LDFLAGS) $(LDLIBS) -o $@
	rm -f $(OBJECTS4)

$(EXECUTABLE5): $(OBJECTS5)
	$(CC) $(OBJECTS5) $(STATICLIBS) $(LDFLAGS) $(LDLIBS) -o $@
	rm -f $(OBJECTS5)

$(EXECUTABLE6): $(OBJECTS6)
	$(CC) $(OBJECTS6) $(STATICLIBS) $(LDFLAGS) $(LDLIBS) -o $@
	rm -f $(OBJECTS6)

$(EXECUTABLE7): $(OBJECTS7)
	$(CC) $(OBJECTS7) $(STATICLIBS) $(LDFLAGS) $(LDLIBS) -o $@
	rm -f $(OBJECTS7)

.c.o:
	$(CC) $(CFLAGS) $(CPPFLAGS) $< -o $@

clean:
	rm -f $(EXECUTABLE1) $(EXECUTABLE2) $(EXECUTABLE3) $(EXECUTABLE4) $(EXECUTABLE5) $(EXECUTABLE6) $(EXECUTABLE7)
