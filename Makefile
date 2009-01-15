#Makefile for stress

CFLAGS = -g -O2 -Wall -D_GNU_SOURCE `curl-config --cflags`
LDFLAGS = -lghttp `curl-config --libs` -lpthread

SOURCES = stress.c ghttp.c curl.c
OBJECTS = stress.o ghttp.o curl.o
STATICS = /usr/lib/libcurl.a /usr/lib/libz.a /usr/lib/libssl.a /usr/lib/libcrypto.a /usr/lib/libghttp.a /usr/lib/libdl.a /usr/lib/libpthread.a

all: static dynamic 
	
dynamic: ${OBJECTS} Makefile
	gcc ${LDFLAGS} -o stress ${OBJECTS}

static: ${OBJECTS}
	gcc ${LDFLAGS} -static -o stress-static ${OBJECTS} ${STATICS}

.c.o: ${SOURCES} Makefile
	gcc ${CFLAGS} -c $< -o $@
    
clean: 
	rm -f *.o core stress stress-static
