from trac.core import *
from trac.prefs import IPreferencePanelProvider
from trac.web.chrome import add_notice, add_warning
from tools import *

from model import PortUserRepositoryIterator, PortRepository

class RedportsRepositoryPreferencePanel(Component):

    implements(IPreferencePanelProvider)

    # IPreferencePanelProvider methods
    def get_preference_panels(self, req):
        yield ('repository', 'Repositories')

    def render_preference_panel(self, req, panel):
        if req.method == 'POST':
            if req.args.get('delete') and req.args.get('id'):
                repo = PortRepository(self.env, req.args.get('id'))
	        if repo.load() and repo.type == "git" and repo.username == req.authname:
                    repo.delete()
                add_notice(req, 'Repository has been deleted')
                req.redirect(req.href.prefs(panel or None))
            elif req.args.get('add') and req.args.get('url'):
	        fullurl = "https://github.com/" + req.args.get('url').replace('.git', '')

                repo = PortRepository(self.env, None)
                repo.type = "git"
                repo.name = fullurl.replace('https://', '')
                repo.url = fullurl
                repo.browseurl = fullurl + "/commits/%REVISION%"
                repo.username = req.authname
                repo.add()
                add_notice(req, 'Repository has been added. Please don\'t forget to configure the webhook on github')
                req.redirect(req.href.prefs(panel or None))

            req.redirect(req.href.prefs(panel or None))

        return 'repoprefs.html', {
	    'repositories': PortUserRepositoryIterator(self.env, req.authname)
        }

