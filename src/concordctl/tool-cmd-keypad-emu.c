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
#include <poll.h>
#include <termios.h>
#include "concordctl-utils.h"
#include "tool-cmd-log.h"
#include "assert-macros.h"
#include "string-utils.h"
#include "args.h"
#include "concordd-dbus.h"

const char keypad_emu_cmd_syntax[] = "[args]";

static const arg_list_item_t keypad_emu_option_list[] = {
    {'h', "help", NULL, "Print Help"},
    {'t', "timeout", "ms", "Set timeout period"},
    {0}
};

static const char gDBusObjectManagerMatchString[] =
	"type='signal'"
	",interface='" CONCORDD_DBUS_INTERFACE "'"
	;

#define ANSI_ED0          "\x1b[J"
#define ANSI_EL0          "\x1b[K"
#define ANSI_EL1          "\x1b[1K"
#define ANSI_SCP          "\x1b[s"
#define ANSI_RCP          "\x1b[u"
#define ANSI_HIDE_CURSOR  "\x1b[25l"
#define ANSI_SHOW_CURSOR  "\x1b[25l"
#define ANSI_SGR_RESET    "\x1b[0m"
#define ANSI_SGR_REVERSE  "\x1b[7m"
#define ANSI_SGR_YELLOW   "\x1b[33m"
#define ANSI_SGR_MAGENTA  "\x1b[35m"

static bool gUseAnsi = true;
static struct termios gPrevious;

static void enter_raw_termios() {
	struct termios raw;
	tcgetattr(STDIN_FILENO, &raw);
	tcgetattr(STDIN_FILENO, &gPrevious);
	raw.c_lflag &= ~(ECHO|ICANON);
	tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
	if (gUseAnsi) {
		fprintf(stdout, ANSI_HIDE_CURSOR);
		fprintf(stdout, ANSI_SCP);
		fflush(stdout);
	}
}

static void exit_raw_termios() {
  tcsetattr(STDIN_FILENO, TCSAFLUSH, &gPrevious);
	if (gUseAnsi) {
		fprintf(stdout, ANSI_SHOW_CURSOR "\n");
		fflush(stdout);
	}
}



char touchpad_text[200];
uint32_t touchpad_armlevel;
uint32_t touchpad_sirenRepeat;
char touchpad_sirenCadence[33];

static void
touchpad_display_init()
{
	touchpad_text[0] = 0;
	touchpad_armlevel = 0;
	touchpad_sirenRepeat = 0;
	touchpad_sirenCadence[0] = 0;
}
static void
touchpad_display_draw()
{
	if (gUseAnsi) {
		fprintf(stdout, ANSI_EL1 "\r");
		fprintf(stdout, ANSI_RCP ANSI_ED0);
		fprintf(stdout, ANSI_SGR_RESET);
		switch (touchpad_armlevel) {
		default:
		case 1:
			break;
		case 2:
			fprintf(stdout, ANSI_SGR_YELLOW);
			break;
		case 3:
			fprintf(stdout, ANSI_SGR_MAGENTA);
			break;
		}
		if (touchpad_sirenRepeat == 0 && strstr(touchpad_sirenCadence,"1") != NULL) {
			// The siren is sounding. Indicate this visually.
			fprintf(stdout, ANSI_SGR_REVERSE);
		}
		fprintf(stdout, " %s ", touchpad_text);
		fprintf(stdout, ANSI_SGR_RESET);
	} else {
		fprintf(stdout, "\r                                          ");
		fprintf(stdout, "\r%s",touchpad_text);
	}
	fflush(stdout);
}

