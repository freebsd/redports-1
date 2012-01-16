from trac.core import *
from trac.web.api import IRequestHandler
from trac.web.chrome import add_script, add_stylesheet, INavigationContributor, ITemplateProvider
from trac.perm import IPermissionRequestor
from trac.util.translation import _

from genshi.builder import tag

import re
from string import hexdigits
from model import Port, Build
from notify import BuildNotify

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
        if re.match(r'^/backend/', req.path_info):
            return True

    def process_request(self, req):
        # https://redports.org/backend/finished/123456789012345678901234
        if req.path_info.startswith("/backend/finished/"):
            key = req.path_info[18:]
            if len(key) > 10 and self.ishex(key):
                port = Port(self.env)
                port.updateStatus(51, key)

                req.send("OK", "text/plain", 200)
                return ""

        # https://redports.org/backend/notify/20120105095150-2805
        if req.path_info.startswith("/backend/notify/"):
            queueid = req.path_info[16:]

            build = Build(self.env, queueid)
            if not build.notifyEnabled():
               req.send("ERROR", "text/plain", 500)
               return ""

            notifier = BuildNotify(self.env)
            notifier.notify(queueid)

            req.send("OK", "text/plain", 200)
            return ""

        req.send("ERROR", "text/plain", 500)
        return ""

    def get_permission_actions(self):
        return ""

    def ishex(self, s):
        for c in s:
            if not c in hexdigits:
                return False
        return True
