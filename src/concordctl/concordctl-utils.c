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

#include "assert-macros.h"
#include "string-utils.h"
#include "concordctl-utils.h"
#include "concordd-dbus.h"
#include <unistd.h>
#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <signal.h>

int gPartitionIndex = 1;
int gRet = 0;

static bool gDidInterrupt = false;
static sig_t gPreviousHandlerForSIGINT;

static void
signal_SIGINT(int sig)
{
    static const char message[] = "\nCaught SIGINT!\n";

    gRet = ERRORCODE_INTERRUPT;

    // Can't use syslog() because it isn't async signal safe.
    // So we write to stderr
    (void)write(STDERR_FILENO, message, sizeof(message)-1);

	gDidInterrupt = true;

	interrupt_trap_end();
}

void
interrupt_trap_begin() {
	gDidInterrupt = false;
	gPreviousHandlerForSIGINT = signal(SIGINT, &signal_SIGINT);
}

void
interrupt_trap_end() {
	signal(SIGINT, &gPreviousHandlerForSIGINT);
}

bool
interrupt_trap_was_interrupted() {
	return gDidInterrupt;
}


void dump_info_from_iter(FILE* file, DBusMessageIter *iter, int indent, bool bare, bool indentFirstLine)
{
	DBusMessageIter sub_iter;
	int i;

	if (!bare && indentFirstLine) for (i = 0; i < indent; i++) fprintf(file, "\t");

	switch (dbus_message_iter_get_arg_type(iter)) {
	case DBUS_TYPE_DICT_ENTRY:
		dbus_message_iter_recurse(iter, &sub_iter);
		dump_info_from_iter(file, &sub_iter, indent + 1, true, false);
		fprintf(file, " => ");
		dbus_message_iter_next(&sub_iter);
		dump_info_from_iter(file, &sub_iter, indent + 1, bare, false);
		bare = true;
		break;
	case DBUS_TYPE_ARRAY:
		dbus_message_iter_recurse(iter, &sub_iter);
		if (dbus_message_iter_get_arg_type(&sub_iter) == DBUS_TYPE_BYTE ||
		    dbus_message_iter_get_arg_type(&sub_iter) == DBUS_TYPE_INVALID) {
			fprintf(file, "[");
			indent = 0;
		} else {
			fprintf(file, "[\n");
		}

		for (;
		     dbus_message_iter_get_arg_type(&sub_iter) != DBUS_TYPE_INVALID;
		     dbus_message_iter_next(&sub_iter)
		) {
			dump_info_from_iter(file,
			                    &sub_iter,
			                    indent + 1,
			                    dbus_message_iter_get_arg_type(&sub_iter) == DBUS_TYPE_BYTE,
								true);
		}
		for (i = 0; i < indent; i++) fprintf(file, "\t");
		fprintf(file, "]");

		break;
	case DBUS_TYPE_VARIANT:
		dbus_message_iter_recurse(iter, &sub_iter);
		dump_info_from_iter(file, &sub_iter, indent, bare, false);
		bare = true;
		break;
	case DBUS_TYPE_STRING:
	{
		const char* string;
		dbus_message_iter_get_basic(iter, &string);
		fprintf(file, "\"%s\"", string);
	}
	break;

	case DBUS_TYPE_BYTE:
	{
		uint8_t v;
		dbus_message_iter_get_basic(iter, &v);
		if (!bare) {
			fprintf(file, "0x%02X", v);
		} else {
			fprintf(file, "%02X", v);
		}
	}
	break;
	case DBUS_TYPE_UINT16:
	{
		uint16_t v;
		dbus_message_iter_get_basic(iter, &v);
		fprintf(file, "0x%04X", v);
	}
	break;
	case DBUS_TYPE_INT16:
	{
		int16_t v;
		dbus_message_iter_get_basic(iter, &v);
		fprintf(file, "%d", v);
	}
	break;
	case DBUS_TYPE_UINT32:
	{
		uint32_t v;
		dbus_message_iter_get_basic(iter, &v);
		fprintf(file, "%d", v);
	}
	break;
	case DBUS_TYPE_BOOLEAN:
	{
		dbus_bool_t v;
		dbus_message_iter_get_basic(iter, &v);
		fprintf(file, "%s", v ? "true" : "false");
	}
	break;
	case DBUS_TYPE_INT32:
	{
		int32_t v;
		dbus_message_iter_get_basic(iter, &v);
		fprintf(file, "%d", v);
	}
	break;
	case DBUS_TYPE_UINT64:
	{
		uint64_t v;
		dbus_message_iter_get_basic(iter, &v);
		fprintf(file, "0x%016llX", (unsigned long long)v);
	}
	break;
	default:
		fprintf(file, "<%s>",
		        dbus_message_type_to_string(dbus_message_iter_get_arg_type(iter)));
		break;
	}
	if (!bare)
		fprintf(file, "\n");
}


const char*
concordd_status_to_cstr(int status)
{
    static char ret[100];
    // TODO: Make better
    snprintf(ret,sizeof(ret),"%d",status);
    return ret;
}
