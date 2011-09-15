#
# RedPorts Dispatcher Daemon
#

LOCALBASE?=/usr/local

CPPFLAGS=-I${LOCALBASE}/include
LDFLAGS=-L${LOCALBASE}/lib

all:	rpd

clean:
	@rm -f *.o

rpd:	rpd.o remote.o util.o
	$(CC) $(LDFLAGS) -lcurl `mysql_config --libs` rpd.o remote.o util.o -o rpd

rpd.o:	rpd.c util.o
	$(CC) $(CPPFLAGS) `mysql_config --cflags` -c rpd.c

remote.o: remote.c
	$(CC) $(CPPFLAGS) -c remote.c

util.o:	util.c
	$(CC) $(CPPFLAGS) -c util.c
