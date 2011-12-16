from trac.core import *
from trac.web.api import IRequestHandler
from trac.web.chrome import add_ctxtnav, add_script, add_stylesheet, INavigationContributor, ITemplateProvider
from trac.perm import IPermissionRequestor
from trac.util.translation import _

from genshi.builder import tag

from pkg_resources import resource_filename
import re

from model import BuildarchiveIterator
from buildqueue import render_ctxtnav

class BuildarchivePanel(Component):
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
        if re.match(r'^/buildarchive', req.path_info):
            return True

    def process_request(self, req):
        uriparts = req.path_info.split('/')

        add_stylesheet(req, 'common/css/admin.css')
        add_stylesheet(req, 'redports/redports.css')
        render_ctxtnav(req)

        if len(uriparts) == 2:
            sitename = 'Build Archive'

            if req.authname != 'anonymous':
                sitename = 'Your Build Archive'

            return ('buildarchive.html', 
                {   'builds': BuildarchiveIterator(self.env, req),
                    'sitename': sitename
                },  None)
        else:
            return ('buildarchivedetails.html', 
                {   'builds': BuildarchiveIterator(self.env, req, uriparts[2])
                },  None)

    def get_permission_actions(self):
        return ""

