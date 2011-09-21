# Trac redports plugin Makefile

all:	build rollout clean

build:
	python setup.py bdist_egg

clean:
	rm -fr build/ dist/ redports.egg-info/

rollout:
	cp -p dist/*.egg /var/db/trac/redports/plugins/

restart:
	/usr/local/etc/rc.d/tracd stop && sleep 3
	trac-admin /var/db/trac/redports upgrade
	/usr/local/etc/rc.d/tracd start