static void
touchpad_display_update(DBusMessageIter *iter)
{
	DBusMessageIter sub_iter;

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

		if (strequal(key, CONCORDD_DBUS_INFO_TOUCHPAD_TEXT)) {
			const char* text = NULL;
			if (dbus_message_iter_get_arg_type(&val_iter) == DBUS_TYPE_STRING) {
				dbus_message_iter_get_basic(&val_iter, &text);
				strcpy(touchpad_text, text);
			}
		} else if (strequal(key, CONCORDD_DBUS_INFO_ARM_LEVEL)) {
			if (dbus_message_iter_get_arg_type(&val_iter) == DBUS_TYPE_INT32) {
				dbus_message_iter_get_basic(&val_iter, &touchpad_armlevel);
			}
		} else if (strequal(key, CONCORDD_DBUS_INFO_SIREN_REPEAT)) {
			if (dbus_message_iter_get_arg_type(&val_iter) == DBUS_TYPE_INT32) {
				dbus_message_iter_get_basic(&val_iter, &touchpad_sirenRepeat);
			}
		} else if (strequal(key, CONCORDD_DBUS_INFO_SIREN_CADENCE)) {
			const char* text = NULL;
			if (dbus_message_iter_get_arg_type(&val_iter) == DBUS_TYPE_STRING) {
				dbus_message_iter_get_basic(&val_iter, &text);
				strcpy(touchpad_sirenCadence, text);
			}
		}
		/*
		CONCORDD_DBUS_INFO_CHIME     "ch
		CONCORDD_DBUS_INFO_ENERGY_SAVER
		CONCORDD_DBUS_INFO_NO_DELAY
		CONCORDD_DBUS_INFO_LATCH_KEY
		CONCORDD_DBUS_INFO_SILENT_ARM
		CONCORDD_DBUS_INFO_QUICK_ARM
		*/
	}

bail:
	return;
}

static DBusHandlerResult
dbus_info_changed_handler(
    DBusConnection *connection,
    DBusMessage *   message,
    void *          user_data
) {
	DBusMessageIter iter;
	DBusMessageIter sub_iter;
	char path[100];

	if (!dbus_message_is_signal(message, CONCORDD_DBUS_INTERFACE, CONCORDD_DBUS_SIGNAL_CHANGED)) {
		return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
	}

	snprintf(path, sizeof(path), "%s%d", CONCORDD_DBUS_PATH_PARTITION, gPartitionIndex);

	if (!strequal(path, dbus_message_get_path(message))) {
		return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
	}

	dbus_message_iter_init(message, &iter);

	touchpad_display_update(&iter);
	touchpad_display_draw();

bail:
	return DBUS_HANDLER_RESULT_HANDLED;
}

static int refresh_touchpad(DBusConnection *connection, int timeout, DBusError *error)
{
    int ret = ERRORCODE_UNKNOWN;
    DBusMessage *message = NULL;
    DBusMessage *reply = NULL;
    char path[DBUS_MAXIMUM_NAME_LENGTH+1];
    char interface_dbus_name[DBUS_MAXIMUM_NAME_LENGTH+1];
    DBusMessageIter iter;

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
        CONCORDD_DBUS_CMD_GET_INFO
    );

    ret = ERRORCODE_TIMEOUT;

    reply = dbus_connection_send_with_reply_and_block(
        connection,
        message,
        timeout,
        error
    );

    require(reply != NULL, bail);

    dbus_message_iter_init(reply, &iter);

	touchpad_display_update(&iter);
	touchpad_display_draw();

    ret = 0;
bail:
    if (message) {
        dbus_message_unref(message);
    }

    if (reply) {
        dbus_message_unref(reply);
    }

    return ret;
}

static int
dispatch_keypress(DBusConnection *connection, const char* keys, int timeout, DBusError *error)
{
    int ret = ERRORCODE_UNKNOWN;
    DBusMessage *message = NULL;
    DBusMessage *reply = NULL;
    char path[DBUS_MAXIMUM_NAME_LENGTH+1];
    char interface_dbus_name[DBUS_MAXIMUM_NAME_LENGTH+1];
    DBusMessageIter iter;

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
        CONCORDD_DBUS_CMD_PRESS_KEYS
    );

	dbus_message_append_args(
		message,
		DBUS_TYPE_STRING, &keys,
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

    dbus_message_iter_init(reply, &iter);

    ret = 0;
bail:
    if (message) {
        dbus_message_unref(message);
    }

    if (reply) {
        dbus_message_unref(reply);
    }

    return ret;
}

