#
# RedPorts Dispatcher Daemon
#

LOCALBASE?=/usr/local

CPPFLAGS=-I${LOCALBASE}/include -g -Wall -Wno-format
LDFLAGS=-L${LOCALBASE}/lib

SRC =	database.c log.c remote.c rpdd.c steps.c stephelper.c steputil.c util.c
OBJS =	${SRC:.c=.o}

all:	rpdd

install: rpdd
	install -o root -g wheel -m 0755 rpdd /usr/local/bin/rpdd
	install -o root -g wheel -m 0755 rpdd.in /usr/local/etc/rc.d/rpdd
	install -o root -g wheel -m 0644 rpdd.conf-dist /usr/local/etc/rpdd.conf-dist
	install -o root -g wheel -m 0755 -d /usr/local/share/rpdd
	install -o root -g wheel -m 0755 scripts/hook.sh.tmpl /usr/local/share/rpdd/hook.sh.tmpl

clean:
	@rm -f *.o *.core

rpdd:	${OBJS}
	$(CC) $(LDFLAGS) -lutil -lcurl -lpq ${OBJS} -o rpdd

# SUFFIX RULES
.SUFFIXES: .c .o

.c.o:
	$(CC) $(CPPFLAGS) -c ${.IMPSRC}
