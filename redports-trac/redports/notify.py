from genshi.template.text import NewTextTemplate
from trac.core import Component, implements
from trac.web.chrome import ITemplateProvider, Chrome
from trac.config import BoolOption
from trac.notification import NotifyEmail
from pkg_resources import resource_filename
from model import BuildarchiveIterator

class BuildNotify(Component):
    implements(ITemplateProvider)

    def notify(self, queueid=None):
         email = BuildNotifyEmail(self.env)
         email.notify(queueid)

    # ITemplateProvider methods
    def get_templates_dirs(self):
        return [resource_filename('redports', 'templates')]

    def get_htdocs_dirs(self):
        return []


class BuildNotifyEmail(NotifyEmail):
    template_name = 'notify_email.txt'

    def __init__(self, env):
        NotifyEmail.__init__(self, env)
        # Override the template type to always use NewTextTemplate
        if not isinstance(self.template, NewTextTemplate):
            self.template = Chrome(env).templates.load(
                                self.template.filepath, cls=NewTextTemplate)

    def notify(self, queueid):
        self.queueid = queueid
        self.data.update(self.template_data())
        subject = '[%s] Build %s completed: %s' % (self.env.project_name, self.queueid, self.status)
        NotifyEmail.notify(self, self.queueid, subject)

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
        builds = BuildarchiveIterator(self.env, None, self.queueid)
        self.build = builds.next()
        self.genstatus()

        return {
            'queueid': self.queueid,
            'build': self.build,
            'buildurl': self.build_link(),
            'status': self.status,
        }
