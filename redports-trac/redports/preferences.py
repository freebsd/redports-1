from trac.core import *
from trac.prefs import IPreferencePanelProvider
from trac.web.chrome import add_notice, add_warning

class RedportsPreferencePanel(Component):

    implements(IPreferencePanelProvider)

    # IPreferencePanelProvider methods
    def get_preference_panels(self, req):
        yield ('builds', 'Builds')

    def render_preference_panel(self, req, panel):
        if req.method == 'POST':
            if req.args.get('notifications'):
                req.session['build_notifications'] = True
            else:
                if req.session.get('build_notifications'):
                    del req.session['build_notifications']

            if req.args.get('wrkdir'):
                req.session['build_wrkdirdownload'] = True
            else:
                if req.session.get('build_wrkdirdownload'):
                    del req.session['build_wrkdirdownload']

            if req.session.get('build_notifications') and ( not req.session.get('email') or req.session.get('email_verification_token')):
                add_warning(req, 'You need to verify your EMail to get notifications.')

            add_notice(req, 'Your preferences have been saved.')
            req.redirect(req.href.prefs(panel or None))

        options = {}
        if req.session.get('build_notifications'):
            options['notifications'] = True
        else:
            options['notifications'] = False

        if req.session.get('build_wrkdirdownload'):
            options['wrkdir'] = True
        else:
            options['wrkdir'] = False

        return 'preferences.html', {
            'options': options
        }
