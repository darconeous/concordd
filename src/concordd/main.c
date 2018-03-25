#if HAVE_CONFIG_H
#include <config.h>
#endif

#ifndef ASSERT_MACROS_USE_SYSLOG
#define ASSERT_MACROS_USE_SYSLOG 1
#endif

#ifndef ASSERT_MACROS_SQUELCH
#define ASSERT_MACROS_SQUELCH 0
#endif

#include "assert-macros.h"

#include "concordd.h"
#include "concordd-config.h"
#include "concordd-dbus-server.h"

#include "config-file.h"
#include "args.h"
#include "socket-utils.h"
#include "string-utils.h"
#include "version.h"
#include "crash-trace.h"

#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <string.h>
#include <ctype.h>

#include <unistd.h>
#include <signal.h>

#include <poll.h>
#include <sys/select.h>

#include <syslog.h>
#include <errno.h>
#include <libgen.h>

#if HAVE_PWD_H
#include <pwd.h>
#endif

#include "time-utils.h"

#ifndef PREFIX
#define PREFIX "/usr/local"
#endif

#ifndef SYSCONFDIR
#define SYSCONFDIR PREFIX "/etc"
#endif

#ifndef DEFAULT_MAX_LOG_LEVEL
#if DEBUG
#define DEFAULT_MAX_LOG_LEVEL	LOG_INFO
#else
#define DEFAULT_MAX_LOG_LEVEL	LOG_NOTICE
#endif
#endif

#ifndef CONCORDD_DEFAULT_PRIV_DROP_USER
#define CONCORDD_DEFAULT_PRIV_DROP_USER     NULL
#endif

#ifndef CONCORDD_DEFAULT_CHROOT_PATH
#define CONCORDD_DEFAULT_CHROOT_PATH        NULL
#endif


#ifndef SOURCE_VERSION
#define SOURCE_VERSION		PACKAGE_VERSION
__BEGIN_DECLS
#include "version.c.in"
__END_DECLS
#endif

static arg_list_item_t option_list[] = {
	{ 'h', "help",   NULL,	"Print Help"},
	{ 'd', "debug",  "<level>", "Enable debugging mode"},
	{ 'c', "config", "<filename>", "Config File"},
	{ 'o', "option", "<option-string>", "Config option"},
	{ 's', "socket", "<socket>", "Socket file"},
	{ 'v', "version", NULL, "Print version" },
#if HAVE_PWD_H
	{ 'u', "user", NULL, "Username for dropping privileges" },
#endif
	{ 0 }
};

#define ERRORCODE_INTERRUPT    EXIT_FAILURE
#define ERRORCODE_QUIT         -2
#define ERRORCODE_SIGHUP       EXIT_FAILURE
#define ERRORCODE_UNKNOWN      EXIT_FAILURE
#define ERRORCODE_BADARG      EXIT_FAILURE
#define ERRORCODE_HELP      EXIT_FAILURE
#define ERRORCODE_ERRNO      EXIT_FAILURE

static int gRet;

static const char* gProcessName = "concordd";
static const char* gPIDFilename = NULL;
static const char* gChroot = CONCORDD_DEFAULT_CHROOT_PATH;
static const char* gSocketPath = "/dev/null";

static const char* gPartitionAlarmCommand;
static const char* gPartitionTroubleCommand;
static const char* gPartitionEventCommand;
static const char* gSystemTroubleCommand;
static const char* gSystemEventCommand;
static const char* gLightChangedCommand;
static const char* gOutputChangedCommand;
static const char* gZoneChangedCommand;
static const char* gAcPowerFailureCommand;
static const char* gAcPowerRestoredCommand;

#if HAVE_PWD_H
static const char* gPrivDropToUser = CONCORDD_DEFAULT_PRIV_DROP_USER;
#endif

/* ------------------------------------------------------------------------- */
/* MARK: Signal Handlers */

static sig_t gPreviousHandlerForSIGINT;
static sig_t gPreviousHandlerForSIGTERM;

static void
signal_SIGINT(int sig)
{
	static const char message[] = "\nCaught SIGINT!\n";

	gRet = ERRORCODE_INTERRUPT;

	// Can't use syslog() because it isn't async signal safe.
	// So we write to stderr
	(void)write(STDERR_FILENO, message, sizeof(message)-1);

	// Restore the previous handler so that if we end up getting
	// this signal again we peform the system default action.
	signal(SIGINT, gPreviousHandlerForSIGINT);
	gPreviousHandlerForSIGINT = NULL;
}

static void
signal_SIGTERM(int sig)
{
	static const char message[] = "\nCaught SIGTERM!\n";

	gRet = ERRORCODE_QUIT;

	// Can't use syslog() because it isn't async signal safe.
	// So we write to stderr
	(void)write(STDERR_FILENO, message, sizeof(message)-1);

	// Restore the previous handler so that if we end up getting
	// this signal again we peform the system default action.
	signal(SIGTERM, gPreviousHandlerForSIGTERM);
	gPreviousHandlerForSIGTERM = NULL;
}

static void
signal_SIGHUP(int sig)
{
	static const char message[] = "\nCaught SIGHUP!\n";

	gRet = ERRORCODE_SIGHUP;

	// Can't use syslog() because it isn't async signal safe.
	// So we write to stderr
	(void)write(STDERR_FILENO, message, sizeof(message)-1);

	// We don't restore the "previous handler"
	// because we always want to let the main
	// loop decide what to do for hangups.
}

