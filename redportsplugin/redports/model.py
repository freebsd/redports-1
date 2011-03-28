from trac.core import *
from trac.util.datefmt import from_utimestamp, pretty_timedelta

class Port(object):
    def __init__(self, env, id=None):
        self.env = env
        self.clear()

    def clear(self):
        self.id = None
	self.owner = None
	self.repository = None
        self.revision = None
        self.portname = None
        self.status = None
        self.startdate = None
        self.enddate = None

def PortsQueueIterator(env, req):
    cursor = env.get_db_cnx().cursor()
    #   Using the prepared db statement won't work if we have more than one entry in order_by
    cursor.execute("SELECT id, owner, repository, revision, portname, status, startdate, enddate FROM buildqueue WHERE owner = %s ORDER BY id DESC", req.authname )
    for id, owner, repository, revision, portname, status, startdate, enddate in cursor:
        port = Port(env)
 	port.id = id
	port.owner = owner
	port.repository = repository
	port.revision = revision
	port.portname = portname
	port.status = status
	port.startdate = pretty_timedelta( from_utimestamp(startdate) )
	port.enddate = from_utimestamp(enddate)
        yield port
        
