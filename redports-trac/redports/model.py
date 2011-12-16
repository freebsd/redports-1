from trac.core import *
from trac.util.datefmt import from_utimestamp, pretty_timedelta, format_datetime
from datetime import datetime
from time import time
import math

class PortRepository(object):
    def __init__(self, env, id):
        self.env = env
        self.clear()
        self.id = id

    def clear(self):
        self.id = None
        self.name = None
        self.type = None
        self.url = None
        self.browseurl = None

class Build(object):
    def __init__(self, env, id=None):
        self.env = env
        self.clear()

    def clear(self):
        self.queueid = None
        self.repository = None
        self.revision = None
        self.description = None
        self.runtime = None
        self.endtime = None
        self.owner = None
        self.priority = None
        self.ports = list()
        self.deletable = None
        self.status = None

    def setStatus(self, status):
        self.status = status

        if math.floor(self.status / 10) == 9:
            self.deletable = True

    def addBuild(self, groups, ports):
        db = self.env.get_db_cnx()
        cursor = db.cursor()

        self.status = 20

        if not self.queueid:
            self.queueid = datetime.now().strftime('%Y%m%d%H%M%S-%f')[0:20]

        if not self.revision:
            self.revision = None

        if not self.priority:
            self.priority = 5

        if not self.description:
            self.description = 'Web rebuild'

        cursor.execute("SELECT count(*) FROM portrepositories WHERE id = %s AND ( username = %s OR username IS NULL )", (
 self.repository, self.owner ))
        row = cursor.fetchone()
        if not row:
            raise TracError('SQL Error')
        if row[0] != 1:
            raise TracError('Invalid repository')

        if isinstance(groups, basestring):
            grouplist = list()
            grouplist.append(groups)
            groups = grouplist

        if isinstance(ports, basestring):
            ports = ports.split()
            ports.sort()

        if groups[0] == 'automatic':
            cursor.execute("SELECT buildgroup FROM automaticbuildgroups WHERE username = %s ORDER BY priority", (self.owner,) )
            if cursor.rowcount < 1:
                return False
            groups = cursor.fetchall()
        else:
            for group in groups:
                cursor.execute("SELECT name FROM buildgroups WHERE name = %s", (group,) )
                if cursor.rowcount != 1:
                    raise TracError('Invalid Buildqueue')

        cursor.execute("INSERT INTO buildqueue (id, owner, repository, revision, status, priority, startdate, enddate, description) VALUES (%s, %s, %s, %s, %s, %s, %s, %s, %s)", ( self.queueid, self.owner, self.repository, self.revision, self.status, self.priority, long(time()*1000000), 0, self.description ))

        for portname in ports:
            for group in groups:
                cursor.execute("INSERT INTO builds (queueid, backendkey, buildgroup, portname, pkgversion, status, buildstatus, buildreason, buildlog, wrkdir, backendid, startdate, enddate, checkdate) VALUES (%s, SUBSTRING(MD5(RANDOM()::text), 1, 25), %s, %s, null, %s, null, null, null, null, 0, 0, 0, 0)", ( self.queueid, group, portname, self.status ))
        db.commit()
        return True

    def delete(self, req):
        db = self.env.get_db_cnx()
        cursor = db.cursor()

        cursor.execute("SELECT count(*) FROM buildqueue WHERE buildqueue.id = %s AND buildqueue.owner = %s", ( self.queueid, req.authname ))
        row = cursor.fetchone()
        if not row:
            raise TracError('SQL Error')
        if row[0] != 1:
            raise TracError('Invalid QueueID')

        cursor.execute("SELECT status FROM buildqueue WHERE id = %s", (self.queueid,) )
        row = cursor.fetchone()
        if not row:
            raise TracError('SQL Error')
        if row[0] == 90:
            cursor.execute("UPDATE buildqueue SET status = 95 WHERE id = %s", (self.queueid,) )

        db.commit()


