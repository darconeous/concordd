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
#include "tool-cmd-zone.h"
#include "assert-macros.h"
#include "args.h"
#include "concordd-dbus.h"

const char zone_cmd_syntax[] = "[args] [<zone-id> <zone-id> ...]";

static const arg_list_item_t zone_option_list[] = {
    {'h', "help", NULL, "Print Help"},
    {'t', "timeout", "ms", "Set timeout period"},
    {'a', "all", NULL, "Print all zones from all partitions"},
    {'b', "bypass", NULL, "Bypass zone(s)"},
    {'B', "unbypass", NULL, "Unbypass zone(s)"},
    {'u', "user", "user-id", "User id to assume for bypass/unbypass"},
    {'r', NULL, NULL, "Print out raw structures"},
    {0}
};

static bool did_write_table_header = false;

static int
dump_zone_info_table(FILE* file, DBusMessageIter *iter)
{
	int ret = -1;
	DBusMessageIter sub_iter;
	const char* name = NULL;
	int zoneId = -1;
	int partId = -1;
	int type = -1;
	int group = -1;
	int32_t time_since_changed = 0;
	dbus_bool_t isTripped = false;
	dbus_bool_t isBypassed = false;
	dbus_bool_t isTrouble = false;
	dbus_bool_t isAlarm = false;
	dbus_bool_t isFault = false;

	if (!did_write_table_header) {
		did_write_table_header = true;
		fprintf(file, " %30.30s ", "Zone Name");
		fprintf(file, "| Id ");
		fprintf(file, "|Par");
		fprintf(file, "|Typ");
		fprintf(file, "| Grp");
		fprintf(file, "|Trp|Byp|Tro|Alr|Flt");
		fprintf(file, "|Last Changed");
		fprintf(file, "\n");
		fprintf(file, "-%30.30s-", "--------------------------------------------------");
		fprintf(file, "+----");
		fprintf(file, "+---");
		fprintf(file, "+---");
		fprintf(file, "+----");
		fprintf(file, "+---+---+---+---+---");
		fprintf(file, "|------------");
		fprintf(file, "\n");
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
		} else if (0 == strcmp(key, CONCORDD_DBUS_INFO_ZONE_ID)) {
			require(dbus_message_iter_get_arg_type(&val_iter) == DBUS_TYPE_INT32, bail);
			dbus_message_iter_get_basic(&val_iter, &zoneId);
		} else if (0 == strcmp(key, CONCORDD_DBUS_INFO_PARTITION_ID)) {
			require(dbus_message_iter_get_arg_type(&val_iter) == DBUS_TYPE_INT32, bail);
			dbus_message_iter_get_basic(&val_iter, &partId);
		} else if (0 == strcmp(key, CONCORDD_DBUS_INFO_NAME)) {
			require(dbus_message_iter_get_arg_type(&val_iter) == DBUS_TYPE_STRING, bail);
			dbus_message_iter_get_basic(&val_iter, &name);
		} else if (0 == strcmp(key, CONCORDD_DBUS_INFO_GROUP)) {
			if (dbus_message_iter_get_arg_type(&val_iter) != DBUS_TYPE_INT32) {
				continue;
			}
			dbus_message_iter_get_basic(&val_iter, &group);
		} else if (0 == strcmp(key, CONCORDD_DBUS_INFO_TYPE)) {
			if (dbus_message_iter_get_arg_type(&val_iter) != DBUS_TYPE_INT32) {
				continue;
			}
			dbus_message_iter_get_basic(&val_iter, &type);
		} else if (0 == strcmp(key, CONCORDD_DBUS_INFO_IS_TRIPPED)) {
			require(dbus_message_iter_get_arg_type(&val_iter) == DBUS_TYPE_BOOLEAN, bail);
			dbus_message_iter_get_basic(&val_iter, &isTripped);
		} else if (0 == strcmp(key, CONCORDD_DBUS_INFO_IS_BYPASSED)) {
			require(dbus_message_iter_get_arg_type(&val_iter) == DBUS_TYPE_BOOLEAN, bail);
			dbus_message_iter_get_basic(&val_iter, &isBypassed);
		} else if (0 == strcmp(key, CONCORDD_DBUS_INFO_IS_TROUBLE)) {
			require(dbus_message_iter_get_arg_type(&val_iter) == DBUS_TYPE_BOOLEAN, bail);
			dbus_message_iter_get_basic(&val_iter, &isTrouble);
		} else if (0 == strcmp(key, CONCORDD_DBUS_INFO_IS_ALARM)) {
			require(dbus_message_iter_get_arg_type(&val_iter) == DBUS_TYPE_BOOLEAN, bail);
			dbus_message_iter_get_basic(&val_iter, &isAlarm);
		} else if (0 == strcmp(key, CONCORDD_DBUS_INFO_IS_FAULT)) {
			require(dbus_message_iter_get_arg_type(&val_iter) == DBUS_TYPE_BOOLEAN, bail);
			dbus_message_iter_get_basic(&val_iter, &isFault);
		} else if (0 == strcmp(key, CONCORDD_DBUS_INFO_LAST_CHANGED_AT)) {
			int32_t last_changed = 0;
			if (dbus_message_iter_get_arg_type(&val_iter) != DBUS_TYPE_INT32) {
				continue;
			}
			dbus_message_iter_get_basic(&val_iter, &last_changed);
			if (last_changed != 0) {
				time_since_changed = time(NULL)-last_changed;
			}
		}
	}

	fprintf(file, " %30.30s", name);
	fprintf(file, " | %2d", zoneId);
	fprintf(file, " | %1d", partId);
	fprintf(file, " | %1d", type);
	fprintf(file, " | %2d", group);
	fprintf(file, " | %c | %c | %c | %c | %c",
		isTripped?'T':' ',
		isBypassed?'B':' ',
		isTrouble?'X':' ',
		isAlarm?'A':' ',
		isFault?'F':' '
	);
	if (time_since_changed > (60*60*24)) {
		fprintf(file, " | %.1fd ago", (double)time_since_changed/(60*60*24));
	} else if (time_since_changed > (60*60)) {
		fprintf(file, " | %.1fh ago", (double)time_since_changed/(60*60));
	} else if (time_since_changed > (60)) {
		fprintf(file, " | %.1fm ago", (double)time_since_changed/(60));
	} else if (time_since_changed > 0) {
		fprintf(file, " | %ds ago", time_since_changed);
	} else {
		fprintf(file, " | ");
	}
	fprintf(file, "\n");

	ret = 0;
