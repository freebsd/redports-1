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
        self.enddate = None

    def setStatus(self, status):
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
        elif math.floor(status / 10) == 9:
            self.statusname = 'finished'
            self.finished = True
        else:
            self.statusname = 'unknown'

def PortsQueueIterator(env, req):
    cursor = env.get_db_cnx().cursor()
    #   Using the prepared db statement won't work if we have more than one entry in order_by
    cursor.execute("SELECT buildqueue.id, buildqueue.owner, buildqueue.repository, buildqueue.revision, builds.group, buildqueue.portname, buildqueue.status, builds.startdate, builds.enddate FROM buildqueue, builds WHERE buildqueue.id = builds.queueid AND owner = %s ORDER BY id DESC", req.authname )
    for id, owner, repository, revision, group, portname, status, startdate, enddate in cursor:
        port = Port(env)
 	port.id = id
	port.owner = owner
	port.repository = repository
	port.revision = revision
	port.group = group
	port.portname = portname
        port.setStatus(status)
	port.startdate = pretty_timedelta( from_utimestamp(startdate) )
	port.enddate = from_utimestamp(enddate)
        yield port

class Buildgroup(object):
   def __init__(self, env, id=None):
        self.env = env
        self.clear()

   def clear(self):
        self.name = None
        self.version = None
        self.arch = None
        self.type = None
        self.description = None
        self.available = None
        self.status = None

def BuildgroupsIterator(env, req):
   cursor = env.get_db_cnx().cursor()
   cursor.execute("SELECT name, version, arch, type, description, (SELECT count(*) FROM backendbuilds WHERE buildgroup = name AND status = 1) FROM buildgroups WHERE 1=1 ORDER BY version DESC, arch")

   for name, version, arch, type, description, available in cursor:
        buildgroup = Buildgroup(env)
        buildgroup.name = name
        buildgroup.version = version
        buildgroup.arch = arch
        buildgroup.type = type
        buildgroup.description = description
        buildgroup.available = available
        buildgroup.status = 'error'
        if available > 0:
            buildgroup.status = 'ok'
        yield buildgroup
