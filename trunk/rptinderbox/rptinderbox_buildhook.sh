#!/bin/sh
#
# RedPorts tinderbox hook script
#
# $Id$
#
# If installed as a postPortBuild Hook, it will compress the WRKDIR. Since this
# is a time consuming job, by default it only compresses it for failed builds.
# If you want to compress all the WRKDIRs, uncomment COMPRESS_ALL bellow.
# If you don't change the path, tindy's WebUI will show a link to the tarball.
# If you un-comment DELETE_OLD, and the build was succesful an existing tarball
# corresponding to the current PKGNAME will be deleted.

#COMPRESS_ALL=true
#DELETE_OLD=true

TBUSER=www
LOCK=/tmp/rptinderbox/${BUILD}/.lock
FINISHED=/tmp/rptinderbox/${BUILD}/.finished

compress_wrkdir () {
    mkdir -p "${PB}/wrkdirs/${BUILD}" && \
    cd ${PB}/${BUILD}/a/ports/${PORTDIR} && objdir=$(make -V WRKDIR) && \
    objdir=`echo ${objdir} | sed "s,${PB}/${BUILD}/,${PB}/${BUILD}/work/,"` && \
    tar cfjC ${PB}/wrkdirs/${BUILD}/${PACKAGE_NAME}.tbz ${objdir}/.. work && \
    chown ${TBUSER} ${PB}/wrkdirs/${BUILD}/${PACKAGE_NAME}.tbz && \
    echo "WRKDIR=\"/wrkdirs/${BUILD}/${PACKAGE_NAME}.tbz\"" >> ${FINISHED}
    return 0
}

collect_ignorereason() {
    IGNORE=`chroot ${CHROOT} make -C /a/ports/${PORTDIR} -V IGNORE`
    FORBIDDEN=`chroot ${CHROOT} make -C /a/ports/${PORTDIR} -V FORBIDDEN`

    REASON=""
    if [ ! -z "${FORBIDDEN}" ]; then
        REASON="forbidden: ${FORBIDDEN}"
    elif [ ! -z "${IGNORE}" ]; then
        REASON="ignored: ${IGNORE}"
    fi
    echo "${REASON}"
}

chown -R ${TBUSER} ${PB}/packages/${BUILD}/

if [ -f "${LOCK}" ]; then
    . ${LOCK}

    if [ "${PORTDIR}" = "${PORT}" -o "${FAIL_REASON}" != "" ]; then
        touch ${FINISHED}

        echo "BUILDSTATUS=\"${STATUS}\"" >> ${FINISHED}
        echo "PACKAGE_NAME=\"${PACKAGE_NAME}\"" >> ${FINISHED}

        if [ "${STATUS}" = "DUD" ]; then
            FAIL_REASON=`collect_ignorereason`
        fi

        if [ "${PORTDIR}" = "${PORT}" ]; then
            echo "FAIL_REASON=\"${FAIL_REASON}\"" >> ${FINISHED}
        else
            echo "FAIL_REASON=\"depend (${FAIL_REASON} in ${PORTDIR})\"" >> ${FINISHED}
        fi
    else
        FINISHURL=""
    fi
fi

if [ "${STATUS}" = "SUCCESS" -o "${STATUS}" = "LEFTOVERS" ]; then
    if [ -f "${LOCK}" -a "${PORTDIR}" = "${PORT}" ]; then
        echo "BUILDLOG=\"/logs/${BUILD}/${PACKAGE_NAME}.log\"" >> ${FINISHED}
    fi
    if [ -n "${DELETE_OLD}" ]; then
        rm ${PB}/wrkdirs/${BUILD}/${PACKAGE_NAME}.tbz
    fi
    if [ -n "${COMPRESS_ALL}" ]; then
        compress_wrkdir
    fi
elif [ "${STATUS}" != "DUD" ]; then
    if [ -f "${LOCK}" ]; then
        echo "BUILDLOG=\"/errors/${BUILD}/${PACKAGE_NAME}.log\"" >> ${FINISHED}
    fi
    compress_wrkdir
fi

if [ ! -z "${FINISHURL}" ]; then
    fetch -q -o /dev/null "${FINISHURL}" >/dev/null 2>/dev/null
fi
