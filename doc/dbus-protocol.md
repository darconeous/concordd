# Concordd D-Bus API

* Daemon ID: `net.voria.concordd`
* Interface: `net.voria.concordd.v1`

## Path: `/net/voria/concordd/`

### Command: `get_info`
Returns a dictionary describing various details about the alarm system.

Keys:

* `className` (string, always "panel")
* `panelType` (unsigned int)
* `hardwareRevision` (unsigned int)
* `softwareRevision` (unsigned int)
* `serialNumber` (unsigned int)

### Command: `get_partitions`
Returns a list of partition paths.

### Command: `get_zones`
Returns a list of zone object paths for all partitions.

### Command: `get_bus_devices`
Returns a list of bus device object paths for all partitions.

### Command: `get_users`
Returns a list of users object paths for all partitions.

### Command: `get_outputs`
Returns a list of output object paths.

### Command: `get_version`
Returns a version string describing the version of `concordd`.

### Command: `refresh`
Refreshes the cached state of the system.

### Command: `send_raw_frame`
Used to send a raw frame to the alarm system panel(checksum excluded).

### Signal: `recv_raw_frame`
Emitted when a frame is received from the alarm system panel(checksum excluded).

### Signal: `changed`
Emits a dictionary describing the changed keys from `get_info`

### Signal: `keyfob-button`
Emitted when a keyfob button is pressed.

## Path: `/net/voria/concordd/partition/[partition-number]/`

### Command: `get_info`
Returns a dictionary describing the current status of the partition.

Keys:

* `className` (string, always "partition")
* `partitionId` (unsigned int)
* `armLevel`  (unsigned int)
* `armLevelUser` (unsigned int)
* `armLevelTimestamp` (unsigned int)
* `lastException` (dictionary)
    * `sourceType`
    * `sourceNumber`
    * `generalType`
    * `specificType`
    * `extraData`
    * `timestamp`
    * `description`
* `touchpadText` (string)
* `sirenRepeat` (unsigned int)
* `sirenCadence` (uint32)
* `chime` (bool)
* `energySaver` (bool)
* `noDelay` (bool)
* `latchKey` (bool)
* `silentArm` (bool)
* `quickArm` (bool)

### Command: `get_zones`
Returns a list of zone object paths for this partition.

### Command: `get_lights`
Returns a list of light object paths for this partition.

### Command: `get_schedules`
Returns a list of schedule object paths.

### Command: `get_scheduled_events`
Returns a list of scheduled event object paths.

### Command: `press_buttons`
Executes keypresses

### Command: `set_arm_level`
Immediately changes the current arm level.

### Command: `get_exception_history`
Returns a list of recent exceptions/alarms. Return value is a list of structures that contains the alarm partition, source of the alarm, the type of the alarm, any event-specific data associated with the alarm, and the date/time of the alarm.

### Signal: `changed`
Emits a dictionary describing the changed keys from `get_info`

### Signal: `siren_sync`
Command to start playing the siren cadence. If already playing, it is an indication that the cadence should be synchronized to the first bit.

## Path: `/net/voria/concordd/zone/[zone-number]/`

### Command: `get_info`
Returns a dictionary describing the current status of the zone.

Keys:

* `className` (string, always "zone")
* `zoneId` (unsigned int)
* `partitionId` (unsigned int)
* `name` (string)
* `group` (unsigned int)
* `type` (enum: Hardwired, RF, RF-Touchpad)
* `isTripped` (bool)
* `isBypassed` (bool)
* `isTrouble` (bool)
* `isAlarm` (bool)
* `isFault` (bool)

### Command: `set_bypassed`
Bypass or unbypass this zone.

### Signal: `changed`
Emits a dictionary describing the changed keys from `get_info`

## Path: `/net/voria/concordd/partition/[partition-number]/light/[light-number]/`

### Command: `get_info`
Returns a dictionary describing the current status of the light.

Keys:

* `className` (string, always "light")
* `lightId` (unsigned int)
* `partitionId` (unsigned int)
* `zoneId` (unsigned int)
* `value` (bool)
* `lastChangedBy` (string)
* `lastChangedAt` (unsigned int)

### Command: `set_value`
Sets the new value of the light to either on or off.

### Command: `toggle_value`
Toggles the light.

### Signal: `changed`
Emits a dictionary describing the changed keys from `get_info`

## Path: `/net/voria/concordd/output/[output-number]/`

### Command: `get_info`
Returns a dictionary describing the current status of the output.

Keys:

* `className` (string, always "output")
* `outputId` (unsigned int)
* `name` (string)
* `value` (bool)
* `lastChangedAt` (unsigned int)

### Command: `set_value`
Sets the new value of the output to either on or off.

### Command: `toggle_value`
Toggles the output.

### Signal: `changed`
Emits a dictionary describing the changed keys from `get_info`



## Path: `/net/voria/concordd/partition/[partition-number]/schedule/[schedule-number]/`

## Path: `/net/voria/concordd/partition/[partition-number]/scheduled-event/[scheduled-event-number]/`
