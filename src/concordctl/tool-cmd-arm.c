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

#if HAVE_CONFIG_H
#include <config.h>
#endif

#include <getopt.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>

#include "concordctl-utils.h"
#include "tool-cmd-arm.h"
#include "assert-macros.h"
#include "args.h"
#include "concordd-dbus.h"

const char set_arm_level_cmd_syntax[] = "[args] {1, 2, 3}";

static const arg_list_item_t set_arm_level_option_list[] = {
	{'h', "help", NULL, "Print Help"},
	{'t', "timeout", "ms", "Set timeout period"},
	{0}
};

static int do_set_arm_level(int arm_level, int timeout, DBusError *error)
{
	int ret = ERRORCODE_UNKNOWN;
	DBusConnection *connection = NULL;
	DBusMessage *message = NULL;
	DBusMessage *reply = NULL;
	char path[DBUS_MAXIMUM_NAME_LENGTH+1];
	char interface_dbus_name[DBUS_MAXIMUM_NAME_LENGTH+1];

	connection = dbus_bus_get(DBUS_BUS_STARTER, error);

	if (!connection) {
		if (error != NULL) {
			dbus_error_free(error);
			dbus_error_init(error);
		}
		connection = dbus_bus_get(DBUS_BUS_SYSTEM, error);
	}

	require(connection != NULL, bail);

	snprintf(
		path,
		sizeof(path),
		"%s%d",
		CONCORDD_DBUS_PATH_PARTITION,
		gPartitionIndex
	);

	message = dbus_message_new_method_call(
		CONCORDD_DBUS_NAME,
		path,
		CONCORDD_DBUS_INTERFACE,
		CONCORDD_DBUS_CMD_SET_ARM_LEVEL
	);

	dbus_message_append_args(
		message,
		DBUS_TYPE_INT32, &arm_level,
		DBUS_TYPE_INVALID
	);

	ret = ERRORCODE_TIMEOUT;

	reply = dbus_connection_send_with_reply_and_block(
		connection,
		message,
		timeout,
		error
	);

	require(reply != NULL, bail);

	dbus_message_get_args(
		reply,
		error,
		DBUS_TYPE_INT32, &ret,
		DBUS_TYPE_INVALID
	);

	if (ret) {
		fprintf(stderr, "set_arm_level: failed with error %d. %s\n", ret, concordd_status_to_cstr(ret));
		goto bail;
	}

bail:
	if (connection) {
		dbus_connection_unref(connection);
	}

	if (message) {
		dbus_message_unref(message);
	}

	if (reply) {
		dbus_message_unref(reply);
	}

	return ret;
}

int
tool_cmd_arm(int argc, char *argv[])
{
	static const int set = 1;
	int ret = 0;
	int timeout = 5 * 1000;
    int arm_level = -1;

	DBusError error;

	dbus_error_init(&error);

	while (1) {
		static struct option long_options[] = {
			{"help", no_argument, 0, 'h'},
			{"timeout", required_argument, 0, 't'},
			{0, 0, 0, 0}
		};

		int option_index = 0;
		int c;

		c = getopt_long(argc, argv, "ht:", long_options, &option_index);

		if (c == -1) {
			break;
		}

		switch (c) {
		case 'h':
			print_arg_list_help(set_arm_level_option_list, argv[0],
					    set_arm_level_cmd_syntax);
			ret = ERRORCODE_HELP;
			goto bail;

		case 't':
			timeout = strtol(optarg, NULL, 0);
			break;
		}
	}

	if (optind < argc) {
        arm_level = strtol(argv[optind], NULL, 0);
		optind++;

	}

    if (arm_level < 1 || arm_level > 3) {
        fprintf(stderr, "%s: error: bad arm level\n", argv[0]);
        ret = ERRORCODE_BADARG;
        goto bail;
    }

	if (optind < argc) {
		fprintf(
			stderr,
		    "%s: error: Unexpected extra argument: \"%s\"\n",
			argv[0],
			argv[optind]
		);
		ret = ERRORCODE_BADARG;
		goto bail;
	}

	ret = do_set_arm_level(arm_level, timeout, &error);

	if (ret) {
		if (error.message != NULL) {
			fprintf(stderr, "%s: error: %s\n", argv[0], error.message);
		}
		goto bail;
	}

bail:

	// Clean up.
	dbus_error_free(&error);

	return ret;
}
