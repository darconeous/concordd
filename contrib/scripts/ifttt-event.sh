#!/bin/sh

IFTTT_WEBHOOK_KEY=enter-webhook-key-here

echo "Triggering 'CONCORDD-${CONCORDD_EVENT_STATUS}-${CONCORDD_TYPE}': ${CONCORDD_EVENT_DESC}"

# Example Event Names:
#
# * CONCORDD-ONGOING-PARTITION-ALARM
# * CONCORDD-RESTORED-PARTITION-ALARM
# * CONCORDD-CANCELED-PARTITION-ALARM

curl \
    -H "Content-Type: application/json" \
    -d '{ "value1" : "'"${CONCORDD_EVENT_DESC}"'", "value2" : "'"${CONCORDD_EVENT_GENERAL_TYPE}.${CONCORDD_EVENT_SPECIFIC_TYPE}"'" }' \
    -X POST https://maker.ifttt.com/trigger/CONCORDD-${CONCORDD_EVENT_STATUS}-${CONCORDD_TYPE}/with/key/${IFTTT_WEBHOOK_KEY} \
&

