/*
 *
 * Copyright (c) 2017 Robert Quattlebaum
 * All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

#ifndef concordd_dbus_h
#define concordd_dbus_h 1

#define CONCORDD_DBUS_NAME                      "net.voria.concordd"
#define CONCORDD_DBUS_PATH_ROOT                 "/net/voria/concordd"
#define CONCORDD_DBUS_PATH_PARTITION            CONCORDD_DBUS_PATH_ROOT "/partition/"
#define CONCORDD_DBUS_PATH_ZONE                 CONCORDD_DBUS_PATH_ROOT "/zone/"
#define CONCORDD_DBUS_PATH_OUTPUT               CONCORDD_DBUS_PATH_ROOT "/output/"
#define CONCORDD_DBUS_INTERFACE                 "net.voria.concordd.v1"

#define CONCORDD_DBUS_CMD_GET_INFO              "get_info"
#define CONCORDD_DBUS_CMD_GET_PARTITIONS              "get_partitions"
#define CONCORDD_DBUS_CMD_GET_ZONES              "get_zones"
#define CONCORDD_DBUS_CMD_GET_BUS_DEVICES              "get_bus_devices"
#define CONCORDD_DBUS_CMD_GET_USERS              "get_users"
#define CONCORDD_DBUS_CMD_GET_OUTPUTS              "get_outputs"
#define CONCORDD_DBUS_CMD_GET_VERSION              "get_version"
#define CONCORDD_DBUS_CMD_REFRESH              "refresh"
#define CONCORDD_DBUS_CMD_SEND_RAW_FRAME              "send_raw_frame"

#define CONCORDD_DBUS_CMD_GET_TROUBLES             "get_troubles" // Returns array of events
#define CONCORDD_DBUS_CMD_GET_ALARMS               "get_alarms" // Returns array of events
#define CONCORDD_DBUS_CMD_GET_EVENTLOG               "get_event_log" // Returns array of events

#define CONCORDD_DBUS_CMD_GET_LIGHTS              "get_lights"
#define CONCORDD_DBUS_CMD_GET_SCHEDULES              "get_schedules"
#define CONCORDD_DBUS_CMD_GET_SCHEDULED_EVENTS              "get_scheduled_events"
#define CONCORDD_DBUS_CMD_PRESS_KEYS              "press_keys"
#define CONCORDD_DBUS_CMD_SET_ARM_LEVEL              "set_arm_level"
#define CONCORDD_DBUS_CMD_GET_EVENT_LOG              "get_event_log"

#define CONCORDD_DBUS_CMD_SET_BYPASSED              "set_bypassed"
#define CONCORDD_DBUS_CMD_SET_VALUE              "set_value"
#define CONCORDD_DBUS_CMD_TOGGLE_VALUE              "toggle_value"

#define CONCORDD_DBUS_SIGNAL_RECV_RAW_FRAME     "recv_raw_frame"
#define CONCORDD_DBUS_SIGNAL_CHANGED            "changed"
#define CONCORDD_DBUS_SIGNAL_SIREN_SYNC         "siren_sync"
#define CONCORDD_DBUS_SIGNAL_ALARM              "alarm"
#define CONCORDD_DBUS_SIGNAL_TROUBLE            "trouble"
#define CONCORDD_DBUS_SIGNAL_EVENT              "event"

#define CONCORDD_DBUS_EXCEPTION_SOURCE_TYPE     "sourceType"
#define CONCORDD_DBUS_EXCEPTION_UNIT_ID     "unitId"
#define CONCORDD_DBUS_EXCEPTION_ZONE_ID     "zoneId"
#define CONCORDD_DBUS_EXCEPTION_STATUS     "status"
#define CONCORDD_DBUS_EXCEPTION_GENERAL_TYPE     "generalType"
#define CONCORDD_DBUS_EXCEPTION_SPECIFIC_TYPE     "specificType"
#define CONCORDD_DBUS_EXCEPTION_EXTRA_DATA     "extraData"
#define CONCORDD_DBUS_EXCEPTION_TIMESTAMP     "timestamp"
#define CONCORDD_DBUS_EXCEPTION_DESCRIPTION     "description"
#define CONCORDD_DBUS_EXCEPTION_CATEGORY     "category"

#define CONCORDD_DBUS_CLASS_NAME_PANEL     "panel"
#define CONCORDD_DBUS_CLASS_NAME_PARTITION     "partition"
#define CONCORDD_DBUS_CLASS_NAME_ZONE     "zone"
#define CONCORDD_DBUS_CLASS_NAME_OUTPUT     "output"
#define CONCORDD_DBUS_CLASS_NAME_LIGHT     "light"

#define CONCORDD_DBUS_INFO_CLASS_NAME       "className"    // string
#define CONCORDD_DBUS_INFO_PANEL_TYPE       "panelType"    // unsigned int
#define CONCORDD_DBUS_INFO_HW_REVISION      "hwRevision"   // unsigned int
#define CONCORDD_DBUS_INFO_SW_REVISION      "swRevision"   // unsigned int
#define CONCORDD_DBUS_INFO_SERIAL_NUMBER    "serialNumber" // unsigned int
#define CONCORDD_DBUS_INFO_PROGRAMMING_MODE "programmingMode" // bool
#define CONCORDD_DBUS_INFO_AC_POWER_FAILURE "acPowerFailure" // bool
#define CONCORDD_DBUS_INFO_AC_POWER_FAILURE_CHANGED_TIMESTAMP "acPowerFailureChangedTimestamp" // unsigned int

#define CONCORDD_DBUS_INFO_PARTITION_ID     "partitionId"  // unsigned int
#define CONCORDD_DBUS_INFO_ARM_LEVEL     "armLevel"  // unsigned int
#define CONCORDD_DBUS_INFO_ARM_LEVEL_USER     "armLevelUser"  // unsigned int
#define CONCORDD_DBUS_INFO_LAST_ALARM     "lastAlarm"  // dictionary

#define CONCORDD_DBUS_INFO_TOUCHPAD_TEXT     "touchpadText"  // string
#define CONCORDD_DBUS_INFO_SIREN_REPEAT     "sirenRepeat"  // unsigned int
#define CONCORDD_DBUS_INFO_SIREN_CADENCE     "sirenCadence"  // uint32
#define CONCORDD_DBUS_INFO_SIREN_STARTED_AT     "sirenStartedAt"  // uint32

#define CONCORDD_DBUS_INFO_CHIME     "chime"  // bool
#define CONCORDD_DBUS_INFO_ENERGY_SAVER     "energySaver"  // bool
#define CONCORDD_DBUS_INFO_NO_DELAY     "noDelay"  // bool
#define CONCORDD_DBUS_INFO_LATCH_KEY     "latchKey"  // bool
#define CONCORDD_DBUS_INFO_SILENT_ARM     "silentArm"  // bool
#define CONCORDD_DBUS_INFO_QUICK_ARM     "quickArm"  // bool

#define CONCORDD_DBUS_INFO_ZONE_ID     "zoneId"  // unsigned int
#define CONCORDD_DBUS_INFO_NAME       "name"    // string
#define CONCORDD_DBUS_INFO_GROUP     "group"  // unsigned int
#define CONCORDD_DBUS_INFO_TYPE     "type"  // unsigned int
#define CONCORDD_DBUS_INFO_IS_TRIPPED     "isTripped"  // bool
#define CONCORDD_DBUS_INFO_IS_BYPASSED     "isBypassed"  // bool
#define CONCORDD_DBUS_INFO_IS_TROUBLE     "isTrouble"  // bool
#define CONCORDD_DBUS_INFO_IS_ALARM     "isAlarm"  // bool
#define CONCORDD_DBUS_INFO_IS_FAULT     "isFault"  // bool
#define CONCORDD_DBUS_INFO_LAST_KC_CHANGED_AT "lastKcChangedAt"
#define CONCORDD_DBUS_INFO_LAST_KC            "lastKc"

#define CONCORDD_DBUS_INFO_VALUE     "value"

#define CONCORDD_DBUS_INFO_OUTPUT_ID     "outputId"  // unsigned int
#define CONCORDD_DBUS_INFO_PULSE         "pulse"  // boolean

#define CONCORDD_DBUS_INFO_LIGHT_ID     "lightId"  // unsigned int
#define CONCORDD_DBUS_INFO_LAST_CHANGED_BY     "lastChangedBy"  // unsigned int
#define CONCORDD_DBUS_INFO_LAST_CHANGED_AT     "lastChangedAt"  // unsigned int
#define CONCORDD_DBUS_INFO_LAST_TRIPPED_AT     "lastTrippedAt"  // unsigned int


/*
FOB CODES
0 = disarm
1 = arm
2 = lights-toggle
3 = star
4 = arm&disarm-toggle
5 = lights&star
6 = long lights
9 = arm&star
10 = disarm&lights-on



className (string, always "light")
lightId (unsigned int)
partitionId (unsigned int)
zoneId (unsigned int)
value (bool)
lastChangedBy (string)
lastChangedAt (unsigned int)


className (string, always "output")
outputId (unsigned int)
name (string)
value (bool)
lastChangedAt (unsigned int)


className (string, always "zone")
zoneId (unsigned int)
partitionId (unsigned int)
name (string)
group (unsigned int)
type (enum: Hardwired, RF, RF-Touchpad)
isTripped (bool)
isBypassed (bool)
isTrouble (bool)
isAlarm (bool)
isFault (bool)

className (string, always "panel")
panelType (unsigned int)
hardwareRevision (unsigned int)
softwareRevision (unsigned int)
serialNumber (unsigned int)


className (string, always "partition")
partitionId (unsigned int)
armLevel (unsigned int)
armLevelUser (unsigned int)
armLevelTimestamp (unsigned int)
lastException (dictionary)
sourceType
sourceNumber
generalType
specificType
extraData
timestamp
description
touchpadText (string)
sirenRepeat (unsigned int)
sirenCadence (uint32)
chime (bool)
energySaver (bool)
noDelay (bool)
latchKey (bool)
silentArm (bool)
quickArm (bool)

*/



#endif // ifndef concordd_h
