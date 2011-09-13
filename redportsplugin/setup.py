#!/usr/bin/env python

from setuptools import setup

PACKAGE = 'redports'

setup(
    name=PACKAGE,
    description='Plugin which integrates an automatic FreeBSD package buildfarm',
    keywords='trac plugin freebsd tinderbox redports',
    version='0.3.90',
    url='http://www.bluelife.at/',
    license='http://www.opensource.org/licenses/mit-license.php',
    author='Bernhard Froehlich',
    author_email='decke@bluelife.at',
    long_description="""
	Integrates a frontend to view progress of a FreeBSD package buildfarm
    """,
#    zip_safe=True,
    packages=[PACKAGE],
    package_data={PACKAGE : ['templates/*.html', 'htdocs/*']},
    entry_points = {
        'trac.plugins': [
            'redports.db = redports.db',
            'redports.web_ui = redports.web_ui',
            'redports.buildgroups = redports.buildgroups',
        ]
    })