class Port(object):
    def __init__(self, env, id=None):
        self.env = env
        self.clear()

    def clear(self):
        self.id = None
        self.group = None
        self.portname = None
        self.pkgversion = None
        self.status = None
        self.buildstatus = 'unknown'
        self.statusname = None
        self.reason = None
        self.buildlog = None
        self.wrkdir = None
        self.startdate = None
        self.deletable = None

    def setStatus(self, status, statusname=None):
        self.status = status

        if not self.status:
            self.status = 20

        if math.floor(self.status / 10) == 2:
            self.statusname = 'waiting'
            self.deletable = True
        elif math.floor(self.status / 10) == 3:
            self.statusname = 'starting'
        elif math.floor(self.status / 10) == 5:
            self.statusname = 'building'
        elif math.floor(self.status / 10) == 7:
            self.statusname = 'transferring'
        elif math.floor(self.status / 10) == 8:
            self.statusname = 'cleaning'
        elif math.floor(self.status / 10) == 9:
            self.statusname = 'finished'
            self.deletable = True
        else:
            self.statusname = 'unknown'

        if statusname:
            self.statusname = statusname.lower()

    def updateStatus(self, status, key):
        db = self.env.get_db_cnx()
        cursor = db.cursor()
        cursor.execute("SELECT count(*) FROM builds WHERE backendkey = %s", (key,) )
        row = cursor.fetchone()
        if not row:
            raise TracError('SQL Error for key ' % key)
        if row[0] != 1:
            raise TracError('Invalid key')
        cursor.execute("UPDATE builds SET status = %s WHERE backendkey = %s AND status < %s", ( status, key, status ))
        db.commit()

    def delete(self, req):
        db = self.env.get_db_cnx()
        cursor = db.cursor()

        cursor.execute("SELECT count(*) FROM buildqueue, builds WHERE buildqueue.id = builds.queueid AND builds.id = %s AND buildqueue.owner = %s", ( self.id, req.authname ))
        row = cursor.fetchone()
        if not row:
            raise TracError('SQL Error')
        if row[0] != 1:
            raise TracError('Invalid ID')

        cursor.execute("SELECT buildqueue.id FROM buildqueue, builds WHERE buildqueue.id = builds.queueid AND builds.id = %s AND buildqueue.owner = %s", ( self.id, req.authname ))
        row = cursor.fetchone()
        if not row:
            raise TracError('SQL Error')
        if cursor.rowcount != 1:
            raise TracError('Invalid ID')

        queueid = row[0]

        cursor.execute("UPDATE builds SET status = 95 WHERE id = %s AND (status < 30 OR status > 89)", (self.id,) )

        cursor.execute("SELECT count(*) FROM builds WHERE queueid = %s AND status <= 90", (queueid,) )
        row = cursor.fetchone()
        if not row:
            raise TracError('SQL Error')
        if row[0] == 0:
            cursor.execute("UPDATE buildqueue SET status = 95 WHERE id = %s", (queueid,) )

        db.commit()


def BuildqueueIterator(env, req):
    cursor = env.get_db_cnx().cursor()
    cursor2 = env.get_db_cnx().cursor()

    cursor.execute("SELECT buildqueue.id, owner, replace(replace(browseurl, '%OWNER%', owner), '%REVISION%', revision::text), revision, status, startdate, enddate, description FROM buildqueue, portrepositories WHERE buildqueue.repository = portrepositories.id AND owner = %s AND buildqueue.status <= 90 ORDER BY buildqueue.id DESC", (req.authname,) )

    for queueid, owner, repository, revision, status, startdate, enddate, description in cursor:
        build = Build(env)
        build.queueid = queueid
        build.owner = owner
        build.repository = repository
        build.revision = revision
        build.setStatus(status)
        build.runtime = pretty_timedelta( from_utimestamp(startdate), from_utimestamp(enddate) )
        build.description = description

        cursor2.execute("SELECT id, buildgroup, portname, pkgversion, status, buildstatus, buildreason, buildlog, wrkdir, startdate, CASE WHEN enddate < startdate THEN extract(epoch from now())*1000000 ELSE enddate END FROM builds WHERE queueid = %s AND status <= 90 ORDER BY id", (queueid,) )

        lastport = None
        for id, group, portname, pkgversion, status, buildstatus, buildreason, buildlog, wrkdir, startdate, enddate in cursor2:
            port = Port(env)
            port.id = id
            port.group = group
            port.portname = portname
            port.pkgversion = pkgversion
            port.buildstatus = buildstatus
            port.buildlog = buildlog
            port.wrkdir = wrkdir
            port.startdate = pretty_timedelta( from_utimestamp(startdate), from_utimestamp(enddate) )
            port.directory = '/~%s/%s-%s' % ( owner, queueid, id )

            if buildstatus:
                port.buildstatus = buildstatus.lower()
            if buildstatus and not buildreason:
                buildreason = buildstatus.lower()

            port.setStatus(status, buildreason)

            if lastport != portname:
                port.head = True
                lastport = portname

            build.ports.append(port)

        yield build


