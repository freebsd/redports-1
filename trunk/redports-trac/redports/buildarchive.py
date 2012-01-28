from trac.core import *
from trac.config import IntOption
from trac.web.api import IRequestHandler
from trac.web.chrome import add_ctxtnav, add_link, add_script, add_stylesheet, INavigationContributor, ITemplateProvider
from trac.perm import IPermissionRequestor
from trac.util.presentation import Paginator
from trac.util.translation import _

from genshi.builder import tag

from pkg_resources import resource_filename
import re

from model import BuildarchiveIterator, BuildqueueIterator
from buildqueue import render_ctxtnav

class BuildarchivePanel(Component):
    implements(INavigationContributor, ITemplateProvider, IRequestHandler, IPermissionRequestor)

    items_per_page = IntOption('buildarchive', 'items_per_page', 25,
        """Number of builds displayed per page in buildarchive""")

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

        add_stylesheet(req, 'redports/redports.css')
        render_ctxtnav(req)

        if len(uriparts) == 2:
            # Buildarchive details
            page = int(req.args.get('page', '1'))
            limit = self.items_per_page
            offset = (page - 1) * limit

            if limit < 0 or page < 1:
                raise TracError('Invalid page')

            builds = BuildarchiveIterator(self.env)
            builds.filter(req.args.get('owner', None), None, True)
            builddata = list(builds.get_data(limit, offset))

            paginator = Paginator(builddata, page-1, limit, builds.count())
            pagedata = []
            shown_pages = paginator.get_shown_pages()

            for p in shown_pages:
                pagedata.append([req.href.buildarchive(page=p, owner=req.args.get('owner', None)), None, str(p), _('Page %(num)d', num=p)])

            if paginator.has_next_page:
                add_link(req, 'next', req.href.buildarchive(page=page+1, owner=req.args.get('owner', None)), _('Next Page'))

            if paginator.has_previous_page:
                add_link(req, 'prev', req.href.buildarchive(page=page-1, owner=req.args.get('owner', None)), _('Previous Page'))

            fields = ['href', 'class', 'string', 'title']
            paginator.shown_pages = [dict(zip(fields, p)) for p in pagedata]
            paginator.current_page = {'href': None, 'class': 'current',
                                    'string': str(paginator.page + 1),
                                    'title': None}

            if req.args.get('format') == 'rss':
                return ('buildarchive.rss', {
                        'builds': builddata,
                        'paginator': paginator
                    }, 'application/rss+xml')

            add_link(req, 'alternate', req.href.buildarchive(page=page, owner=req.args.get('owner', None), format='rss'),
                _('RSS Feed'), 'application/rss+xml', 'rss')

            return ('buildarchive.html', 
                {   'builds': builddata,
                    'paginator': paginator
                },  None)
        else:
            # Buildarchive
            builds = BuildarchiveIterator(self.env)
            builds.filter(None, uriparts[2])

            if req.args.get('format') == 'rss':
                return ('buildarchivedetails.rss', {
                        'builds': builds.get_data(),
                    }, 'application/rss+xml')

            add_link(req, 'alternate', req.href.buildarchive(uriparts[2], format='rss'),
                _('RSS Feed'), 'application/rss+xml', 'rss')

            return ('buildarchivedetails.html', {
                    'builds': builds.get_data()
                },  None)

    def get_permission_actions(self):
        return ""

