# Trac redports plugin Makefile

all:	build

build:
	python setup.py bdist_egg

clean:
	rm -fr build/ dist/ redports.egg-info/

install: build
	chown www:www dist/*.egg
	cp -p dist/*.egg /var/db/trac/redports/plugins/

restart:
	/usr/local/etc/rc.d/uwsgi stop && sleep 3
	trac-admin /var/db/trac/redports upgrade
	/usr/local/etc/rc.d/uwsgi start
