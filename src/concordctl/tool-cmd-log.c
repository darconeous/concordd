/*
 *
 * Copyright (c) 2016 Nest Labs, Inc.
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

#undef ASSERT_MACROS_USE_SYSLOG
#define ASSERT_MACROS_USE_SYSLOG 0
//#define DEBUG 1

#include <getopt.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <libgen.h>

#include "concordctl-utils.h"
#include "tool-cmd-log.h"
#include "assert-macros.h"
#include "string-utils.h"
#include "args.h"
#include "concordd-dbus.h"

const char log_cmd_syntax[] = "[args]";

static const arg_list_item_t log_option_list[] = {
    {'h', "help", NULL, "Print Help"},
    {'t', "timeout", "ms", "Set timeout period"},
    {0}
};

static const char gDBusObjectManagerMatchString[] =
	"type='signal'"
	",interface='" CONCORDD_DBUS_INTERFACE "'"
	;

static DBusHandlerResult
dbus_info_changed_handler(
    DBusConnection *connection,
    DBusMessage *   message,
    void *          user_data
) {
	DBusMessageIter iter;
	DBusMessageIter sub_iter;
	const char* path;

	if (!dbus_message_is_signal(message, CONCORDD_DBUS_INTERFACE, CONCORDD_DBUS_SIGNAL_CHANGED)) {
		return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
	}

	path = dbus_message_get_path(message);

	dbus_message_iter_init(message, &iter);

	require(dbus_message_iter_get_arg_type(&iter) == DBUS_TYPE_ARRAY, bail);

	dbus_message_iter_recurse(&iter, &sub_iter);

	for (;
		 dbus_message_iter_get_arg_type(&sub_iter) != DBUS_TYPE_INVALID;
		 dbus_message_iter_next(&sub_iter)
	) {
		DBusMessageIter dict_iter;
		DBusMessageIter val_iter;
		const char* key = NULL;

		require(dbus_message_iter_get_arg_type(&sub_iter) == DBUS_TYPE_DICT_ENTRY, bail);

		dbus_message_iter_recurse(&sub_iter, &dict_iter);

		require(dbus_message_iter_get_arg_type(&dict_iter) == DBUS_TYPE_STRING, bail);

		dbus_message_iter_get_basic(&dict_iter, &key);

		dbus_message_iter_next(&dict_iter);

		require(dbus_message_iter_get_arg_type(&dict_iter) == DBUS_TYPE_VARIANT, bail);

		dbus_message_iter_recurse(&dict_iter, &val_iter);

		fprintf(stdout, "%s: %s=", path, key);
		dump_info_from_iter(stdout, &val_iter, 0, 0, false);
	}

bail:
	return DBUS_HANDLER_RESULT_HANDLED;
}

static DBusHandlerResult
dbus_siren_handler(
    DBusConnection *connection,
    DBusMessage *   message,
    void *          user_data
) {
	const char* path;

	if (!dbus_message_is_signal(message, CONCORDD_DBUS_INTERFACE, CONCORDD_DBUS_SIGNAL_SIREN_SYNC)) {
		return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
	}

	path = dbus_message_get_path(message);

	fprintf(stdout, "%s: %s\n", path, dbus_message_get_member(message));

bail:
	return DBUS_HANDLER_RESULT_HANDLED;
}

static DBusHandlerResult
dbus_event_handler(
    DBusConnection *connection,
    DBusMessage *   message,
    void *          user_data
) {
	DBusMessageIter iter;
	const char* path;

	if ( !dbus_message_is_signal(message, CONCORDD_DBUS_INTERFACE, CONCORDD_DBUS_SIGNAL_ALARM)
	  && !dbus_message_is_signal(message, CONCORDD_DBUS_INTERFACE, CONCORDD_DBUS_SIGNAL_TROUBLE)
	  && !dbus_message_is_signal(message, CONCORDD_DBUS_INTERFACE, CONCORDD_DBUS_SIGNAL_EVENT)
	) {
		return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
	}

	path = dbus_message_get_path(message);

	dbus_message_iter_init(message, &iter);

	require(dbus_message_iter_get_arg_type(&iter) == DBUS_TYPE_ARRAY, bail);

	fprintf(stdout, "%s: %s ", path, dbus_message_get_member(message));
	dump_info_from_iter(stdout, &iter, 0, 0, false);

bail:
	return DBUS_HANDLER_RESULT_HANDLED;
}


int tool_cmd_log(int argc, char *argv[])
{
	int ret = 0;
	int timeout = 5 * 1000;
	int status;
	DBusMessage* message = NULL;
	DBusConnection *connection = NULL;
	DBusMessageIter iter;
	int style = STYLE_RAW;

	DBusError error;

	dbus_error_init(&error);

	while (1) {
		static struct option long_options[] = {
			{"help", no_argument, 0, 'h'},
			{"timeout", required_argument, 0, 't'},
			{"raw", no_argument, 0, 'r'},
			{"style", no_argument, 0, 's'},
			{0, 0, 0, 0}
		};

		int option_index = 0;
		int c;

		c = getopt_long(argc, argv, "rhs:t:", long_options, &option_index);

		if (c == -1) {
			break;
		}

		switch (c) {
		case 'h':
			print_arg_list_help(log_option_list, argv[0],
						log_cmd_syntax);
			ret = ERRORCODE_HELP;
			goto bail;
		case 's':
			if (optarg == NULL) {
				fprintf(stderr, "Possible styles: table, json, raw\n");
				ret = ERRORCODE_HELP;
			} else if (0 == strcmp(optarg, "table")) {
				style = STYLE_TABLE;
			} else if (0 == strcmp(optarg, "json")) {
				style = STYLE_JSON;
			} else if (0 == strcmp(optarg, "raw")) {
				style = STYLE_RAW;
			} else {
				fprintf(stderr, "Possible styles: table, json, raw\n");
				ret = ERRORCODE_HELP;
			}
			break;

		case 'r':
			style = STYLE_RAW;
			break;

		case 't':
			timeout = strtol(optarg, NULL, 0);
			break;
		}
	}

	connection = dbus_bus_get(DBUS_BUS_STARTER, &error);

	if (connection == NULL) {
		dbus_error_free(&error);
		dbus_error_init(&error);
		connection = dbus_bus_get(DBUS_BUS_SYSTEM, &error);
	}

	require_string(connection != NULL, bail, error.message);

	dbus_bus_add_match(connection, gDBusObjectManagerMatchString, &error);

	require_string(!dbus_error_is_set(&error), bail, error.message);

	require(dbus_connection_add_filter(connection, &dbus_info_changed_handler, NULL, NULL), bail);
	//require(dbus_connection_add_filter(connection, &dbus_siren_handler, NULL, NULL), bail);
	require(dbus_connection_add_filter(connection, &dbus_event_handler, NULL, NULL), bail);

	interrupt_trap_begin();

	while (!interrupt_trap_was_interrupted()) {
		dbus_connection_read_write_dispatch(connection, 250 /*ms*/);
	}

	interrupt_trap_end();

	dbus_bus_remove_match(connection, gDBusObjectManagerMatchString, &error);

	require_string(!dbus_error_is_set(&error), bail, error.message);

	dbus_connection_remove_filter(connection, &dbus_event_handler, NULL);
	//dbus_connection_remove_filter(connection, &dbus_siren_handler, NULL);
	dbus_connection_remove_filter(connection, &dbus_info_changed_handler, NULL);

	if (ret) {
		if (error.message != NULL) {
			fprintf(stderr, "%s: error: %s\n", argv[0], error.message);
		}
		goto bail;
	}

bail:
	if (connection) {
		dbus_connection_unref(connection);
	}

	if (message) {
		dbus_message_unref(message);
		message = NULL;
	}

	// Clean up.
	dbus_error_free(&error);

	fflush(stdout);
	fflush(stderr);
	return ret;
}
