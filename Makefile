# redports release Makefile

VERSION=0.8.92

all:	clean

clean:
	cd redports-trac && make clean
	cd rpdd && make clean
	rm -f redports-*.tar.bz2

release: clean
	tar -cjf redports-${VERSION}.tar.bz2 --exclude .svn *

