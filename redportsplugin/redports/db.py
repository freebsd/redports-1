from trac.core import *
from trac.db.schema import Table, Column, Index
from trac.env import IEnvironmentSetupParticipant

class RedportsEnvironment(Component):
    implements(IEnvironmentSetupParticipant)
    
    def __init__(self):
        self.schema = [
            Table('buildqueue', key=('id'))[
                Column('id', type='int', auto_increment=True),
                Column('owner'),
                Column('repository'),
                Column('revision', type='int'),
                Column('portname'),
                Column('status'),
                Column('startdate', type='bigint'),
                Column('enddate', type='bigint')
            ],
        ]

        self.db_version_key = 'redports_version'
        #   Increment this whenever there are DB changes
        self.db_version = 1
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
        """Called when a new Trac environment is created."""
        if self.environment_needs_upgrade(None):
            self.upgrade_environment(None)
    
    def environment_needs_upgrade(self, db):
        """Called when Trac checks whether the environment needs to be upgraded.
        
        Should return `True` if this participant needs an upgrade to be
        performed, `False` otherwise.

        """
        return self.system_needs_upgrade()

    def upgrade_environment(self, db):
        """Actually perform an environment upgrade.
        
        Implementations of this method should not commit any database
        transactions. This is done implicitly after all participants have
        performed the upgrades they need without an error being raised.
        """
        print 'Environment needs Upgrade'
        cursor = db.cursor()
        if self.db_installed_version < 1:
            for table in self.schema:
                for stmt in to_sql(self.env, table):
                    cursor.execute(stmt)

        cursor.execute("UPDATE system SET value = %s WHERE name = %s", (self.db_version, self.db_version_key))
        
        print 'Done Upgrading'

    
#   Taken from the fullblogplugin source
def to_sql(env, table):
    """ Convenience function to get the to_sql for the active connector."""
    from trac.db.api import DatabaseManager
    dm = env.components[DatabaseManager]
    dc = dm._get_connector()[0]
    return dc.to_sql(table)
