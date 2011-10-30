#
# RedPorts Dispatcher Daemon
#

LOCALBASE?=/usr/local

CPPFLAGS=-I${LOCALBASE}/include
LDFLAGS=-L${LOCALBASE}/lib

all:	rpdd

clean:
	@rm -f *.o *.core

rpdd:	database.o remote.o rpdd.o steps.o util.o
	$(CC) $(LDFLAGS) -lcurl `mysql_config --libs` database.o remote.o rpdd.o steps.o util.o -o rpdd

database.o: database.c
	$(CC) $(CPPFLAGS) `mysql_config --cflags` -c database.c

remote.o: remote.c
	$(CC) $(CPPFLAGS) -c remote.c

rpdd.o:	rpdd.c util.o
	$(CC) $(CPPFLAGS) -c rpdd.c

steps.o: steps.c
	$(CC) $(CPPFLAGS) `mysql_config --cflags` -c steps.c

util.o:	util.c
	$(CC) $(CPPFLAGS) -c util.c
