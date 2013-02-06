from trac.core import *
from trac.web.api import IRequestHandler
from trac.web.chrome import add_ctxtnav, add_script, add_stylesheet, add_warning, add_notice, INavigationContributor, ITemplateProvider
from trac.perm import IPermissionRequestor
from trac.util.translation import _, tag_

from genshi.builder import tag

from pkg_resources import resource_filename
import re

from model import Build, Port, BuildqueueIterator, UserBuildgroupsIterator, RepositoryIterator

class BuildqueuePanel(Component):
    implements(INavigationContributor, ITemplateProvider, IRequestHandler, IPermissionRequestor)

    def get_active_navigation_item(self, req):
        return 'buildqueue'

    def get_navigation_items(self, req):
        if 'BUILDQUEUE_VIEW' in req.perm('buildqueue'):
            yield('mainnav', 'buildarchive', tag.a(_('Archive'), href=req.href.buildarchive(owner=req.authname)))
            yield('mainnav', 'buildqueue', tag.a(_('Builds'), href=req.href.buildqueue()))
        else:
            yield('mainnav', 'buildarchive', tag.a(_('Archive'), href=req.href.buildarchive()))

        if req.authname == 'anonymous':
            if 'WIKI_VIEW' in req.perm('wiki'):
                yield('mainnav', 'swiki', tag.a(_('Wiki'), href=req.href.wiki()))
            if 'BROWSER_VIEW' in req.perm:
                yield('mainnav', 'sbrowser', tag.a(_('Browse Source'), href=req.href.browser()))
        else:
            if 'WIKI_VIEW' in req.perm('wiki'):
                yield('mainnav', 'swiki', tag.a(_('Wiki'), href=req.href.wiki('Users')+'/'+req.authname))
            if 'BROWSER_VIEW' in req.perm:
                yield('mainnav', 'sbrowser', tag.a(_('Browse Source'), href=req.href.browser(req.authname)))

    def get_htdocs_dirs(self):
        return [('redports', resource_filename('redports', 'htdocs'))]

    def get_templates_dirs(self):
        return [resource_filename('redports', 'templates')]

    def match_request(self, req):
        if re.match(r'^/buildqueue', req.path_info):
            return True

    def process_request(self, req):
        req.perm('redports').assert_permission('BUILDQUEUE_VIEW')

        try:
            if req.method == 'POST' and req.args.get('addbuild'):
                build = Build(self.env)
                build.owner = req.authname
                build.repository = req.args.get('repository')
                build.revision = req.args.get('revision')
                build.description = req.args.get('description')
                build.priority = req.args.get('priority')

                build.addBuild(req.args.get('group'), req.args.get('portname'), req)
                add_notice(req, 'New builds have been scheduled')
                req.redirect(req.href.buildqueue())
            elif req.method == 'POST' and req.args.get('deleteport'):
                port = Port(self.env)
                port.id = req.args.get('id')
                port.delete(req)
                req.redirect(req.href.buildqueue())
            elif req.method == 'POST' and req.args.get('deletebuild'):
                build = Build(self.env)
                build.queueid = req.args.get('queueid')
                build.delete(req)
                req.redirect(req.href.buildqueue())
        except TracError as e:
            add_warning(req, e.message);

        add_stylesheet(req, 'redports/redports.css')
        add_ctxtnav(req, _('Environments'), req.href.buildgroups())
        add_ctxtnav(req, _('Statistics'), req.href.buildstats())

        return ('buildqueue.html', 
            {   'buildqueue': BuildqueueIterator(self.env, req),
                'allbuildgroups': UserBuildgroupsIterator(self.env),
                'repository': RepositoryIterator(self.env, req),
                'authname': req.authname
            }, None)

    def get_permission_actions(self):
        return ['BUILDQUEUE_VIEW']