bool
checkpoll(int fd, short poll_flags)
{
    bool ret = false;

    if (fd >= 0) {
        struct pollfd pollfd = { fd, poll_flags, 0 };
        IGNORE_RETURN_VALUE( poll(&pollfd, 1, 0) );
        ret = ((pollfd.revents & poll_flags) != 0);
    }

    return ret;
}

static int
handle_keypresses(DBusConnection *connection, int timeout, DBusError *error)
{
	int ret = 0;
	char keys[30];
	while (ret == 0 && checkpoll(STDIN_FILENO, POLLIN)) {
		memset(keys, 0, sizeof(keys));

		if (read(STDIN_FILENO, keys, 1) < 0) {
			ret = -1;
			break;
		}

		if (keys[0] == 0x1B) {
			char* ptr = keys + 1;
			if (read(STDIN_FILENO, ptr, 1) < 0) {
				ret = -1;
				break;
			}
			if (*ptr == '[') {
				while(ret == 0) {
					ptr++;
					if (read(STDIN_FILENO, ptr, 1) < 0) {
						ret = -1;
						break;
					}
					if (*ptr < 32) {
						break;
					}
					if ((*ptr >= 64) && (*ptr<=126)) {
						break;
					}
				}
				if (keys[2] == 'B') {
					ret = dispatch_keypress(connection, "[2C]", timeout, error);
				} else if (keys[2] == 'A') {
					ret = dispatch_keypress(connection, "[30]", timeout, error);
				} else {
					fprintf(stderr,"\r <%s> ",keys+1);
					fflush(stderr);
				}
			}
		} else if ((keys[0] == 'q') || (keys[0] == 'Q')) {
			// Quit
			ret = -2;
		} else if (keys[0] == '\n') {
			ret = dispatch_keypress(connection, "#", timeout, error);
		} else if ((keys[0] == '*') || (keys[0] == '#')) {
			ret = dispatch_keypress(connection, keys, timeout, error);
		} else if ((keys[0] >= 'a') && (keys[0] <= 'f')) {
			ret = dispatch_keypress(connection, keys, timeout, error);
		} else if ((keys[0] >= 'A') && (keys[0] <= 'F')) {
			ret = dispatch_keypress(connection, keys, timeout, error);
		} else if ((keys[0] >= '0') && (keys[0] <= '9')) {
			ret = dispatch_keypress(connection, keys, timeout, error);
		}
	}
	return ret;
}

int tool_cmd_keypad_emu(int argc, char *argv[])
{
	int ret = 0;
	int timeout = 5 * 1000;
	int status;
	DBusMessage* message = NULL;
	DBusConnection *connection = NULL;
	DBusMessageIter iter;

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
			print_arg_list_help(keypad_emu_option_list, argv[0],
						keypad_emu_cmd_syntax);
			ret = ERRORCODE_HELP;
			goto bail;

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

	refresh_touchpad(connection, timeout, &error);

	require_string(!dbus_error_is_set(&error), bail, error.message);

	dbus_bus_add_match(connection, gDBusObjectManagerMatchString, &error);

	require_string(!dbus_error_is_set(&error), bail, error.message);

	require(dbus_connection_add_filter(connection, &dbus_info_changed_handler, NULL, NULL), bail);

	interrupt_trap_begin();
	enter_raw_termios();

	ret = 0;

	while (ret == 0 && !interrupt_trap_was_interrupted()) {
		ret = handle_keypresses(connection, timeout, &error);
		dbus_connection_read_write_dispatch(connection, 40 /*ms*/);
	}

	dbus_bus_remove_match(connection, gDBusObjectManagerMatchString, &error);

	require_string(!dbus_error_is_set(&error), bail, error.message);

	dbus_connection_remove_filter(connection, &dbus_info_changed_handler, NULL);

	if (ret) {
		if (error.message != NULL) {
			fprintf(stderr, "%s: error: %s\n", argv[0], error.message);
		}
		goto bail;
	}

bail:
	exit_raw_termios();
	interrupt_trap_end();

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
