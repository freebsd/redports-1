from trac.core import *
from trac.util.datefmt import from_utimestamp, pretty_timedelta
import math

class Port(object):
    def __init__(self, env, id=None):
        self.env = env
        self.clear()

    def clear(self):
        self.id = None
	self.owner = None
	self.repository = None
        self.revision = None
	self.group = None
        self.portname = None
        self.status = None
        self.statusname = None
        self.startdate = None

    def setStatus(self, status, statusname):
        self.status = status

        if math.floor(status / 10) == 1:
            self.statusname = 'created'
        elif math.floor(status / 10) == 2:
            self.statusname = 'waiting'
        elif math.floor(status / 10) == 3:
            self.statusname = 'starting'
        elif math.floor(status / 10) == 5:
            self.statusname = 'building'
        elif math.floor(status / 10) == 7:
            self.statusname = 'transferring'
        elif math.floor(status / 10) == 8:
            self.statusname = 'cleaning'
        elif math.floor(status / 10) == 9:
            self.statusname = 'finished'
            self.finished = True
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
        cursor.execute("UPDATE builds SET status = %s WHERE backendkey = %s", ( status, key ))
        db.commit()

def PortsQueueIterator(env, req):
    cursor = env.get_db_cnx().cursor()
    #   Using the prepared db statement won't work if we have more than one entry in order_by
    cursor.execute("SELECT buildqueue.id, buildqueue.owner, buildqueue.repository, buildqueue.revision, builds.id, builds.group, buildqueue.portname, GREATEST(buildqueue.status, builds.status), builds.buildstatus, builds.startdate, IF(builds.enddate<builds.startdate,UNIX_TIMESTAMP()*1000000,builds.enddate) FROM buildqueue, builds WHERE buildqueue.id = builds.queueid AND owner = %s ORDER BY builds.id DESC", req.authname )
    for id, owner, repository, revision, buildid, group, portname, status, statusname, startdate, enddate in cursor:
        port = Port(env)
 	port.id = id
	port.owner = owner
	port.repository = repository
	port.revision = revision
	port.group = group
	port.portname = portname
        port.setStatus(status, statusname)
	port.startdate = pretty_timedelta( from_utimestamp(startdate), from_utimestamp(enddate) )
	port.directory = '/~%s/%s-%s' % ( owner, id, buildid )
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
        self.status = None
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
        buildgroup.status = 'error'
        if available > 0:
            buildgroup.status = 'ok'
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
