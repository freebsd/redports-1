from genshi.template.text import NewTextTemplate
from trac.core import Component, implements
from trac.web.chrome import ITemplateProvider, Chrome
from trac.config import BoolOption
from trac.notification import NotifyEmail
from pkg_resources import resource_filename
from model import BuildarchiveIterator

class BuildNotify(Component):
    implements(ITemplateProvider)

    def notify(self, queueid):
        session = self.getSession(queueid)
        email = BuildNotifyEmail(self.env)

        # email not verified or no email set
        if not session.get('email') or session.get('email_verification_token'):
            return False

        if not session.get('build_notifications'):
            return False

        email.load(queueid)

        if email.notifyEnabled(session):
            email.notify()

    # ITemplateProvider methods
    def get_templates_dirs(self):
        return [resource_filename('redports', 'templates')]

    def get_htdocs_dirs(self):
        return []

    def getSession(self, queueid):
        cursor = self.env.get_db_cnx().cursor()

        cursor.execute("SELECT status, owner FROM buildqueue WHERE id = %s", (queueid,))
        if cursor.rowcount != 1:
            return False

        row = cursor.fetchone()

        if row[0] != 90:
            return False

        session = DetachedSession(self.env, row[1])

        if not session:
            return None

        return session


class BuildNotifyEmail(NotifyEmail):
    template_name = 'notify_email.txt'

    def __init__(self, env):
        NotifyEmail.__init__(self, env)
        # Override the template type to always use NewTextTemplate
        if not isinstance(self.template, NewTextTemplate):
            self.template = Chrome(env).templates.load(
                                self.template.filepath, cls=NewTextTemplate)

    def load(self, queueid):
        self.queueid = queueid
        self.data.update(self.template_data())
        self.subject = '[%s] Build %s completed: %s' % (self.env.project_name, self.queueid, self.status)

    def notify(self):
        if not self.queueid or not self.build:
            return False

        NotifyEmail.notify(self, self.queueid, self.subject)

    def get_recipients(self, resid):
        to = [self.build.owner]
        cc = []
        return (to, cc)

    def send(self, torcpts, ccrcpts):
        mime_headers = {
            'X-Trac-Build-ID': self.queueid,
            'X-Trac-Build-URL': self.build_link(),
            'To': NotifyEmail.get_smtp_address(self, ''.join(torcpts)),
        }
        NotifyEmail.send(self, torcpts, ccrcpts, mime_headers)

    def build_link(self):
        return self.env.abs_href.buildarchive(self.queueid)

    def genstatus(self):
        statusdict = {}
        for port in self.build.ports:
            if port.statusname not in statusdict:
                statusdict[port.statusname] = 0
            statusdict[port.statusname] += 1

        self.status = ''
        for stat, num in statusdict.iteritems():
            self.status += '%dx %s, ' % (num, stat)
        self.status = self.status.rstrip(', ')

    def template_data(self):
        builds = BuildarchiveIterator(self.env)
        builds.filter(None, self.queueid)
        self.build = builds.get_data().next()
        self.genstatus()

        return {
            'queueid': self.queueid,
            'build': self.build,
            'buildurl': self.build_link(),
            'status': self.status,
        }

    def notifyEnabled(self, session):
        if session.get('build_notifications_failed'):
            for port in self.build.ports:
                if port.statusname == 'fail':
                    return True
        elif session.get('build_notifications_success'):
            return True
        else:
            return False
