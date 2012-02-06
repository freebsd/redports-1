#!/bin/sh

# redports dispatcher hook script

# Environment variables:
#   RPOWNER
#   RPSTATUS
#   RPBUILDID
#   RPBUILDQUEUEID
#   RPBUILDGROUP
#   RPPORTNAME
#   RPPKGVERSION
#   RPBUILDSTATUS
#   RPBUILDREASON
#   RPBUILDLOG

MSG=""

if [ "${RPSTATUS}" = "31" ]; then
    MSG="[${RPBUILDGROUP}] (${RPOWNER}) - ${RPBUILDQUEUEID}-${RPBUILDID} - ${RPPORTNAME} - start"
fi

if [ "${RPSTATUS}" = "71" ]; then
    MSG="[${RPBUILDGROUP}] (${RPOWNER}) - ${RPBUILDQUEUEID}-${RPBUILDID} - ${RPPORTNAME} ${RPPKGVERSION} - ${RPBUILDSTATUS} ${RPBUILDREASON}"
    if [ ! -z "${RPBUILDLOG}" ] ; then
        MSG="${MSG} - ${RPWWWURL}/~${RPOWNER}/${RPBUILDQUEUEID}-${RPBUILDID}/${RPBUILDLOG}"
    fi
fi

if [ ! -z "${MSG}" ]; then
    echo ${MSG} >> /var/log/rpdd-hook.log
    echo "#redports-buildlog ${MSG}" | nc localhost 9999
fi

exit 0

