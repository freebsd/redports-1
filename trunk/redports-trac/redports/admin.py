from trac.core import *
from trac.admin import IAdminPanelProvider
from trac.util.translation import _
from trac.perm import IPermissionRequestor, PermissionSystem
from trac.web.chrome import ITemplateProvider, add_notice, add_warning, add_stylesheet

from pkg_resources import resource_filename

from model import Backend, BackendsIterator

class AdminPanel(Component):

    implements(IAdminPanelProvider, IPermissionRequestor, ITemplateProvider)

    # IPermissionRequestor
    def get_permission_actions(self):
        return ['REDPORTS_ADMIN']

    # IAdminPanelProvider
    def get_admin_panels(self, req):
        if req.perm.has_permission('REDPORTS_ADMIN'):
            yield ('redports', _("Redports"), 'backends', _("Backends"))
            yield ('redports', _("Redports"), 'builds', _("Builds"))
            yield ('redports', _("Redports"), 'buildgroups', _("Buildgroups"))

    def render_admin_panel(self, req, cat, page, path_info):
        if page == 'backends':
            return self.show_backends(req)
        elif page == 'buildgroups':
            return ""

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

