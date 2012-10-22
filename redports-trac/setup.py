#!/usr/bin/env python

from setuptools import setup

PACKAGE = 'redports'

setup(
    name=PACKAGE,
    description='Plugin which integrates an automatic FreeBSD package buildfarm',
    keywords='trac plugin freebsd tinderbox redports',
    version='1.0.91',
    url='http://www.bluelife.at/',
    license='http://www.opensource.org/licenses/bsd-license.php',
    author='Bernhard Froehlich',
    author_email='decke@bluelife.at',
    long_description="""
        Integrates a frontend to view progress of a FreeBSD package buildfarm
    """,
    zip_safe=True,
    packages=[PACKAGE],
    package_data={PACKAGE : ['templates/*', 'htdocs/flot/*', 'htdocs/*.png']},
    entry_points = {
        'trac.plugins': [
            'redports.db = redports.db',
            'redports.admin = redports.admin',
            'redports.backend = redports.backend',
            'redports.buildarchive = redports.buildarchive',
            'redports.buildgroups = redports.buildgroups',
            'redports.buildqueue = redports.buildqueue',
            'redports.buildstats = redports.buildstats',
            'redports.preferences = redports.preferences',
            'redports.tools = redports.tools',
            'redports.notify = redports.notify',
        ]
    })