static void
signal_SIGCHLD(int sig)
{
	// Automatically call wait as children terminate.
    int stat = 0;
    pid_t pid = -1;

	do {
		pid = waitpid(-1, &stat, WNOHANG);
	} while (pid > 0);
}

/* ------------------------------------------------------------------------- */
/* MARK: Misc. */



// TODO: Refactor and clean up.
static int
set_config_param(
    void* ignored, const char* key, const char* value
    )
{
	int ret = -1;

	syslog(LOG_INFO, "set-config-param: \"%s\" = \"%s\"", key, value);

	if (strcaseequal(key, kCONCORDDConfig_SocketBaud)) {
		int baud = atoi(value);
		ret = 0;
		require(9600 <= baud, bail);
		gSocketWrapperBaud = baud;
#if HAVE_PWD_H
	} else if (strcaseequal(key, kCONCORDDConfig_PrivDropToUser)) {
		if (value[0] == 0) {
			gPrivDropToUser = NULL;
		} else {
			gPrivDropToUser = strdup(value);
		}
		ret = 0;
#endif
	} else if (strcaseequal(key, kCONCORDDConfig_Chroot)) {
		if (value[0] == 0) {
			gChroot = NULL;
		} else {
			gChroot = strdup(value);
		}
		ret = 0;
    } else if (strcaseequal(key, kCONCORDDConfig_SocketPath)) {
        if (value[0] == 0) {
            gSocketPath = NULL;
        } else {
            gSocketPath = strdup(value);
        }
        ret = 0;
	} else if (strcaseequal(key, kCONCORDDConfig_SyslogMask)) {
		setlogmask(strtologmask(value, setlogmask(0)));
		ret = 0;

    } else if (strcaseequal(key, kCONCORDDConfig_PartitionAlarmCommand)) {
        if (value[0] == 0) {
            gPartitionAlarmCommand = NULL;
        } else {
            gPartitionAlarmCommand = strdup(value);
        }
        ret = 0;
    } else if (strcaseequal(key, kCONCORDDConfig_PartitionTroubleCommand)) {
        if (value[0] == 0) {
            gPartitionTroubleCommand = NULL;
        } else {
            gPartitionTroubleCommand = strdup(value);
        }
        ret = 0;
    } else if (strcaseequal(key, kCONCORDDConfig_PartitionEventCommand)) {
        if (value[0] == 0) {
            gPartitionEventCommand = NULL;
        } else {
            gPartitionEventCommand = strdup(value);
        }
        ret = 0;
    } else if (strcaseequal(key, kCONCORDDConfig_SystemTroubleCommand)) {
        if (value[0] == 0) {
            gSystemTroubleCommand = NULL;
        } else {
            gSystemTroubleCommand = strdup(value);
        }
        ret = 0;
    } else if (strcaseequal(key, kCONCORDDConfig_SystemEventCommand)) {
        if (value[0] == 0) {
            gSystemEventCommand = NULL;
        } else {
            gSystemEventCommand = strdup(value);
        }
        ret = 0;

    } else if (strcaseequal(key, kCONCORDDConfig_LightChangedCommand)) {
        if (value[0] == 0) {
            gLightChangedCommand = NULL;
        } else {
            gLightChangedCommand = strdup(value);
        }
        ret = 0;

    } else if (strcaseequal(key, kCONCORDDConfig_OutputChangedCommand)) {
        if (value[0] == 0) {
            gOutputChangedCommand = NULL;
        } else {
            gOutputChangedCommand = strdup(value);
        }
        ret = 0;

	} else if (strcaseequal(key, kCONCORDDConfig_ZoneChangedCommand)) {
        if (value[0] == 0) {
            gZoneChangedCommand = NULL;
        } else {
            gZoneChangedCommand = strdup(value);
        }
        ret = 0;

	} else if (strcaseequal(key, kCONCORDDConfig_AcPowerFailureCommand)) {
        if (value[0] == 0) {
            gAcPowerFailureCommand = NULL;
        } else {
            gAcPowerFailureCommand = strdup(value);
        }
        ret = 0;

	} else if (strcaseequal(key, kCONCORDDConfig_AcPowerRestoredCommand)) {
        if (value[0] == 0) {
            gAcPowerRestoredCommand = NULL;
        } else {
            gAcPowerRestoredCommand = strdup(value);
        }
        ret = 0;

	} else if (strcaseequal(key, kCONCORDDConfig_PIDFile)) {
		if (gPIDFilename)
			goto bail;
		gPIDFilename = strdup(value);
		unlink(gPIDFilename);
		FILE* pidfile = fopen(gPIDFilename, "w");
		if (!pidfile) {
			syslog(LOG_ERR, "Unable to open PID file \"%s\".", gPIDFilename);
			goto bail;
		}
		fclose(pidfile);
		ret = 0;
	}

bail:
	if (ret == 0) {
		syslog(LOG_INFO, "set-config-param: \"%s\" set succeded", key);
	}
	return ret;
}

static void
handle_error(int err)
{
	gRet = err;
}

