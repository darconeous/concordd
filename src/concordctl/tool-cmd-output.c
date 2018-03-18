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

#undef ASSERT_MACROS_USE_SYSLOG
#define ASSERT_MACROS_USE_SYSLOG 0
//#define DEBUG 1

#include <getopt.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>

#include "concordctl-utils.h"
#include "tool-cmd-output.h"
#include "assert-macros.h"
#include "args.h"
#include "concordd-dbus.h"

const char output_cmd_syntax[] = "[args]";

static const arg_list_item_t output_option_list[] = {
    {'h', "help", NULL, "Print Help"},
    {'t', "timeout", "ms", "Set timeout period"},
    {'s', "style", "table|json|raw", "Output style"},
    {0}
};

static bool did_write_table_header = false;

/*
* `className` (string, always "output")
* `outputId` (unsigned int)
* `partitionId` (unsigned int)
* `zoneId` (unsigned int)
* `value` (bool)
* `lastChangedBy` (string)
* `lastChangedAt` (unsigned int)
*/
static int
dump_output_info_table(FILE* file, DBusMessageIter *iter)
{
	int ret = -1;
	DBusMessageIter sub_iter;
	int outputId = -1;
	int zoneId = -1;
	dbus_bool_t value = false;
	const char* name = "";

	if (!did_write_table_header) {
		did_write_table_header = true;
		fprintf(file, " Id|Val|Name\n");
		fprintf(file, "---|---|--------------\n");
	}

	require(dbus_message_iter_get_arg_type(iter) == DBUS_TYPE_ARRAY, bail);

	dbus_message_iter_recurse(iter, &sub_iter);

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

		if (0 == strcmp(key, CONCORDD_DBUS_INFO_CLASS_NAME)) {
			require(dbus_message_iter_get_arg_type(&val_iter) == DBUS_TYPE_STRING, bail);
		} else if (0 == strcmp(key, CONCORDD_DBUS_INFO_OUTPUT_ID)) {
			require(dbus_message_iter_get_arg_type(&val_iter) == DBUS_TYPE_INT32, bail);
			dbus_message_iter_get_basic(&val_iter, &outputId);
		} else if (0 == strcmp(key, CONCORDD_DBUS_INFO_NAME)) {
			require(dbus_message_iter_get_arg_type(&val_iter) == DBUS_TYPE_STRING, bail);
			dbus_message_iter_get_basic(&val_iter, &name);
		} else if (0 == strcmp(key, CONCORDD_DBUS_INFO_VALUE)) {
			require(dbus_message_iter_get_arg_type(&val_iter) == DBUS_TYPE_BOOLEAN, bail);
			dbus_message_iter_get_basic(&val_iter, &value);
		}
	}

	fprintf(file, "%2d", outputId);
	fprintf(file, " | %1d", value);
	fprintf(file, " | %s", name);
	fprintf(file, "\n");

	ret = 0;
bail:
	fflush(file);
	return ret;
}

static int
dump_output_info_json(FILE* file, DBusMessageIter *iter)
{
	dump_info_from_iter(file, iter, 0, 0, false);
	return -1;
}


static int
dump_output_info(FILE* file, DBusMessageIter *iter, int style)
{
	int ret = -1;

	switch (style) {
	case STYLE_TABLE:
		ret = dump_output_info_table(file, iter);
		break;
	case STYLE_JSON:
		ret = dump_output_info_json(file, iter);
		break;
	case STYLE_RAW:
		dump_info_from_iter(file, iter, 0, 0, false);
		ret = 0;
		break;
	default:
		break;
	}
	return ret;
}

static int
get_output_info(DBusConnection *connection, int timeout, int outputId, DBusMessage** reply, DBusMessageIter *iter, DBusError *error)
{
	int ret = ERRORCODE_UNKNOWN;
    DBusMessage *message = NULL;
    char path[DBUS_MAXIMUM_NAME_LENGTH+1];
    char interface_dbus_name[DBUS_MAXIMUM_NAME_LENGTH+1];

    snprintf(
        path,
        sizeof(path),
        "%s%d",
        CONCORDD_DBUS_PATH_OUTPUT,
        outputId
    );

    message = dbus_message_new_method_call(
        CONCORDD_DBUS_NAME,
        path,
        CONCORDD_DBUS_INTERFACE,
        CONCORDD_DBUS_CMD_GET_INFO
    );

    ret = ERRORCODE_TIMEOUT;

    *reply = dbus_connection_send_with_reply_and_block(
        connection,
        message,
        timeout,
        error
    );

    require(*reply != NULL, bail);

    dbus_message_iter_init(*reply, iter);

	ret = 0;
bail:

    if (message) {
        dbus_message_unref(message);
    }

    return ret;
}

int
tool_cmd_output(int argc, char *argv[])
{
	int ret = 0;
	int timeout = 5 * 1000;
	int status;
	DBusMessage* message = NULL;
	DBusConnection *connection = NULL;
	DBusMessageIter iter;
	int style = STYLE_TABLE;

	DBusError error;

	dbus_error_init(&error);

	did_write_table_header = false;

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
			print_arg_list_help(output_option_list, argv[0],
						output_cmd_syntax);
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

	if (connection != NULL) {
		if (optind < argc) {
			ret = 0;
			for (;optind < argc; optind++) {
				int outputId = atoi(argv[optind]);
				status = get_output_info(connection, timeout, outputId, &message, &iter, &error);
				if (status != 0) {
					ret = -1;
					fprintf(stderr, "%s: output-%d error: %s\n", argv[0], outputId, error.message);
					break;
				}
				status = dump_output_info(stdout, &iter, style);
				if (message) {
					dbus_message_unref(message);
					message = NULL;
				}
				if (status != 0){
					fprintf(stderr, "%s: output-%d error printing output\n", argv[0], outputId);
					break;
				}
			}
		} else {
			ret = 0;
			int outputId;
			for (outputId = 0; outputId < 71; outputId++) {
				status = get_output_info(connection, timeout, outputId, &message, &iter, NULL);
				if (status != 0) {
					continue;
				}
				status = dump_output_info(stdout, &iter, style);
				if (message) {
					dbus_message_unref(message);
					message = NULL;
				}
				if (status != 0){
					fprintf(stderr, "%s: output-%d error printing output\n", argv[0], outputId);
					break;
				}
			}
		}
	} else {
		ret = -1;
	}

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
