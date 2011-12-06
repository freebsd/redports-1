from trac.core import *
from trac.web.api import IRequestHandler
from trac.web.chrome import add_script, add_stylesheet, add_warning, add_notice, INavigationContributor, ITemplateProvider
from trac.perm import IPermissionRequestor
from trac.util.translation import _, tag_

from genshi.builder import tag

from pkg_resources import resource_filename
import re

from model import Port, PortsQueueIterator, AllBuildgroupsIterator, RepositoryIterator

class BuildqueuePanel(Component):
    implements(INavigationContributor, ITemplateProvider, IRequestHandler, IPermissionRequestor)

    def get_active_navigation_item(self, req):
        return 'buildqueue'

    def get_navigation_items(self, req):
        if 'BUILDQUEUE_VIEW' in req.perm('buildqueue'):
            yield('mainnav', 'buildqueue', tag.a(_('Your Builds'), href=req.href.buildqueue()))

    def get_htdocs_dirs(self):
        return [('redports', resource_filename('redports', 'htdocs'))]

    def get_templates_dirs(self):
        return [resource_filename('redports', 'templates')]

    def match_request(self, req):
        if re.match(r'^/buildqueue', req.path_info):
            return True

    def process_request(self, req):
        req.perm('redports').assert_permission('BUILDQUEUE_VIEW')

        if req.method == 'POST' and req.args.get('addbuild'):
            port = Port(self.env)
            port.owner = req.authname
            port.repository = req.args.get('repository')
            port.revision = req.args.get('revision')
            port.portname = req.args.get('portname')
            port.group = req.args.get('group')
            port.description = req.args.get('description')

            if port.addPort():
                add_notice(req, 'New builds for port %s have been scheduled', req.args.get('portname'))
            else:
                buildgroup = tag.a("Buildgroup", href=req.href.buildgroups())
                add_warning(req, tag_("Cannot schedule automatic builds. You need to join a %(buildgroup)s first.", buildgroup=buildgroup))
            req.redirect(req.href.buildqueue())
        elif req.method == 'POST' and req.args.get('deletebuild'):
            port = Port(self.env)
            port.id = req.args.get('id')
            port.deleteBuildQueueEntry(req)
            req.redirect(req.href.buildqueue())
        elif req.method == 'POST' and req.args.get('deletebuildqueue'):
            port = Port(self.env)
            port.queueid = req.args.get('queueid')
            port.deleteBuildQueueEntry(req)
            req.redirect(req.href.buildqueue())

        add_stylesheet(req, 'common/css/admin.css')
        add_stylesheet(req, 'redports/redports.css')

        return ('buildqueue.html', 
            {   'buildqueue': PortsQueueIterator(self.env, req),
                'allbuildgroups': AllBuildgroupsIterator(self.env),
                'repository': RepositoryIterator(self.env, req),
                'authname': req.authname
            }, None)

    def get_permission_actions(self):
        return ['BUILDQUEUE_VIEW']
