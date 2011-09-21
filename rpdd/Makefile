#
# RedPorts Dispatcher Daemon
#

LOCALBASE?=/usr/local

CPPFLAGS=-I${LOCALBASE}/include
LDFLAGS=-L${LOCALBASE}/lib

all:	rpd

clean:
	@rm -f *.o

rpd:	database.o remote.o rpd.o steps.o util.o
	$(CC) $(LDFLAGS) -lcurl `mysql_config --libs` database.o remote.o rpd.o steps.o util.o -o rpd

database.o: database.c
	$(CC) $(CPPFLAGS) `mysql_config --cflags` -c database.c

remote.o: remote.c
	$(CC) $(CPPFLAGS) -c remote.c

rpd.o:	rpd.c util.o
	$(CC) $(CPPFLAGS) -c rpd.c

steps.o: steps.c
	$(CC) $(CPPFLAGS) `mysql_config --cflags` -c steps.c

util.o:	util.c
	$(CC) $(CPPFLAGS) -c util.c
