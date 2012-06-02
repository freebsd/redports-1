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
            if req.args.get('notifications_success'):
                req.session['build_notifications_success'] = True
            else:
                if req.session.get('build_notifications_success'):
                    del req.session['build_notifications_success']

            if req.args.get('notifications_failed'):
                req.session['build_notifications_failed'] = True
            else:
                if req.session.get('build_notifications_failed'):
                    del req.session['build_notifications_failed']

            if req.args.get('wrkdir'):
                req.session['build_wrkdirdownload'] = True
            else:
                if req.session.get('build_wrkdirdownload'):
                    del req.session['build_wrkdirdownload']

            if ( req.session.get('build_notifications_success') or req.session.get('build_notifications_failed') ) and ( not req.session.get('email') or req.session.get('email_verification_token')):
                add_warning(req, 'You need to verify your EMail to get notifications.')

            add_notice(req, 'Your preferences have been saved.')
            req.redirect(req.href.prefs(panel or None))

        options = {}
        if req.session.get('build_notifications_success'):
            options['notifications_success'] = True
        else:
            options['notifications_success'] = False

        if req.session.get('build_notifications_failed'):
            options['notifications_failed'] = True
        else:
            options['notifications_failed'] = False

        if req.session.get('build_wrkdirdownload'):
            options['wrkdir'] = True
        else:
            options['wrkdir'] = False

        return 'preferences.html', {
            'options': options
        }