const char*
get_version_string(void)
{
	static char version_string[128] = PACKAGE_VERSION;
	static bool did_calc = false;

	if (!did_calc) {
		did_calc = true;

		if ((internal_build_source_version[0] == 0) || strequal(SOURCE_VERSION, internal_build_source_version)) {
			// Ignore any coverty warnings about the following line being pointless.
			// I assure you, it is not pointless.
			if (strequal(PACKAGE_VERSION, SOURCE_VERSION)) {
				strlcat(version_string, " (", sizeof(version_string));
				strlcat(version_string, internal_build_date, sizeof(version_string));
				strlcat(version_string, ")", sizeof(version_string));
			} else {
				strlcat(version_string, " (", sizeof(version_string));
				strlcat(version_string, SOURCE_VERSION, sizeof(version_string));
				strlcat(version_string, "; ", sizeof(version_string));
				strlcat(version_string, internal_build_date, sizeof(version_string));
				strlcat(version_string, ")", sizeof(version_string));
			}
		} else {
			// Ignore any coverty warnings about the following line being pointless.
			// I assure you, it is not pointless.
			if (strequal(SOURCE_VERSION, PACKAGE_VERSION) || strequal(PACKAGE_VERSION, internal_build_source_version)) {
				strlcat(version_string, " (", sizeof(version_string));
				strlcat(version_string, internal_build_source_version, sizeof(version_string));
				strlcat(version_string, "; ", sizeof(version_string));
				strlcat(version_string, internal_build_date, sizeof(version_string));
				strlcat(version_string, ")", sizeof(version_string));
			} else {
				strlcat(version_string, " (", sizeof(version_string));
				strlcat(version_string, SOURCE_VERSION, sizeof(version_string));
				strlcat(version_string, "/", sizeof(version_string));
				strlcat(version_string, internal_build_source_version, sizeof(version_string));
				strlcat(version_string, "; ", sizeof(version_string));
				strlcat(version_string, internal_build_date, sizeof(version_string));
				strlcat(version_string, ")", sizeof(version_string));
			}
		}
	}
	return version_string;
}

static void
print_version()
{
	printf("concordd %s\n", get_version_string());
}

/* ------------------------------------------------------------------------- */

static fd_set gReadableFDs;
static fd_set gWritableFDs;
static fd_set gErrorableFDs;

/* ------------------------------------------------------------------------- */

struct concordd_state_s {
    struct concordd_instance_s instance;
    int fd;
    struct concordd_dbus_server_s dbus_server;
};

static ge_rs232_status_t
send_bytes_func(struct concordd_state_s* context, const uint8_t* data, int len, struct ge_rs232_s* instance) {
    while (len > 0) {
        ssize_t written = write(context->fd, data, len);
        if (written < 0) {
            syslog(LOG_ERR, "Error on write(): %s (data:%p, len:%d)", strerror(errno),data, len);
            return GE_RS232_STATUS_ERROR;
        }
        len  -= written;
        data += written;
    }

    return GE_RS232_STATUS_OK;
}

void
concordd_instance_info_changed_func(void* context, concordd_instance_t instance, int changed)
{
    struct concordd_state_s *concordd_state = (struct concordd_state_s *)context;

	// Pass-thru to D-Bus first.
	concordd_dbus_system_info_changed_func(&concordd_state->dbus_server, instance, changed);

    if (0 == (changed & CONCORDD_INSTANCE_AC_POWER_FAILURE_CHANGED)) {
		// We only handle AC power failure changes
        return;
    }

	const char* command = NULL;

	if (instance->ac_power_failure) {
		command = gAcPowerFailureCommand;
	} else {
		command = gAcPowerRestoredCommand;
	}

	if (command == NULL) {
		return;
	}

	// Now handle via system.
    int pid = fork();
    if (pid == -1) {
        syslog(LOG_ERR, "concordd_instance_info_changed_func: fork() failed: %s", strerror(errno));

    } else if (pid == 0) {
		// Child
        _exit(system(command));
    }
}

void
concordd_partition_info_changed_func(void* context, concordd_instance_t instance, concordd_partition_t partition, int changed)
{
    struct concordd_state_s *concordd_state = (struct concordd_state_s *)context;

	// Pass-thru to D-Bus first.
	concordd_dbus_partition_info_changed_func(&concordd_state->dbus_server, instance, partition, changed);

	// TODO: Now handle via system
}

