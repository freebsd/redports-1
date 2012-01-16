# redports release Makefile

VERSION=0.9.92
MODULES=LICENSE README redports-trac rpdd rptinderbox scripts

all:	clean

clean:
	cd redports-trac && make clean
	cd rpdd && make clean
	rm -rf out/

release: clean
	mkdir -p out/redports-${VERSION}/
	cp -pR ${MODULES} out/redports-${VERSION}/
	chown -R root:wheel out/redports-${VERSION}/
	cd out/ && tar -cjf redports-${VERSION}.tar.bz2 redports-${VERSION}

