 
CC=gcc -g 

CFLAGS=-c -I/usr/local/include/ -I/usr/local/opt/openssl/include/ -I/Users/ssts/Downloads/libimobiledevice-master/include/ -I/Users/ssts/Downloads/libimobiledevice-master/common/

LDFLAGS=-L/usr/local/lib  -L/usr/local/opt/openssl/lib/

LDLIBS=-lpng -lz -lusbmuxd -lplist -lssl -lcrypto -lgcrypt -lpthread -lgnutls -lresolv 

STATICLIBS=/usr/local/lib/libimobiledevice.a
SOURCES1=idevicelocation.c

OBJECTS1=$(SOURCES1:.c=.o)

EXECUTABLE1=idevicelocation

all: $(SOURCES1) $(EXECUTABLE1) 

$(EXECUTABLE1): $(OBJECTS1)
	$(CC) $(OBJECTS1) $(STATICLIBS) $(LDFLAGS) $(LDLIBS) -o $@
	rm -f $(OBJECTS1)

.c.o:
	$(CC) $(CFLAGS) $(CPPFLAGS) $< -o $@

clean:
	rm -f $(EXECUTABLE1) 
