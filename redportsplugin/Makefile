# Trac redports plugin Makefile

all:	build rollout clean

build:
	python setup.py bdist_egg

clean:
	rm -fr build/ redports.egg-info/

rollout:
	cp -p dist/*.egg /var/db/trac/redports/plugins/

restart:
	killall python2.6 || true
	trac-admin /var/db/trac/redports upgrade
	sh /etc/rc.local
