#
# RedPorts Dispatcher Daemon
#

LOCALBASE?=/usr/local
PREFIX?=${LOCALBASE}

CPPFLAGS=-I${LOCALBASE}/include -g \
	 -std=gnu99 -fstack-protector -Wsystem-headers -Wall -Wno-format-y2k \
	 -W -Wno-unused-parameter -Wstrict-prototypes -Wmissing-prototypes \
	 -Wpointer-arith -Wno-uninitialized -Wno-pointer-sign -Wno-empty-body \
	 -Wno-string-plus-int -Wno-tautological-compare -Wno-unused-value \
	 -Wno-conversion -Wno-parentheses-equality -Wno-unused-function \
	 -Wshadow
LDFLAGS=-L${LOCALBASE}/lib

SRC =	database.c log.c remote.c rpdd.c steps.c stephelper.c steputil.c util.c
OBJS =	${SRC:.c=.o}

all:	rpdd

install: rpdd
	install -o root -g wheel -m 0755 rpdd ${PREFIX}/bin/rpdd
	install -o root -g wheel -m 0755 rpdd.in ${PREFIX}/etc/rc.d/rpdd
	install -o root -g wheel -m 0644 rpdd.conf-dist ${PREFIX}/etc/rpdd.conf-dist
	install -o root -g wheel -m 0755 -d ${PREFIX}/share/rpdd
	install -o root -g wheel -m 0755 scripts/hook.sh.tmpl ${PREFIX}/share/rpdd/hook.sh.tmpl

clean:
	@rm -f *.o *.core

rpdd:	${OBJS}
	$(CC) $(LDFLAGS) -lutil -lcurl -lpq ${OBJS} -o rpdd

# SUFFIX RULES
.SUFFIXES: .c .o

.c.o:
	$(CC) $(CPPFLAGS) -c ${.IMPSRC}
