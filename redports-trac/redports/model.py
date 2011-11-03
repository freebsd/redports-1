from trac.core import *
from trac.util.datefmt import from_utimestamp, pretty_timedelta
import math

class Port(object):
    def __init__(self, env, id=None):
        self.env = env
        self.clear()

    def clear(self):
        self.id = None
        self.queueid = None
	self.owner = None
	self.repository = None
        self.revision = None
	self.group = None
        self.portname = None
        self.status = None
        self.buildstatus = 'unknown'
        self.statusname = None
        self.reason = None
        self.buildlog = None
        self.wrkdir = None
        self.startdate = None
        self.head = None

    def addPort(self):
        db = self.env.get_db_cnx()
        cursor = db.cursor()

        cursor.execute("SELECT CONCAT(DATE_FORMAT(NOW(), '%Y%m%d%H%i%s'), '-', RPAD(SUBSTRING(FLOOR(RAND()*1000000), 1, 5), 5, '0'))")
        row = cursor.fetchone()
        if not row:
            raise TracError('SQL Error while generating new buildqueue id')

        self.queueid = row[0]

        if self.group == 'automatic':
            self.setStatus(10)
        else:
            self.setStatus(20)

            cursor.execute("SELECT count(*) FROM buildgroups WHERE name = %s", self.group )
            row = cursor.fetchone()
            if not row:
                raise TracError('SQL Error')
            if row[0] != 1:
                raise TracError('Invalid buildgroup')

        cursor.execute("INSERT INTO buildqueue (id, owner, repository, revision, portname, status, startdate, enddate) VALUES (%s, %s, %s, %s, %s, %s, UNIX_TIMESTAMP()*1000000, 0)", ( self.queueid, self.owner, self.repository, self.revision, self.portname, self.status ))

        if self.status == 20:
             cursor.execute("INSERT INTO builds VALUES ( null, '%s', SUBSTRING(MD5(RAND()), 1, 25), '%s', 20, null, null, null, null, 0, 0, 0 )" % ( self.queueid, self.group ))
        db.commit()

    def setStatus(self, status, statusname=None):
        self.status = status

        if not self.status:
            self.status = 10

        if math.floor(self.status / 10) == 1:
            self.statusname = 'created'
            self.deletable = True
        elif math.floor(self.status / 10) == 2:
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
            self.finished = True
            self.deletable = True
        else:
            self.statusname = 'unknown'

        if statusname:
            self.statusname = statusname.lower()

    def updateStatus(self, status, key):
        db = self.env.get_db_cnx()
        cursor = db.cursor()
        cursor.execute("SELECT count(*) FROM builds WHERE backendkey = %s", key)
        row = cursor.fetchone()
        if not row:
            raise TracError('SQL Error for key ' % key)
        if row[0] != 1:
            raise TracError('Invalid key')
        cursor.execute("UPDATE builds SET status = %s WHERE backendkey = %s AND status < %s", ( status, key, status ))
        db.commit()

    def deleteBuildQueueEntry(self, req):
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

        self.queueid = row[0]
        
        cursor.execute("UPDATE builds SET status = 95 WHERE id = %s AND (status < 30 OR status > 89)", self.id )

        cursor.execute("SELECT count(*) FROM builds WHERE queueid = %s AND status <= 90", self.queueid )
        row = cursor.fetchone()
        if not row:
            raise TracError('SQL Error')
        if row[0] == 0:
            cursor.execute("UPDATE buildqueue SET status = 95 WHERE id = %s", self.queueid )
        db.commit()
        

def PortsQueueIterator(env, req):
    cursor = env.get_db_cnx().cursor()
    cursor.execute("SELECT builds.id, buildqueue.owner, buildqueue.repository, buildqueue.revision, buildqueue.id, builds.group, buildqueue.portname, GREATEST(buildqueue.status, builds.status, 0), builds.buildstatus, builds.buildreason, builds.buildlog, builds.wrkdir, builds.startdate, IF(builds.enddate<builds.startdate,UNIX_TIMESTAMP()*1000000,builds.enddate) FROM buildqueue LEFT OUTER JOIN builds ON buildqueue.id = builds.queueid WHERE owner = %s AND buildqueue.status <= 90 AND (builds.status IS NULL OR builds.status <= 90) ORDER BY buildqueue.id DESC, builds.id", req.authname )
    lastid = None
    for id, owner, repository, revision, queueid, group, portname, status, buildstatus, buildreason, buildlog, wrkdir, startdate, enddate in cursor:
	port = Port(env)
 	port.id = id
	port.queueid = queueid
	port.owner = owner
	port.repository = repository
	port.revision = revision
	port.group = group
	port.portname = portname
        if buildstatus:
            port.buildstatus = buildstatus.lower()
        if buildstatus and not buildreason:
            buildreason = buildstatus.lower()
	port.setStatus(status, buildreason)
	port.buildlog = buildlog
	port.wrkdir = wrkdir
	port.startdate = pretty_timedelta( from_utimestamp(startdate), from_utimestamp(enddate) )
	port.directory = '/~%s/%s-%s' % ( owner, queueid, id )
        if lastid != queueid:
            port.head = True
            lastid = queueid
	yield port


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
        
        yield buildgroup

def AvailableBuildgroupsIterator(env, req):
   cursor = env.get_db_cnx().cursor()
   cursor.execute("SELECT name FROM buildgroups WHERE name NOT IN (SELECT buildgroup FROM automaticbuildgroups WHERE username = %s) ORDER BY name", req.authname )

   for name in cursor:
        buildgroup = Buildgroup(env, name)

        yield buildgroup

def AllBuildgroupsIterator(env):
   cursor = env.get_db_cnx().cursor()
   cursor.execute("SELECT name FROM buildgroups ORDER BY name")

   for name in cursor:
        buildgroup = Buildgroup(env, name)

        yield buildgroup