void
concordd_zone_info_changed_func(void* context, concordd_instance_t instance, concordd_zone_t zone, int changed)
{
    struct concordd_state_s *concordd_state = (struct concordd_state_s *)context;

	// Pass-thru to D-Bus first.
	concordd_dbus_zone_info_changed_func(&concordd_state->dbus_server, instance, zone, changed);

    if (gZoneChangedCommand == NULL) {
        return;
    }

    if (0 == (changed & CONCORDD_ZONE_TRIPPED_CHANGED|CONCORDD_ZONE_ALARM_CHANGED|CONCORDD_ZONE_TROUBLE_CHANGED|CONCORDD_ZONE_FAULT_CHANGED)) {
        // We only care about when the zone state has changed.
        return;
    }

    // Now handle via system.
    int pid = fork();
    if (pid == -1) {
        syslog(LOG_ERR, "concordd_zone_info_changed_func: fork() failed: %s", strerror(errno));

    } else if (pid == 0) {
        char value[64];

        setenv("CONCORDD_TYPE", "ZONE", 1);

        snprintf(value, sizeof(value), "%d", zone->partition_id);
        setenv("CONCORDD_PARTITION_ID", value, 1);

        snprintf(value, sizeof(value), "%d", concordd_get_zone_index(instance, zone));
        setenv("CONCORDD_ZONE_ID", value, 1);

		setenv("CONCORDD_ZONE_NAME", ge_text_to_ascii_one_line(zone->encoded_name, zone->encoded_name_len), 1);

		snprintf(value, sizeof(value), "%d", zone->type);
        setenv("CONCORDD_ZONE_TYPE", value, 1);

		snprintf(value, sizeof(value), "%d", zone->group);
        setenv("CONCORDD_ZONE_GROUP", value, 1);


		if ((zone->zone_state&GE_RS232_ZONE_STATUS_TRIPPED) == GE_RS232_ZONE_STATUS_TRIPPED) {
	        setenv("CONCORDD_ZONE_TRIPPED", "1", 1);
		}

		if ((zone->zone_state&GE_RS232_ZONE_STATUS_ALARM) == GE_RS232_ZONE_STATUS_ALARM) {
	        setenv("CONCORDD_ZONE_ALARM", "1", 1);
		}

		if ((zone->zone_state&GE_RS232_ZONE_STATUS_FAULT) == GE_RS232_ZONE_STATUS_FAULT) {
	        setenv("CONCORDD_ZONE_FAULT", "1", 1);
		}

		if ((zone->zone_state&GE_RS232_ZONE_STATUS_TROUBLE) == GE_RS232_ZONE_STATUS_TROUBLE) {
	        setenv("CONCORDD_ZONE_TROUBLE", "1", 1);
		}

		if ((zone->zone_state&GE_RS232_ZONE_STATUS_BYPASSED) == GE_RS232_ZONE_STATUS_BYPASSED) {
	        setenv("CONCORDD_ZONE_BYPASSED", "1", 1);
		}

        _exit(system(gZoneChangedCommand));
    }
}

void
concordd_event_func(void* context, concordd_instance_t instance, concordd_event_t event)
{
    struct concordd_state_s *concordd_state = (struct concordd_state_s *)context;

    // Pass-thru to D-Bus first.
    concordd_dbus_event_func(&concordd_state->dbus_server, instance, event);

    // Now handle via system.
    int pid = fork();
    if (pid == -1) {
        syslog(LOG_ERR, "concordd_event_func: fork() failed: %s", strerror(errno));
    } else if (pid == 0) {
        // Child
        char value[64];
        const char* command = NULL;
        const char* specific_desc = NULL;

        switch(event->general_type) {
        case GE_RS232_ALARM_GENERAL_TYPE_SYSTEM_TROUBLE:
        case GE_RS232_ALARM_GENERAL_TYPE_SYSTEM_TROUBLE_RESTORAL:
            snprintf(value, sizeof(value), "SYSTEM-TROUBLE");
            command = gSystemTroubleCommand;
            specific_desc = ge_specific_system_trouble_to_cstr(NULL, event->specific_type);
            break;

        case GE_RS232_ALARM_GENERAL_TYPE_ALARM:
        case GE_RS232_ALARM_GENERAL_TYPE_ALARM_RESTORAL:
        case GE_RS232_ALARM_GENERAL_TYPE_ALARM_CANCEL:
            command = gPartitionAlarmCommand;
            snprintf(value, sizeof(value), "PARTITION-ALARM");
            specific_desc = ge_specific_alarm_to_cstr(NULL, event->specific_type);
            break;

        case GE_RS232_ALARM_GENERAL_TYPE_FIRE_TROUBLE:
        case GE_RS232_ALARM_GENERAL_TYPE_NONFIRE_TROUBLE:
        case GE_RS232_ALARM_GENERAL_TYPE_FIRE_TROUBLE_RESTORAL:
        case GE_RS232_ALARM_GENERAL_TYPE_NONFIRE_TROUBLE_RESTORAL:
            command = gPartitionTroubleCommand;
            snprintf(value, sizeof(value), "PARTITION-TROUBLE");
            specific_desc = ge_specific_trouble_to_cstr(NULL, event->specific_type);
            break;

        case GE_RS232_ALARM_GENERAL_TYPE_PARTITION_EVENT:
            command = gPartitionEventCommand;
            snprintf(value, sizeof(value), "PARTITION-EVENT");
            specific_desc = ge_specific_partition_to_cstr(NULL, event->specific_type);
            break;

        case GE_RS232_ALARM_GENERAL_TYPE_SYSTEM_EVENT:
            command = gSystemEventCommand;
            snprintf(value, sizeof(value), "SYSTEM-EVENT");
            break;
        }

        if (command) {
            setenv("CONCORDD_TYPE", value, 1);

            switch (event->status) {
            case CONCORDD_EVENT_STATUS_ONGOING:
                snprintf(value, sizeof(value), "%s", "ONGOING");
                break;
            case CONCORDD_EVENT_STATUS_CANCELED:
                snprintf(value, sizeof(value), "%s", "CANCELED");
                break;
            case CONCORDD_EVENT_STATUS_RESTORED:
                snprintf(value, sizeof(value), "%s", "RESTORED");
                break;
            default:
            case CONCORDD_EVENT_STATUS_UNSPECIFIED:
                snprintf(value, sizeof(value), "%s", "TRIGGERED");
                break;
            }
            setenv("CONCORDD_EVENT_STATUS", value, 1);

            snprintf(value, sizeof(value), "%d", event->general_type);
            setenv("CONCORDD_EVENT_GENERAL_TYPE", value, 1);

            snprintf(value, sizeof(value), "%d", event->specific_type);
            setenv("CONCORDD_EVENT_SPECIFIC_TYPE", value, 1);

            snprintf(value, sizeof(value), "%d", event->extra_data);
            setenv("CONCORDD_EVENT_EXTRA_DATA", value, 1);

            snprintf(value, sizeof(value), "%d", event->partition_id);
            setenv("CONCORDD_PARTITION_ID", value, 1);

            if (event->zone_id != 0xFFFF) {
                concordd_zone_t zone = concordd_get_zone(instance, event->zone_id);
                snprintf(value, sizeof(value), "%d", event->zone_id);
                setenv("CONCORDD_ZONE_ID", value, 1);
                setenv("CONCORDD_EVENT_SOURCE_ID", value, 1);

                if (zone) {
                    const char* zone_name = ge_text_to_ascii_one_line(zone->encoded_name, zone->encoded_name_len);
                    setenv("CONCORDD_ZONE_NAME", zone_name, 1);
                    snprintf(value, sizeof(value), "%s %s [%s] (%d.%d) ZONE %d: %s",
                        getenv("CONCORDD_EVENT_STATUS"),
                        getenv("CONCORDD_TYPE"),
                        specific_desc,
                        event->general_type,
                        event->specific_type,
                        event->zone_id,
                        zone_name
                    );
                    setenv("CONCORDD_EVENT_DESC", value, 1);
                }
            } else {
                snprintf(value, sizeof(value), "%d", event->device_id);
                setenv("CONCORDD_UNIT_ID", value, 1);
                setenv("CONCORDD_EVENT_SOURCE_ID", value, 1);

                    snprintf(value, sizeof(value), "%s %s [%s] (%d.%d) UNIT %d",
                        getenv("CONCORDD_EVENT_STATUS"),
                        getenv("CONCORDD_TYPE"),
                        specific_desc,
                        event->general_type,
                        event->specific_type,
                        event->device_id
                    );
                    setenv("CONCORDD_EVENT_DESC", value, 1);
            }
        }

        _exit(system(command));
    }
}

