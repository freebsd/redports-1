from genshi.template.text import NewTextTemplate
from trac.core import Component, implements
from trac.web.chrome import ITemplateProvider, Chrome
from trac.config import BoolOption
from trac.notification import NotifyEmail
from pkg_resources import resource_filename

class BuildNotify(Component):
    implements(ITemplateProvider)

    def notify(self, build=None):
         email = BuildNotifyEmail(self.env)
         email.notify(build)

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

    def notify(self, build):
        self.build = build
        self.data.update(self.template_data())
        subject = '[redports] build finished'
        NotifyEmail.notify(self, build, subject)

    def get_recipients(self, resid):
        to = ['decke']
        cc = []
        return (to, cc)

    def send(self, torcpts, ccrcpts):
        mime_headers = {
            'X-Trac-Build-ID': self.build,
            'To': NotifyEmail.get_smtp_address(self, ''.join(torcpts)),
        }
        NotifyEmail.send(self, torcpts, ccrcpts, mime_headers)

    def template_data(self):
        return {
            'author': 'decke',
            'queueid': self.build,
        }
