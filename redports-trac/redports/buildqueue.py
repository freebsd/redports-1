from trac.core import *
from trac.web.api import IRequestHandler
from trac.web.chrome import add_script, add_stylesheet, add_warning, add_notice, INavigationContributor, ITemplateProvider
from trac.perm import IPermissionRequestor
from trac.util.translation import _
from trac.versioncontrol import RepositoryManager

from genshi.builder import tag

from pkg_resources import resource_filename
import re

from model import Port, PortsQueueIterator, AllBuildgroupsIterator

class BuildqueuePanel(Component):
    """ Pages for adding/editing contacts. """
    implements(INavigationContributor, ITemplateProvider, IRequestHandler, IPermissionRequestor)

    #   INavigationContributor methods
    def get_active_navigation_item(self, req):
        """This method is only called for the `IRequestHandler` processing the
        request.
        
        It should return the name of the navigation item that should be
        highlighted as active/current.
        """
        return 'buildqueue'
    def get_navigation_items(self, req):
        """Should return an iterable object over the list of navigation items to
        add, each being a tuple in the form (category, name, text).
        """
        if 'BUILDQUEUE_VIEW' in req.perm('buildqueue'):
            yield('mainnav', 'buildqueue', tag.a(_('Your Builds'), href=req.href.buildqueue()))

    #   ITemplateProvider
    def get_htdocs_dirs(self):
        return [('redports', resource_filename('redports', 'htdocs'))]
    def get_templates_dirs(self):
        """Return a list of directories containing the provided template
        files.
        """
        return [resource_filename('redports', 'templates')]

    #   IRequestHandler methods
    def match_request(self, req):
        """Return whether the handler wants to process the given request."""
        if re.match(r'^/buildqueue', req.path_info):
            return True
    def process_request(self, req):
        """Process the request. For ClearSilver, return a (template_name,
        content_type) tuple, where `template` is the ClearSilver template to use
        (either a `neo_cs.CS` object, or the file name of the template), and
        `content_type` is the MIME type of the content. For Genshi, return a
        (template_name, data, content_type) tuple, where `data` is a dictionary
        of substitutions for the template.

        For both templating systems, "text/html" is assumed if `content_type` is
        `None`.

        Note that if template processing should not occur, this method can
        simply send the response itself and not return anything.
        """
        req.perm('redports').assert_permission('BUILDQUEUE_VIEW')

        repopath = RepositoryManager(self.env).get_repository(None).get_path_url(req.authname, 1)
        reponame, repos, repopath2 = RepositoryManager(self.env).get_repository_by_path(repopath)

        if not repos.has_node(req.authname):
            add_warning(req, 'Your account was not activated yet '
                             'so scheduling new builds will not work.')
        elif req.method == 'POST' and req.args.get('addbuild'):
            port = Port(self.env)
            port.owner = req.authname
            port.repository = repopath
            port.revision = req.args.get('revision')
            port.portname = req.args.get('portname')
            port.group = req.args.get('group')
            port.addPort()
            add_notice(req, 'New builds for port %s have been scheduled', req.args.get('portname'))
            req.redirect(req.href.buildqueue())
        elif req.method == 'POST' and req.args.get('deletebuild'):
            port = Port(self.env)
            port.id = req.args.get('id')
            port.deleteBuildQueueEntry(req)
            req.redirect(req.href.buildqueue())

        add_stylesheet(req, 'common/css/admin.css')
        add_stylesheet(req, 'redports/redports.css')

        return ('buildqueue.html', 
            {   'buildqueue': PortsQueueIterator(self.env, req),
                'allbuildgroups': AllBuildgroupsIterator(self.env),
                'repository': repopath,
                'revision': repos.youngest_rev,
                'authname': req.authname
            }, None)
    #   IPermissionRequest methods
    def get_permission_actions(self):
        """Return a list of actions defined by this component.
        
        The items in the list may either be simple strings, or
        `(string, sequence)` tuples. The latter are considered to be "meta
        permissions" that group several simple actions under one name for
        convenience.
        """
        return ['BUILDQUEUE_VIEW']