void
concordd_light_info_changed_func(void* context,  concordd_instance_t instance, concordd_partition_t partition, concordd_light_t light, int changed)
{
    struct concordd_state_s *concordd_state = (struct concordd_state_s *)context;

    // Pass-thru to D-Bus first.
    concordd_dbus_light_info_changed_func(&concordd_state->dbus_server, instance, partition, light, changed);

    if (gLightChangedCommand == NULL) {
        return;
    }

    if (0 == (changed & CONCORDD_LIGHT_LIGHT_STATE_CHANGED)) {
        // We only care about when the light state has changed.
        return;
    }

    // Now handle via system.
    int pid = fork();
    if (pid == -1) {
        syslog(LOG_ERR, "concordd_light_info_changed_func: fork() failed: %s", strerror(errno));

    } else if (pid == 0) {
        char value[64];

        setenv("CONCORDD_TYPE", "OUTPUT", 1);

        snprintf(value, sizeof(value), "%d", concordd_get_partition_index(instance, partition));
        setenv("CONCORDD_PARTITION_ID", value, 1);

        snprintf(value, sizeof(value), "%d", concordd_get_light_index(instance, partition, light));
        setenv("CONCORDD_LIGHT_ID", value, 1);

        snprintf(value, sizeof(value), "%d", light->light_state);
        setenv("CONCORDD_LIGHT_STATE", value, 1);

        if (0 == (changed & CONCORDD_LIGHT_LAST_CHANGED_AT_CHANGED)) {
            snprintf(value, sizeof(value), "%ld", light->last_changed_at);
            setenv("CONCORDD_LAST_CHANGED_AT", value, 1);
        }

        if (0 == (changed & CONCORDD_LIGHT_LAST_CHANGED_BY_CHANGED)) {
            snprintf(value, sizeof(value), "%d", light->last_changed_by);
            setenv("CONCORDD_LAST_CHANGED_BY", value, 1);
        }

        _exit(system(gLightChangedCommand));
    }
}

