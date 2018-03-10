/*
 *
 * Copyright (c) 2017 Robert Quattlebaum
 * Portions Copyright (c) 2016 Nest Labs, Inc.
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

#ifndef concordd_concordctl_cmds_h
#define concordd_concordctl_cmds_h

//#include "tool-cmd-list-partitions.h"
#include "tool-cmd-arm.h"
#include "tool-cmd-log.h"
#include "tool-cmd-zone.h"
#include "tool-cmd-light.h"
#include "tool-cmd-output.h"
#include "tool-cmd-keypad.h"
#include "tool-cmd-partition-info.h"

#include "concordctl-utils.h"

#define CONCORDCTL_CLI_COMMANDS \
    { \
        "partition-info", \
        "Get info about partition", \
        &tool_cmd_partition_info \
    }, \
    { "status", "", &tool_cmd_partition_info, 1 }, \
	{ \
		"arm", \
		"Read or change the partition arm level", \
		&tool_cmd_arm \
	}, \
    { "set-arm-level", "", &tool_cmd_arm, 1 }, \
    { \
        "keypad-input", \
        "Simulate button presses on the keypad", \
        &tool_cmd_keypad_input \
    }, \
    { "press-keys", "", &tool_cmd_keypad_input, 1 }, \
    { \
        "keypad-text", \
        "Print out the current text on the keypad", \
        &tool_cmd_keypad_text \
    }, \
    { \
        "keypad-emu", \
        "Emulate a touchpad", \
        &tool_cmd_keypad_emu \
    }, \
    { \
        "zone", \
        "Prints out info for one or more zones", \
        &tool_cmd_zone \
    }, \
    { \
        "light", \
        "Prints out info for one or more lights", \
        &tool_cmd_light \
    }, \
    { \
        "output", \
        "Prints out info for one or more outputs", \
        &tool_cmd_output \
    }, \
    { \
        "log", \
        "Prints out changes to the system as they occur", \
        &tool_cmd_log \
    }, \
//    { \
//        "list-partitions", \
//        "List partitions", \
//        &tool_cmd_list_partitions \
//    }, \
//    { "lp", "", &tool_cmd_list_partitions, 1 }, \


#endif
