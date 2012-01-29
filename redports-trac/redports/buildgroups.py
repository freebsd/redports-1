from trac.core import *
from trac.web.api import IRequestHandler
from trac.web.chrome import add_ctxtnav, add_script, add_stylesheet, INavigationContributor, ITemplateProvider
from trac.perm import IPermissionRequestor
from trac.util.translation import _

from genshi.builder import tag

from pkg_resources import resource_filename
import re

from model import Buildgroup, BuildgroupsIterator, AvailableBuildgroupsIterator, GlobalBuildqueueIterator
from buildqueue import render_ctxtnav

class BuildgroupPanel(Component):
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
        if re.match(r'^/buildgroups', req.path_info):
            return True

    def process_request(self, req):
        if req.method == 'POST' and req.args.get('leave'):
            group = Buildgroup(self.env, req.args.get('group'))
            group.leave(req)
            req.redirect(req.href.buildgroups())

        if req.method == 'POST' and req.args.get('join') and req.args.get('group') and req.args.get('priority'):
            group = Buildgroup(self.env, req.args.get('group'))
            group.setPriority(req.args.get('priority'))
            group.join(req)
            req.redirect(req.href.buildgroups())
        
        add_stylesheet(req, 'redports/redports.css')
        render_ctxtnav(req)

        return ('buildgroups.html', 
            {   'buildgroups': BuildgroupsIterator(self.env, req),
                'availablegroups': AvailableBuildgroupsIterator(self.env, req),
                'buildqueue': GlobalBuildqueueIterator(self.env, req),
                'authenticated': (req.authname and req.authname != 'anonymous')
            },  None)

    def get_permission_actions(self):
        return ""