class Buildgroup(object):
   def __init__(self, env, name):
        self.env = env
        self.clear()
        self.name = name

   def clear(self):
        self.name = None
        self.version = None
        self.arch = None
        self.type = None
        self.description = None
        self.available = None
        self.status = 'fail'
        self.priority = None
        self.priorityname = None
        self.joined = None
        self.queued = None

   def setPriority(self, priority):
        self.priority = 0
        self.priorityname = 'unknown'

        if int(priority) > 0 and int(priority) < 10:
            self.priority = int(priority)
        else:
            self.priority = 5

        if self.priority == 1:
            self.priorityname = 'highest'
        if self.priority == 3:
            self.priorityname = 'high'
        if self.priority == 5:
            self.priorityname = 'normal'
        if self.priority == 7:
            self.priorityname = 'low'
        if self.priority == 9:
            self.priorityname = 'lowest'

   def leave(self, req):
        db = self.env.get_db_cnx()
        cursor = db.cursor()
        cursor.execute("DELETE FROM automaticbuildgroups WHERE buildgroup = %s AND username = %s", ( self.name, req.authname ) )
        db.commit()

   def join(self, req):
        self.leave(req)
        db = self.env.get_db_cnx()
        cursor = db.cursor()
        cursor.execute("INSERT INTO automaticbuildgroups (username, buildgroup, priority) VALUES(%s, %s, %s)", ( req.authname, self.name, self.priority ) )
        db.commit()


def BuildgroupsIterator(env, req):
   cursor = env.get_db_cnx().cursor()
   cursor2 = env.get_db_cnx().cursor()
   cursor.execute("SELECT name, version, arch, type, description, (SELECT count(*) FROM backendbuilds, backends WHERE buildgroup = name AND backendbuilds.status = 1 AND backendbuilds.backendid = backends.id AND backends.status = 1) FROM buildgroups WHERE 1=1 ORDER BY version DESC, arch")

   for name, version, arch, type, description, available in cursor:
        buildgroup = Buildgroup(env, name)
        buildgroup.version = version
        buildgroup.arch = arch
        buildgroup.type = type
        buildgroup.description = description
        buildgroup.available = available

        if available > 0:
            buildgroup.status = 'success'
        if req.authname and req.authname != 'anonymous':
            cursor2.execute("SELECT priority FROM automaticbuildgroups WHERE username = %s AND buildgroup = %s", ( req.authname, name ) )

            if cursor2.rowcount > 0:
                buildgroup.joined = 'true'
                buildgroup.setPriority(cursor2.fetchall()[0][0])

        cursor2.execute("SELECT count(*) FROM builds WHERE buildgroup = %s AND status < 90", (name,) )
        if cursor2.rowcount > 0:
            buildgroup.queued = cursor2.fetchall()[0][0]
        
        yield buildgroup


def AvailableBuildgroupsIterator(env, req):
    cursor = env.get_db_cnx().cursor()
    cursor.execute("SELECT name FROM buildgroups WHERE name NOT IN (SELECT buildgroup FROM automaticbuildgroups WHERE username = %s) ORDER BY name", (req.authname,) )

    for name in cursor:
        buildgroup = Buildgroup(env, name)

        yield buildgroup


def AllBuildgroupsIterator(env):
    cursor = env.get_db_cnx().cursor()
    cursor.execute("SELECT name FROM buildgroups ORDER BY name")

    for name in cursor:
        buildgroup = Buildgroup(env, name)

        yield buildgroup


def RepositoryIterator(env, req):
    cursor = env.get_db_cnx().cursor()
    cursor.execute("SELECT id, replace(name, '%OWNER%', %s), type, url, replace(browseurl, '%OWNER%', %s) FROM portrepositories WHERE username IS NULL OR username = %s ORDER BY id", (req.authname, req.authname, req.authname))

    for id, name, type, url, browseurl in cursor:
        repository = PortRepository(env, id)
        repository.name = name
        repository.type = type
        repository.url = url
        repository.browseurl = browseurl

        yield repository


def BuildarchiveIterator(env, req):
    cursor = env.get_db_cnx().cursor()
    cursor2 = env.get_db_cnx().cursor()
    cursor.execute("SELECT buildqueue.id, replace(replace(portrepositories.browseurl, '%OWNER%', buildqueue.owner), '%REVISION%', buildqueue.revision::text), buildqueue.revision, description, startdate, CASE WHEN enddate < startdate THEN startdate ELSE enddate END FROM buildqueue, portrepositories WHERE buildqueue.repository = portrepositories.id AND (owner = %s OR %s = 'anonymous') AND buildqueue.status >= 90 ORDER BY buildqueue.id DESC LIMIT 100", (req.authname, req.authname) )

    for queueid, repository, revision, description, startdate, enddate in cursor:
        build = Build(env)
        build.queueid = queueid
        build.repository = repository
        build.revision = revision
        build.description = description
        build.runtime = pretty_timedelta( from_utimestamp(startdate), from_utimestamp(enddate) )
        build.endtime = format_datetime(enddate)

        cursor2.execute("SELECT id, portname, pkgversion, buildstatus FROM builds WHERE queueid = %s ORDER BY id", (queueid,) )

        for id, portname, pkgversion, buildstatus in cursor2:
            port = Port(env)
            port.id = id
            port.portname = portname
            port.pkgversion = pkgversion
            port.buildstatus = buildstatus
            build.ports.append(port)

        yield build
