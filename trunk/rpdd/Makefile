#
# RedPorts Dispatcher Daemon
#

LOCALBASE?=/usr/local

CPPFLAGS=-I${LOCALBASE}/include
LDFLAGS=-L${LOCALBASE}/lib

all:	rpdd

install: rpdd
	install -o root -g wheel -m 0755 rpdd /usr/local/bin/rpdd
	install -o root -g wheel -m 0755 rpdd.in /usr/local/etc/rc.d/rpdd
	install -o root -g wheel -m 0644 rpdd.conf-dist /usr/local/etc/rpdd.conf-dist

clean:
	@rm -f *.o *.core

rpdd:	database.o log.o remote.o rpdd.o steps.o util.o
	$(CC) $(LDFLAGS) -lutil -lcurl -lpq database.o log.o remote.o rpdd.o steps.o util.o -o rpdd

database.o: database.c
	$(CC) $(CPPFLAGS) -c database.c

log.o: log.c
	$(CC) $(CPPFLAGS) -c log.c

remote.o: remote.c
	$(CC) $(CPPFLAGS) -c remote.c

rpdd.o:	rpdd.c util.o
	$(CC) $(CPPFLAGS) -c rpdd.c

steps.o: steps.c
	$(CC) $(CPPFLAGS) -c steps.c

util.o:	util.c
	$(CC) $(CPPFLAGS) -c util.c