bail:
	fflush(file);
	return ret;
}

static int
dump_zone_info_json(FILE* file, DBusMessageIter *iter)
{
	dump_info_from_iter(file, iter, 0, 0, false);
	return -1;
}


static int
dump_zone_info(FILE* file, DBusMessageIter *iter, int style)
{
	int ret = -1;

	switch (style) {
	case STYLE_TABLE:
		ret = dump_zone_info_table(file, iter);
		break;
	case STYLE_JSON:
		ret = dump_zone_info_json(file, iter);
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
get_zone_info(DBusConnection *connection, int timeout, int zoneId, DBusMessage** reply, DBusMessageIter *iter, DBusError *error)
{
	int ret = ERRORCODE_UNKNOWN;
    DBusMessage *message = NULL;
    char path[DBUS_MAXIMUM_NAME_LENGTH+1];
    char interface_dbus_name[DBUS_MAXIMUM_NAME_LENGTH+1];

    snprintf(
        path,
        sizeof(path),
        "%s%d",
        CONCORDD_DBUS_PATH_ZONE,
        zoneId
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

static int
zone_bypass(DBusConnection *connection, int timeout, int zoneId, bool state, int userId, DBusError *error)
{
	int ret = ERRORCODE_UNKNOWN;
    DBusMessage *message = NULL;
    DBusMessage *reply = NULL;
    char path[DBUS_MAXIMUM_NAME_LENGTH+1];
    char interface_dbus_name[DBUS_MAXIMUM_NAME_LENGTH+1];
	dbus_bool_t state_d = state;

    snprintf(
        path,
        sizeof(path),
        "%s%d",
        CONCORDD_DBUS_PATH_ZONE,
        zoneId
    );

    message = dbus_message_new_method_call(
        CONCORDD_DBUS_NAME,
        path,
        CONCORDD_DBUS_INTERFACE,
        CONCORDD_DBUS_CMD_SET_BYPASSED
    );

	dbus_message_append_args(
        message,
        DBUS_TYPE_BOOLEAN, &state_d,
		DBUS_TYPE_INT32, &userId,
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
        fprintf(stderr, "zone_bypass: failed with error %d. %s\n", ret, concordd_status_to_cstr(ret));
        goto bail;
    }

bail:

    if (message) {
        dbus_message_unref(message);
    }

	if (reply) {
        dbus_message_unref(reply);
    }

    return ret;
}

int
tool_cmd_zone(int argc, char *argv[])
{
    int ret = 0;
    int zone_count = 0;
    int timeout = 5 * 1000;
	bool all_zones = false;
	int status;
	DBusMessage* message = NULL;
    DBusConnection *connection = NULL;
	DBusMessageIter iter;
	int style = STYLE_TABLE;
	int userId = -1;
	enum {
		ACTION_GET_INFO,
		ACTION_BYPASS,
		ACTION_UNBYPASS
	} action = ACTION_GET_INFO;

    DBusError error;

    dbus_error_init(&error);

	did_write_table_header = false;

    while (1) {
        static struct option long_options[] = {
            {"help", no_argument, 0, 'h'},
            {"all", no_argument, 0, 'a'},
            {"timeout", required_argument, 0, 't'},
            {"bypass", no_argument, 0, 'b'},
            {"unbypass", no_argument, 0, 'B'},
            {"user", required_argument, 0, 'u'},
            {"raw", no_argument, 0, 'r'},
            {0, 0, 0, 0}
        };

        int option_index = 0;
        int c;

        c = getopt_long(argc, argv, "rhaBbu:t:", long_options, &option_index);

        if (c == -1) {
            break;
        }

        switch (c) {
        case 'h':
            print_arg_list_help(zone_option_list, argv[0],
						zone_cmd_syntax);
            ret = ERRORCODE_HELP;
            goto bail;

		case 'b':
			action = ACTION_BYPASS;
			break;

		case 'B':
			action = ACTION_UNBYPASS;
			break;

		case 'u':
            userId = strtol(optarg, NULL, 0);
			break;

		case 'a':
			all_zones = true;
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
		if (action == ACTION_GET_INFO) {
			if (optind < argc) {
				ret = 0;
				for (;optind < argc; optind++) {
					int zoneId = atoi(argv[optind]);
					status = get_zone_info(connection, timeout, zoneId, &message, &iter, &error);
					if (status != 0) {
						ret = -1;
						fprintf(stderr, "%s: zone-%d error: %s\n", argv[0], zoneId, error.message);
						dbus_error_free(&error);
						dbus_error_init(&error);
						continue;
					}
					status = dump_zone_info(stdout, &iter, style);
					if (message) {
						dbus_message_unref(message);
						message = NULL;
					}
					if (status != 0){
						fprintf(stderr, "%s: zone-%d error printing zone\n", argv[0], zoneId);
						break;
					}
					zone_count++;
				}
			} else {
				ret = 0;
				int zoneId;
				for (zoneId = 0; zoneId < 96; zoneId++) {
					status = get_zone_info(connection, timeout, zoneId, &message, &iter, &error);
					if (status != 0) {
						dbus_error_free(&error);
						dbus_error_init(&error);
						continue;
					}
					// TODO: Check to see if this is in our partition or not, unless all_zones is true
					status = dump_zone_info(stdout, &iter, style);
					if (message) {
						dbus_message_unref(message);
						message = NULL;
					}
					if (status != 0){
						fprintf(stderr, "%s: zone-%d error printing zone\n", argv[0], zoneId);
						break;
					}
					zone_count++;
				}
				fprintf(stderr, "%d zones total.\n", zone_count);
			}
		} else if ((action == ACTION_BYPASS) || (action == ACTION_UNBYPASS)) {
			ret = 0;
			for (;optind < argc; optind++) {
				int zoneId = atoi(argv[optind]);
				status = zone_bypass(connection, timeout, zoneId, action == ACTION_BYPASS, userId, &error);
				if (status != 0) {
					ret = -1;
					fprintf(stderr, "%s: zone-%d error: %s\n", argv[0], zoneId, error.message);
					dbus_error_free(&error);
					dbus_error_init(&error);
					continue;
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
