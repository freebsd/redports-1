from trac.core import *
from trac.admin import IAdminPanelProvider
from trac.util.translation import _
from trac.perm import IPermissionRequestor, PermissionSystem
from trac.web.chrome import ITemplateProvider, add_notice, add_warning, add_stylesheet

from pkg_resources import resource_filename

from model import Backend, BackendsIterator, Backendbuild, BackendbuildsIterator, Buildgroup, AllBuildgroupsIterator, Port, GlobalBuildqueueIterator, PortRepository, PortRepositoryIterator

class AdminPanel(Component):

    implements(IAdminPanelProvider, IPermissionRequestor, ITemplateProvider)

    # IPermissionRequestor
    def get_permission_actions(self):
        return ['REDPORTS_ADMIN']

    # IAdminPanelProvider
    def get_admin_panels(self, req):
        if req.perm.has_permission('REDPORTS_ADMIN'):
            yield ('redports', _("Redports"), 'backends', _("Backends"))
            yield ('redports', _("Redports"), 'backendbuilds', _("Backendbuilds"))
            yield ('redports', _("Redports"), 'buildgroups', _("Buildgroups"))
            yield ('redports', _("Redports"), 'builds', _("Builds"))
            yield ('redports', _("Redports"), 'repository', _("Repository"))

    def render_admin_panel(self, req, cat, page, path_info):
        if page == 'backends':
            return self.show_backends(req)
        elif page == 'backendbuilds':
            return self.show_backendbuilds(req)
        elif page == 'buildgroups':
            return self.show_buildgroups(req)
        elif page == 'builds':
            return self.show_builds(req)
        elif page == 'repository':
            return self.show_repositories(req)

    def get_htdocs_dirs(self):
        """Return the absolute path of a directory containing additional
        static resources (such as images, style sheets, etc).
        """
        return [('redports', resource_filename('redports', 'htdocs'))]

    def get_templates_dirs(self):
        """Return the absolute path of the directory containing the provided
        Genshi templates.
        """
        return [resource_filename('redports', 'templates')]

    def show_backends(self, req):
        if req.method == 'POST':
            if req.args.get('disable') and req.args.get('backend'):
                backend = Backend(self.env, req.args.get('backend'))
                backend.updateStatus(0)
                req.redirect(req.href.admin('redports/backends'))
            elif req.args.get('enable') and req.args.get('backend'):
                backend = Backend(self.env, req.args.get('backend'))
                backend.updateStatus(1)
                req.redirect(req.href.admin('redports/backends'))
            elif req.args.get('delete') and req.args.get('backend'):
                backend = Backend(self.env, req.args.get('backend'))
                backend.delete()
                req.redirect(req.href.admin('redports/backends'))
            elif req.args.get('add'):
                backend = Backend(self.env)
                backend.host = req.args.get('host')
                backend.protocol = req.args.get('protocol')
                backend.uri = req.args.get('uri')
                backend.credentials = req.args.get('credentials')
                backend.maxparallel = req.args.get('maxparallel')
                backend.type = 'tinderbox'
                backend.setStatus(1)
                backend.add()
                req.redirect(req.href.admin('redports/backends'))
            else:
                raise TracError('Invalid form fields')

        add_stylesheet(req, 'redports/redports.css')
        return 'adminbackends.html', {
                 'backend': None,
                 'backends': BackendsIterator(self.env)
               }

    def show_backendbuilds(self, req):
        if req.method == 'POST':
            if req.args.get('disable') and req.args.get('backendbuild'):
                backendbuild = Backendbuild(self.env, req.args.get('backendbuild'))
                backendbuild.updateStatus(0)
                req.redirect(req.href.admin('redports/backendbuilds'))
            elif req.args.get('enable') and req.args.get('backendbuild'):
                backendbuild = Backendbuild(self.env, req.args.get('backendbuild'))
                backendbuild.updateStatus(1)
                req.redirect(req.href.admin('redports/backendbuilds'))
            elif req.args.get('delete') and req.args.get('backendbuild'):
                backendbuild = Backendbuild(self.env, req.args.get('backendbuild'))
                backendbuild.delete()
                req.redirect(req.href.admin('redports/backendbuilds'))
            elif req.args.get('add'):
                backendbuild = Backendbuild(self.env)
                backendbuild.buildgroup = req.args.get('buildgroup')
                backendbuild.backendid = req.args.get('backendid')
                backendbuild.priority = req.args.get('priority')
                backendbuild.buildname = req.args.get('buildname')
                backendbuild.setStatus(1)
                backendbuild.add()
                req.redirect(req.href.admin('redports/backendbuilds'))
            else:
                raise TracError('Invalid form fields')

        add_stylesheet(req, 'redports/redports.css')
        return 'adminbackendbuilds.html', {
                 'backends': BackendsIterator(self.env),
                 'buildgroups': AllBuildgroupsIterator(self.env),
                 'backendbuilds': BackendbuildsIterator(self.env)
               }

    def show_buildgroups(self, req):
        if req.method == 'POST':
            if req.args.get('delete') and req.args.get('buildgroup'):
                buildgroup = Buildgroup(self.env, req.args.get('buildgroup'))
                buildgroup.delete()
                req.redirect(req.href.admin('redports/buildgroups'))
            elif req.args.get('deleteAllJobs') and req.args.get('buildgroup'):
                buildgroup = Buildgroup(self.env, req.args.get('buildgroup'))
                buildgroup.deleteAllJobs()
                add_notice(req, "Jobs for group %s deleted" % req.args.get('buildgroup'))
                req.redirect(req.href.admin('redports/buildgroups'))
            elif req.args.get('add'):
                buildgroup = Buildgroup(self.env, req.args.get('buildgroup'))
                buildgroup.version = req.args.get('version')
                buildgroup.arch = req.args.get('arch')
                buildgroup.type = 'tinderbox'
                buildgroup.description = req.args.get('description')
                buildgroup.add()
                req.redirect(req.href.admin('redports/buildgroups'))

        add_stylesheet(req, 'redports/redports.css')
        return 'adminbuildgroups.html', {
                 'buildgroups': AllBuildgroupsIterator(self.env)
               }

    def show_builds(self, req):
        if req.method == 'POST':
            if req.args.get('delete') and req.args.get('buildid'):
                port = Port(self.env, req.args.get('buildid'))
                port.delete()
                req.redirect(req.href.admin('redports/builds'))

        add_stylesheet(req, 'redports/redports.css')
        return 'adminbuildqueue.html', {
                 'buildqueue': GlobalBuildqueueIterator(self.env, req)
               }

    def show_repositories(self, req):
        if req.method == 'POST':
            if req.args.get('delete') and req.args.get('id'):
                repo = PortRepository(self.env, req.args.get('id'))
                repo.delete()
                req.redirect(req.href.admin('redports/repository'))
            elif req.args.get('add'):
                repo = PortRepository(self.env, req.args.get('id'))
                repo.type = req.args.get('type')
                repo.name = req.args.get('name')
                repo.url = req.args.get('url')
                repo.browseurl = req.args.get('browseurl')
                repo.username = req.args.get('username')
                repo.add()
                req.redirect(req.href.admin('redports/repository'))

        add_stylesheet(req, 'redports/redports.css')
        return 'adminrepositories.html', {
                 'repositories': PortRepositoryIterator(self.env)
               }