void
concordd_output_info_changed_func(void* context, concordd_instance_t instance, concordd_output_t output, int changed)
{
    struct concordd_state_s *concordd_state = (struct concordd_state_s *)context;

    // Pass-thru to D-Bus first.
    concordd_dbus_output_info_changed_func(&concordd_state->dbus_server, instance, output, changed);

    if (gOutputChangedCommand == NULL) {
        return;
    }

    if (0 == (changed & CONCORDD_OUTPUT_OUTPUT_STATE_CHANGED)) {
        // We only care about when the output state has changed.
        return;
    }

    // Now handle via system.
    int pid = fork();
    if (pid == -1) {
        syslog(LOG_ERR, "concordd_output_info_changed_func: fork() failed: %s", strerror(errno));

    } else if (pid == 0) {
        char value[64];

        setenv("CONCORDD_TYPE", "OUTPUT", 1);

        snprintf(value, sizeof(value), "%d", concordd_get_output_index(instance, output));
        setenv("CONCORDD_OUTPUT_ID", value, 1);

        snprintf(value, sizeof(value), "%d", output->output_state);
        setenv("CONCORDD_OUTPUT_STATE", value, 1);

        if (0 == (changed & CONCORDD_OUTPUT_LAST_CHANGED_AT_CHANGED)) {
            snprintf(value, sizeof(value), "%ld", output->last_changed_at);
            setenv("CONCORDD_LAST_CHANGED_AT", value, 1);
        }

        if (0 == (changed & CONCORDD_OUTPUT_LAST_CHANGED_BY_CHANGED)) {
            snprintf(value, sizeof(value), "%d", output->last_changed_by);
            setenv("CONCORDD_LAST_CHANGED_BY", value, 1);
        }

        _exit(system(gOutputChangedCommand));
    }
}

void
concordd_siren_sync_func(void* context,  concordd_instance_t instance)
{
    struct concordd_state_s *concordd_state = (struct concordd_state_s *)context;

    // Pass-thru to D-Bus first.
    concordd_dbus_siren_sync_func(&concordd_state->dbus_server, instance);
}

/* ------------------------------------------------------------------------- */
/* MARK: Main Function */

