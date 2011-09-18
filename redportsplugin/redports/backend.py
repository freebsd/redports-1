from trac.core import *
from trac.web.api import IRequestHandler
from trac.web.chrome import add_script, add_stylesheet, INavigationContributor, ITemplateProvider
from trac.perm import IPermissionRequestor
from trac.util.translation import _

from genshi.builder import tag

import re

from model import Port

class BackendConnector(Component):
    implements(INavigationContributor, ITemplateProvider, IRequestHandler, IPermissionRequestor)

    def get_active_navigation_item(self, req):
        return ""

    def get_navigation_items(self, req):
        return []

    def get_htdocs_dirs(self):
        return []

    def get_templates_dirs(self):
        return []

    def match_request(self, req):
        if re.match(r'^/backend', req.path_info):
            return True

    def process_request(self, req):
        port = Port(self.env)
        port.updateStatus(70, req.args.get('key'))

        req.redirect(req.href.commitqueue())
        return ""

    def get_permission_actions(self):
        return ""

