from trac.core import *
from trac.web.api import IRequestHandler
from trac.web.chrome import INavigationContributor, ITemplateProvider
from trac.perm import IPermissionRequestor
from trac.util.translation import _

from genshi.builder import tag

from model import Build, PortRepository

import re
import json

class WebhookConnector(Component):
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
	if re.match(r'^/hooks/', req.path_info):
            return True

    def process_request(self, req):
	try:
	    # check that the IP is within the GitHub ranges
	    # https://help.github.com/articles/what-ip-addresses-does-github-use-that-i-should-whitelist
	    if not any(req.remote_addr.startswith(IP)
	        for IP in ('192.30.252', '192.30.253', '192.30.254', '192.30.255', '2620:112:300')):
	        raise TracError("IP not allowed")

	    if req.get_header("X-GitHub-Event") == "ping":
	        req.send("pong", "text/plain", 200)
	        return ""

	    if req.get_header("X-GitHub-Event") == "push":
	        data = req.read(32768)
	        if not data or req.method != "POST":
	            raise TracError("No Data or wrong method")

	        ports = set()
	        payload = json.loads(data)

	        # try to find the portrepository entry
	        repo = PortRepository(self.env, None)
	        if not repo.getByUrl(payload["repository"]["url"]) or repo.type != "git":
	            raise TracError("No matching repository found")

	        # generate a list of ports
	        for commit in payload["commits"]:
	            for file in commit["added"]:
	                portname = file[0:file.find("/", file.find("/")+1)]
	                if re.match('^([a-zA-Z0-9_+.-]+)/([a-zA-Z0-9_+.-]+)$', portname) and len(portname) < 100:
	                    ports.add(portname)
	            for file in commit["modified"]:
	                portname = file[0:file.find("/", file.find("/")+1)]
	                if re.match('^([a-zA-Z0-9_+.-]+)/([a-zA-Z0-9_+.-]+)$', portname) and len(portname) < 100:
	                    ports.add(portname)

                build = Build(self.env)
                build.owner = repo.username
                build.repository = repo.id
                build.revision = payload["head_commit"]["id"]
                build.description = payload["head_commit"]["message"]

                if not build.addBuild(["automatic"], ports):
                    raise TracError("Failed adding jobs")

                req.send("OK", "text/plain", 200)
	        return ""

            req.send("Hook action not supported", "text/plain", 200)
            return ""
	except TracError as e:
            req.send(e.message, "text/plain", 500)
	    return ""

    def get_permission_actions(self):
        return ""