int
main(int argc, char * argv[])
{
	int c;
	int fds_ready = 0;
	bool interface_added = false;
	int zero_cms_in_a_row_count = 0;
	const char* config_file = SYSCONFDIR "/concordd.conf";
	static struct option long_options[] =
	{
		{"help",	no_argument,		0,	'h'},
		{"version",	no_argument,		0,	'v'},
		{"debug",	no_argument,		0,	'd'},
		{"config",	required_argument,	0,	'c'},
		{"option",	required_argument,	0,	'o'},
		{"socket",	required_argument,	0,	's'},
		{"baudrate",	required_argument,	0,	'b'},
		{"user",	required_argument,	0,	'u'},
		{0,		0,			0,	0}
	};

	struct concordd_state_s concordd_state;

	// ========================================================================
	// INITIALIZATION and ARGUMENT PARSING

	gSocketWrapperBaud = 9600;

	gPreviousHandlerForSIGINT = signal(SIGINT, &signal_SIGINT);
	gPreviousHandlerForSIGTERM = signal(SIGTERM, &signal_SIGTERM);
	signal(SIGHUP, &signal_SIGHUP);

	// Automatically clean up child processes.
	signal(SIGCHLD, &signal_SIGCHLD);

	// Always ignore SIGPIPE.
	signal(SIGPIPE, SIG_IGN);

    install_crash_trace_handler();

	openlog(basename(argv[0]), LOG_PERROR | LOG_PID /*| LOG_CONS*/, LOG_DAEMON);

	// Temper the amount of logging.
	setlogmask(setlogmask(0) & LOG_UPTO(DEFAULT_MAX_LOG_LEVEL));

	gRet = ERRORCODE_UNKNOWN;

	if (argc && argv[0][0]) {
		gProcessName = basename(argv[0]);
	}

	optind = 0;
	while(1) {
		int option_index = 0;
		c = getopt_long(argc, argv, "hvd:c:o:I:s:b:u:", long_options,
			&option_index);

		if (c == -1)
			break;

		switch(c) {
		case 'h':
			print_arg_list_help(option_list, argv[0], "[options]");
			gRet = ERRORCODE_HELP;
			goto bail;

		case 'v':
			print_version();
			gRet = 0;
			goto bail;

		case 'd':
			setlogmask(~0);
			break;

		case 'c':
			config_file = optarg;
			break;

		// Skip option-related arguments
		case 'o':
			optind++;
		case 's':
		case 'b':
		case 'u':
			break;

		}
	}

	if (optind < argc) {
		fprintf(stderr,
		        "%s: error: Unexpected extra argument: \"%s\"\n",
		        argv[0],
		        argv[optind]);
		gRet = ERRORCODE_BADARG;
		goto bail;
	}

    // Read the configuration file into the settings map.
    if (0 == read_config(config_file, &set_config_param, NULL)) {
        syslog(LOG_NOTICE, "Configuration file \"%s\" read.", config_file);
    } else {
        syslog(LOG_WARNING, "Configuration file \"%s\" not found, will use defaults.", config_file);
    }

	// Read in the options from the command line
	optind = 0;
	while(1) {
		int option_index = 0;
		c = getopt_long(argc, argv, "hvd:c:o:I:s:b:u:", long_options,
			&option_index);

		if (c == -1)
			break;

		switch(c) {
		case 'd':
			setlogmask(~0);
			break;

		case 's':
			set_config_param(NULL, kCONCORDDConfig_SocketPath, optarg);
			break;

		case 'b':
			set_config_param(NULL, kCONCORDDConfig_SocketBaud, optarg);
			break;

		case 'u':
			set_config_param(NULL, kCONCORDDConfig_PrivDropToUser, optarg);
			break;

		case 'o':
			if ((optind >= argc) || (strncmp(argv[optind], "-", 1) == 0)) {
				syslog(LOG_ERR, "Missing argument to '-o'.");
				gRet = ERRORCODE_BADARG;
				goto bail;
			}
			char *key = optarg;
			char *value = argv[optind];
			optind++;

			set_config_param(NULL, key, value);

			break;
		}
	}


	// ========================================================================
	// STARTUP

	syslog(LOG_NOTICE, "Starting %s " PACKAGE_VERSION " (%s) . . .", gProcessName, internal_build_date);

	if (("" SOURCE_VERSION)[0] != 0) {
		syslog(LOG_NOTICE, "\tSOURCE_VERSION = " SOURCE_VERSION);
	}

	if (internal_build_source_version[0] != 0) {
		syslog(LOG_NOTICE, "\tBUILD_VERSION = %s", internal_build_source_version);
	}

	// ========================================================================
	// Dropping Privileges

	if (gChroot != NULL) {
		if (getuid() == 0) {
			if (chdir(gChroot) != 0) {
				syslog(LOG_CRIT, "chdir: %s", strerror(errno));
				gRet = ERRORCODE_ERRNO;
				goto bail;
			}

			if (chroot(gChroot) != 0) {
				syslog(LOG_CRIT, "chroot: %s", strerror(errno));
				gRet = ERRORCODE_ERRNO;
				goto bail;
			} else {
				if (chdir("/") != 0) {
					syslog(LOG_INFO, "Failed to `chdir` after `chroot` to \"%s\"", gChroot);
					gRet = ERRORCODE_ERRNO;
					goto bail;
				} else {
					syslog(LOG_INFO, "Successfully changed root directory to \"%s\".", gChroot);
				}
			}
		} else {
			syslog(LOG_WARNING, "Not running as root, cannot chroot");
		}
	}

#if HAVE_PWD_H
	if (getuid() == 0) {
		uid_t target_uid = 0;
		gid_t target_gid = 0;

		if (gPrivDropToUser != NULL) {
			struct passwd *passwd = getpwnam(gPrivDropToUser);

			if (passwd == NULL) {
				syslog(LOG_CRIT, "getpwnam: Unable to lookup user \"%s\", cannot drop privileges.", gPrivDropToUser);
				gRet = ERRORCODE_ERRNO;
				goto bail;
			}

			target_uid = passwd->pw_uid;
			target_gid = passwd->pw_gid;
		}

		if (target_gid != 0) {
			if (setgid(target_gid) != 0) {
				syslog(LOG_CRIT, "setgid: Unable to drop group privileges: %s", strerror(errno));
				gRet = ERRORCODE_ERRNO;
				goto bail;
			} else {
				syslog(LOG_INFO, "Group privileges dropped to GID:%d", (int)target_gid);
			}
		}

		if (target_uid != 0) {
			if (setuid(target_uid) != 0) {
				syslog(LOG_CRIT, "setuid: Unable to drop user privileges: %s", strerror(errno));
				gRet = ERRORCODE_ERRNO;
				goto bail;
			} else {
				syslog(LOG_INFO, "User privileges dropped to UID:%d", (int)target_uid);
			}
		}

		if ((target_gid == 0) || (target_uid == 0)) {
			syslog(LOG_NOTICE, "Running as root without dropping privileges!");
		}
	} else if (gPrivDropToUser != NULL) {
		syslog(LOG_NOTICE, "Not running as root, skipping dropping privileges");
	}
#endif // #if HAVE_PWD_H

    // ========================================================================
    // Set up state

    concordd_init(&concordd_state.instance);
    concordd_state.instance.send_bytes_func = (ge_rs232_send_bytes_func_t)&send_bytes_func;
    concordd_state.instance.context = (void*)&concordd_state;
    concordd_state.fd = open_super_socket(gSocketPath);

    if (concordd_state.fd < 0) {
        syslog(LOG_ERR, "Failed to open \"%s\": %s", gSocketPath, strerror(errno));
        goto bail;
    }

    if (concordd_dbus_server_init(&concordd_state.dbus_server, &concordd_state.instance)==NULL) {
        syslog(LOG_ERR, "Failed to start DBus server");
        goto bail;
    }

	concordd_refresh(&concordd_state.instance, NULL, NULL);

    concordd_state.instance.event_func = &concordd_event_func;
    concordd_state.instance.light_info_changed_func = &concordd_light_info_changed_func;
    concordd_state.instance.output_info_changed_func = &concordd_output_info_changed_func;
	concordd_state.instance.zone_info_changed_func = &concordd_zone_info_changed_func;
	concordd_state.instance.instance_info_changed_func = &concordd_instance_info_changed_func;
	concordd_state.instance.partition_info_changed_func = &concordd_partition_info_changed_func;
	concordd_state.instance.siren_sync_func = &concordd_siren_sync_func;

	// ========================================================================
	// MAIN LOOP

	gRet = 0;

	while (!gRet) {
        ge_rs232_status_t ge_rs232_status;
		const cms_t max_main_loop_timeout = 5*MSEC_PER_SEC;//CMS_DISTANT_FUTURE;
		cms_t cms_timeout = max_main_loop_timeout;
		int max_fd = -1;
		struct timeval timeout;

		FD_ZERO(&gReadableFDs);
		FD_ZERO(&gWritableFDs);
		FD_ZERO(&gErrorableFDs);

		// Update the FD masks and timeouts
        if (ge_queue_is_empty(&concordd_state.instance.ge_queue) || concordd_state.instance.ge_rs232.reading_message == true) {
            FD_SET(concordd_state.fd, &gReadableFDs);
            max_fd = concordd_state.fd;
            //FD_SET(concordd_state.fd, &gErrorableFDs);
        } else if (ge_rs232_ready_to_send(&concordd_state.instance.ge_rs232) != GE_RS232_STATUS_WAIT) {
            FD_SET(concordd_state.fd, &gWritableFDs);
            FD_SET(concordd_state.fd, &gReadableFDs);
            max_fd = concordd_state.fd;
            //FD_SET(concordd_state.fd, &gErrorableFDs);
            syslog(LOG_DEBUG, "Waiting for writable FD");
        } else {
            FD_SET(concordd_state.fd, &gReadableFDs);
            max_fd = concordd_state.fd;
        }

        cms_timeout = concordd_get_timeout_cms(&concordd_state.instance);

        concordd_dbus_server_update_fd_set(
            &concordd_state.dbus_server,
            &gReadableFDs,
            &gWritableFDs,
            &gErrorableFDs,
            &max_fd,
            &cms_timeout
        );

		require_string(max_fd < FD_SETSIZE, bail, "Too many file descriptors");

		// Negative CMS timeout values are not valid.
		if (cms_timeout < 0) {
			syslog(LOG_DEBUG, "Negative CMS value: %d", cms_timeout);
			cms_timeout = 0;
		}

		// Identify conditions where we are burning too much CPU.
		if (cms_timeout == 0) {
			double loadavg[3] = {-1.0, -1.0, -1.0};

#if HAVE_GETLOADAVG
			getloadavg(loadavg, 3);
#endif

			switch (++zero_cms_in_a_row_count) {
			case 20:
				syslog(LOG_INFO, "BUG: Main loop is thrashing! (%f %f %f)", loadavg[0], loadavg[1], loadavg[2]);
				break;

			case 200:
				syslog(LOG_WARNING, "BUG: Main loop is still thrashing! Slowing things down. (%f %f %f)", loadavg[0], loadavg[1], loadavg[2]);
				break;

			case 1000:
				syslog(LOG_CRIT, "BUG: Main loop had over 1000 iterations in a row with a zero timeout! Terminating. (%f %f %f)", loadavg[0], loadavg[1], loadavg[2]);
				gRet = ERRORCODE_UNKNOWN;
				break;
			}
			if (zero_cms_in_a_row_count > 200) {
				// If the past 200 iterations have had a zero timeout,
				// start using a minimum timeout of 10ms, so that we
				// don't bring the rest of the system to a grinding halt.
				cms_timeout = 10;
			}
		} else {
			zero_cms_in_a_row_count = 0;
		}

		// Convert our `cms` value into timeval compatible with select().
		timeout.tv_sec = cms_timeout / MSEC_PER_SEC;
		timeout.tv_usec = (cms_timeout % MSEC_PER_SEC) * USEC_PER_MSEC;

		// Block until we timeout or there is FD activity.
		fds_ready = select(
			max_fd + 1,
			&gReadableFDs,
			&gWritableFDs,
			&gErrorableFDs,
			&timeout
		);

		if (fds_ready < 0) {
			if (errno == EINTR) {
				// EINTR isn't necessarily bad. If it was something bad,
				// we would either already be terminated or gRet will be
				// set and we will break out of the main loop in a moment.
				continue;
			}
			syslog(LOG_ERR, "select() errno=\"%s\" (%d)", strerror(errno),
			       errno);

			gRet = ERRORCODE_ERRNO;
			break;
		}

        if (FD_ISSET(concordd_state.fd, &gErrorableFDs)) {
            syslog(LOG_ERR, "FD Error");
            goto bail;
        }

        if (FD_ISSET(concordd_state.fd, &gReadableFDs)) {
            uint8_t buffer[100];
            ssize_t ret = read(concordd_state.fd, buffer, sizeof(buffer));
            if (ret > 0) {
                ssize_t i;
                for (i = 0; i<ret; ++i) {
                    uint8_t byte = buffer[i];
                    if (!byte) {
                        continue;
                    }
                    ge_rs232_receive_byte(&concordd_state.instance.ge_rs232, byte);
                }
            } else if (ret == -1) {
                syslog(LOG_ERR, "read() errno=\"%s\" (%d)", strerror(errno),
                   errno);
                goto bail;
            }
        }

        concordd_dbus_server_process(&concordd_state.dbus_server);

        ge_rs232_status = concordd_process(&concordd_state.instance);

        if (ge_rs232_status != GE_RS232_STATUS_OK) {
            syslog(LOG_ERR, "concordd_process() failed: %d", ge_rs232_status);
            goto bail;
        }

	} // while (!gRet)

bail:
	syslog(LOG_NOTICE, "Cleaning up. (gRet = %d)", gRet);

	if (gRet == ERRORCODE_QUIT) {
		gRet = 0;
	}

	if (gPIDFilename) {
		unlink(gPIDFilename);
	}

	syslog(LOG_NOTICE, "Stopped.");
	return gRet;
}
