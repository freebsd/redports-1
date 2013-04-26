from trac.core import *
from trac.web.api import IRequestHandler
from trac.web.chrome import add_ctxtnav, add_script, add_stylesheet, INavigationContributor, ITemplateProvider
from trac.perm import IPermissionRequestor
from trac.util.translation import _

from genshi.builder import tag

from pkg_resources import resource_filename
import re

from model import BuildstatsAllIterator, BuildstatsUserIterator

class BuildstatsPanel(Component):
    implements(INavigationContributor, ITemplateProvider, IRequestHandler, IPermissionRequestor)

    def get_active_navigation_item(self, req):
        return 'buildqueue'

    def get_navigation_items(self, req):
        return ""

    def get_htdocs_dirs(self):
        return [('redports', resource_filename('redports', 'htdocs'))]

    def get_templates_dirs(self):
        return [resource_filename('redports', 'templates')]

    def match_request(self, req):
        if re.match(r'^/buildstats', req.path_info):
            return True

    def process_request(self, req):
        add_stylesheet(req, 'redports/redports.css')
        add_script(req, 'redports/flot/jquery.flot.js')
        add_ctxtnav(req, _('Environments'), req.href.buildgroups())
        add_ctxtnav(req, _('Statistics'), req.href.buildstats())

        if req.args.get('owner', None) != None:
            username = req.args.get('owner', None)
        elif req.authname != 'anonymous':
            username = req.authname
        else:
            username = None
        

        return ('buildstats.html', 
            {   'alljobs': BuildstatsAllIterator(self.env),
                'userjobs': BuildstatsUserIterator(self.env, username),
                'userjobname': username,
                'authname': req.authname
            },  None)

    def get_permission_actions(self):
        return ""

