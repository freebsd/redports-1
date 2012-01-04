from trac.core import *
from trac.db.schema import Table, Column, Index
from trac.env import IEnvironmentSetupParticipant

class RedportsEnvironment(Component):
    implements(IEnvironmentSetupParticipant)
    
    def __init__(self):
        self.schema1 = [
            Table('buildqueue', key=('id', 'owner', 'status'))[
                Column('id', type='varchar(25)', size=25),
                Column('owner', type='varchar(50)', size=50),
                Column('repository', type='int'),
                Column('revision', type='int'),
                Column('status', type='int'),
                Column('priority', type='int'),
                Column('startdate', type='bigint'),
                Column('enddate', type='bigint'),
                Column('description', type='varchar(255)', size=255)
            ],
            Table('builds', key=('id', 'queueid', 'backendkey', 'buildgroup', 'status', 'backendid'))[
                Column('id', type='int', auto_increment=True),
                Column('queueid', type='varchar(25)', size=25),
                Column('backendkey', type='varchar(25)', size=25),
                Column('buildgroup', type='varchar(50)', size=50),
                Column('portname', type='varchar(50)', size=50),
                Column('pkgversion', type='varchar(25)', size=25),
                Column('status', type='int'),
                Column('buildstatus', type='varchar(25)', size=25),
                Column('buildreason', type='varchar(100)', size=100),
                Column('buildlog', type='varchar(50)', size=50),
                Column('wrkdir', type='varchar(50)', size=50),
                Column('backendid', type='int'),
                Column('startdate', type='bigint'),
                Column('enddate', type='bigint'),
                Column('checkdate', type='bigint')
            ],
            Table('backends', key=('id', 'status'))[
                Column('id', type='int', auto_increment=True),
                Column('host', type='varchar(50)', size=50),
                Column('protocol', type='varchar(10)', size=10),
                Column('uri', type='varchar(25)', size=25),
                Column('credentials', type='varchar(50)', size=50),
                Column('maxparallel', type='int'),
                Column('status', type='int'),
                Column('type', type='varchar(25)', size=25)
            ],
            Table('backendbuilds', key=('id', 'status', 'backendid', 'buildgroup'))[
                Column('id', type='int', auto_increment=True),
                Column('buildgroup', type='varchar(25)', size=25),
                Column('backendid', type='int'),
                Column('priority', type='int'),
                Column('status', type='int'),
                Column('buildname', type='varchar(30)', size=30)
            ],
            Table('buildgroups', key=('name'))[
                Column('name', type='varchar(50)', size=50),
                Column('version', type='varchar(25)', size=25),
                Column('arch', type='varchar(10)', size=10),
                Column('type', type='varchar(25)', size=25),
                Column('description', type='varchar(255)', size=255),
            ],
            Table('automaticbuildgroups')[
                Column('username', type='varchar(50)', size=50),
                Column('buildgroup', type='varchar(25)', size=25),
                Column('priority', type='int'),
            ],
        ]

        self.schema2 = [
            Table('portrepositories', key=('id'))[
                Column('id', type='int', auto_increment=True),
                Column('name', type='varchar(255)', size=255),
                Column('type', type='varchar(25)', size=25),
                Column('url', type='varchar(255)', size=255),
                Column('browseurl', type='varchar(255)', size=255),
                Column('username', type='varchar(50)', size=50)
            ],
        ]

        self.db_version_key = 'redports_version'
        #   Increment this whenever there are DB changes
        self.db_version = 2
        self.db_installed_version = None

        #   Check DB to see if we need to add new tables, etc.
        db = self.env.get_db_cnx()
        cursor = db.cursor()
        cursor.execute("SELECT value FROM system WHERE name=%s", (self.db_version_key,))
        try:
            self.db_installed_version = int(cursor.fetchone()[0])
        except: #   Version did not exists, so the plugin was just installed
            self.db_installed_version = 0
            cursor.execute("INSERT INTO system (name, value) VALUES (%s, %s)", (self.db_version_key,
                self.db_installed_version))
            db.commit()
            db.close()

    def system_needs_upgrade(self):
        return self.db_installed_version < self.db_version

    #   IEnvironmentSetupParticipant methods
    def environment_created(self):
        if self.environment_needs_upgrade(None):
            self.upgrade_environment(None)
    
    def environment_needs_upgrade(self, db):
        return self.system_needs_upgrade()

    def upgrade_environment(self, db):
        print 'Environment needs Upgrade'
        cursor = db.cursor()
        if self.db_installed_version < 1:
            for table in self.schema1:
                for stmt in to_sql(self.env, table):
                    cursor.execute(stmt)
        if self.db_installed_version < 2:
            for table in self.schema2:
                for stmt in to_sql(self.env, table):
                    cursor.execute(stmt)
            cursor.execute("ALTER TABLE buildqueue DROP repository")
            cursor.execute("ALTER TABLE buildqueue ADD COLUMN repository integer")

        cursor.execute("UPDATE system SET value = %s WHERE name = %s", (self.db_version, self.db_version_key))
        print 'Done Upgrading'

    
def to_sql(env, table):
    from trac.db.api import DatabaseManager
    dm = env.components[DatabaseManager]
    dc = dm._get_connector()[0]
    return dc.to_sql(table)
