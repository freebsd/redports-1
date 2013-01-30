from trac.core import *
from trac.prefs import IPreferencePanelProvider
from trac.web.chrome import add_notice, add_warning
from tools import *

class RedportsPreferencePanel(Component):

    implements(IPreferencePanelProvider)

    # IPreferencePanelProvider methods
    def get_preference_panels(self, req):
        if 'BUILDQUEUE_VIEW' in req.perm('buildqueue'):
            yield ('builds', 'Builds')

    def render_preference_panel(self, req, panel):
        if req.method == 'POST':
            if not req.session.get('build_notifications'):
                notifications = 2
            else:
                notifications = int(req.session['build_notifications'])

            if req.args.get('notifications_success'):
                notifications = setBit(notifications, 0)
            else:
                notifications = clearBit(notifications, 0)

            if req.args.get('notifications_failed'):
                notifications = setBit(notifications, 1)
            else:
                notifications = clearBit(notifications, 1)

            if req.args.get('notifications_leftovers'):
                notifications = setBit(notifications, 2)
            else:
                notifications = clearBit(notifications, 2)

            req.session['build_notifications'] = notifications

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
        if not req.session.get('build_notifications'):
            notifications = 2
        else:
            notifications = int(req.session['build_notifications'])

        if testBit(notifications, 0):
            options['notifications_success'] = True
        else:
            options['notifications_success'] = False

        if testBit(notifications, 1):
            options['notifications_failed'] = True
        else:
            options['notifications_failed'] = False

        if testBit(notifications, 2):
            options['notifications_leftovers'] = True
        else:
            options['notifications_leftovers'] = False

        if req.session.get('build_wrkdirdownload'):
            options['wrkdir'] = True
        else:
            options['wrkdir'] = False

        return 'preferences.html', {
            'options': options
        }

