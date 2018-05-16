
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
#include <syslog.h>
#include <stdlib.h>
#include "string-utils.h"

#include "concordd-dbus-server.h"

static void
append_dict_entry(
    DBusMessageIter *dict, const char *key, char type, void *val
    )
{
    DBusMessageIter entry;
    DBusMessageIter value_iter;
    char sig[2] = { type, '\0' };

    if (type == DBUS_TYPE_STRING) {
        const char *str = *((const char**)val);
        if (str == NULL)
            return;
    }

    dbus_message_iter_open_container(dict, DBUS_TYPE_DICT_ENTRY, NULL, &entry);

    dbus_message_iter_append_basic(&entry, DBUS_TYPE_STRING, &key);

    dbus_message_iter_open_container(&entry, DBUS_TYPE_VARIANT, sig,
                                     &value_iter);

    dbus_message_iter_append_basic(&value_iter, type, val);

    dbus_message_iter_close_container(&entry, &value_iter);

    dbus_message_iter_close_container(dict, &entry);
}

static bool
concordd_dbus_path_is_system(const char* path)
{
    return strequal(path, CONCORDD_DBUS_PATH_ROOT);
}

static bool
concordd_dbus_path_is_zone(const char* path)
{
    return strhasprefix(path, CONCORDD_DBUS_PATH_ZONE);
}

static bool
concordd_dbus_path_is_partition(const char* path)
{
    return strhasprefix(path, CONCORDD_DBUS_PATH_PARTITION);
}

static bool
concordd_dbus_path_is_output(const char* path)
{
    return strhasprefix(path, CONCORDD_DBUS_PATH_OUTPUT);
}

static bool
concordd_dbus_path_is_light(const char* path)
{
	static const char* light_dir = "/light/";
	if (concordd_dbus_path_is_partition(path)) {
		return strstr(path, light_dir) != NULL;
	}
	return false;
}

static int
concordd_zone_index_from_dbus_path(const char* path, concordd_instance_t instance)
{
    if (concordd_dbus_path_is_zone(path)) {
        return strtol(path + strlen(CONCORDD_DBUS_PATH_ZONE), NULL, 10);
    }
    return -1;
}

static concordd_zone_t
concordd_zone_from_dbus_path(const char* path, concordd_instance_t instance)
{
    int i = concordd_zone_index_from_dbus_path(path, instance);
    if (i >= 0) {
        return concordd_get_zone(instance, i);
    }

    return NULL;
}

static int
concordd_output_index_from_dbus_path(const char* path, concordd_instance_t instance)
{
    if (concordd_dbus_path_is_output(path)) {
        return strtol(path + strlen(CONCORDD_DBUS_PATH_OUTPUT), NULL, 10);
    }
    return -1;
}

static concordd_output_t
concordd_output_from_dbus_path(const char* path, concordd_instance_t instance)
{
    int i = concordd_output_index_from_dbus_path(path, instance);
    if (i >= 0) {
        return concordd_get_output(instance, i);
    }

    return NULL;
}

static int
concordd_partition_index_from_dbus_path(const char* path, concordd_instance_t instance)
{
    if (concordd_dbus_path_is_partition(path)) {
        return strtol(path + strlen(CONCORDD_DBUS_PATH_PARTITION), NULL, 10);
    }
    return -1;
}

static concordd_partition_t
concordd_partition_from_dbus_path(const char* path, concordd_instance_t instance)
{
    int i = concordd_partition_index_from_dbus_path(path, instance);
    if (i >= 0) {
        return concordd_get_partition(instance, i);
    }

    return NULL;
}

static int
concordd_light_index_from_dbus_path(const char* path, concordd_instance_t instance)
{
	static const char* light_dir = "/light/";
    if (concordd_dbus_path_is_light(path)) {
		path = strstr(path, light_dir);
		if (path != NULL) {
			return strtol(path + strlen(light_dir), NULL, 10);
		}
	}
    return -1;
}

static concordd_light_t
concordd_light_from_dbus_path(const char* path, concordd_instance_t instance)
{
    int p = concordd_partition_index_from_dbus_path(path, instance);
    int l = concordd_light_index_from_dbus_path(path, instance);
    if ((p >= 0) && (l >= 0)) {
        concordd_partition_t partition = concordd_get_partition(instance, p);
		if (partition != NULL) {
			return concordd_partition_get_light(partition, l);
		}
    }

    return NULL;
}


static DBusHandlerResult
concordd_dbus_handle_system_get_partitions(
    concordd_dbus_server_t self,
    DBusConnection *connection,
    DBusMessage *   message
) {
    DBusMessage *reply = dbus_message_new_method_return(message);
    DBusMessageIter iter, array_iter;
    dbus_message_iter_init_append(reply, &iter);
    char path_buffer[128];
    int i;

    dbus_message_iter_open_container(
        &iter,
        DBUS_TYPE_ARRAY,
        DBUS_TYPE_STRING_AS_STRING,
        &array_iter
        );

    for (i = 0; i < CONCORDD_MAX_PARTITIONS; i++) {
        char* partition_path = path_buffer;
        concordd_partition_t partition = concordd_get_partition(self->instance, i);
        if (partition == NULL || partition->active == false) {
            continue;
        }
        snprintf(partition_path, sizeof(path_buffer), "%s%d", CONCORDD_DBUS_PATH_PARTITION, i);
        dbus_message_iter_append_basic(&array_iter, DBUS_TYPE_STRING, &partition_path);
    }

    dbus_message_iter_close_container(&iter, &array_iter);

    dbus_connection_send(connection, reply, NULL);

    dbus_message_unref(reply);

    return DBUS_HANDLER_RESULT_HANDLED;
}

struct concordd_dbus_callback_helper_s {
    concordd_dbus_server_t self;
    DBusMessage *message;
};

struct concordd_dbus_callback_helper_s*
concordd_dbus_callback_helper_new(concordd_dbus_server_t self, DBusMessage *message)
{
    struct concordd_dbus_callback_helper_s* ret = malloc(sizeof(struct concordd_dbus_callback_helper_s));
    if (ret) {
        ret->self = self;
        ret->message = message;
        dbus_message_ref(message);
    }
    return ret;
}

void
concordd_dbus_callback_helper_free(struct concordd_dbus_callback_helper_s* helper)
{
    if (helper) {
        dbus_message_unref(helper->message);
        free(helper);
    }
}

void
concordd_dbus_callback_helper(void* context, ge_rs232_status_t status)
{
    struct concordd_dbus_callback_helper_s* helper = (struct concordd_dbus_callback_helper_s*)context;
    concordd_dbus_server_t self = helper->self;

    DBusMessage *reply = dbus_message_new_method_return(helper->message);

    syslog(LOG_DEBUG, "Sending DBus response for \"%s\" to \"%s\"", dbus_message_get_member(helper->message), dbus_message_get_sender(helper->message));

    if (reply) {
        dbus_message_append_args(
            reply,
            DBUS_TYPE_INT32, &status,
            DBUS_TYPE_INVALID
        );

        dbus_connection_send(self->dbus_connection, reply, NULL);
        dbus_message_unref(reply);
    }

bail:
    concordd_dbus_callback_helper_free(helper);
}

static DBusHandlerResult
concordd_dbus_handle_partition_set_arm_level(
    concordd_dbus_server_t self,
    DBusConnection *connection,
    DBusMessage *   message
) {
    const char* path = dbus_message_get_path(message);
    int partition_index = concordd_partition_index_from_dbus_path(path, self->instance);
    struct concordd_dbus_callback_helper_s* helper;
    ge_rs232_status_t status;
    int32_t arm_level = -1;

    if (partition_index < 0) {
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    }

    dbus_message_get_args(
        message, NULL,
        DBUS_TYPE_INT32, &arm_level,
        DBUS_TYPE_INVALID
    );

    helper = concordd_dbus_callback_helper_new(self, message);

    status = concordd_set_arm_level(
        self->instance,
        partition_index,
        arm_level,
        &concordd_dbus_callback_helper,
        (void*)helper);

    if (status < 0) {
        if (status == GE_RS232_STATUS_ALREADY) {
            status = GE_RS232_STATUS_OK;
        }
        concordd_dbus_callback_helper((void*)helper, status);
    }

    return DBUS_HANDLER_RESULT_HANDLED;
}

static DBusHandlerResult
concordd_dbus_handle_light_set_value(
    concordd_dbus_server_t self,
    DBusConnection *connection,
    DBusMessage *   message
) {
    DBusHandlerResult ret = DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    const char* path = dbus_message_get_path(message);
    const int partition_index = concordd_partition_index_from_dbus_path(path, self->instance);
    const int light_index = concordd_light_index_from_dbus_path(path, self->instance);
	concordd_light_t light = concordd_light_from_dbus_path(path, self->instance);
    struct concordd_dbus_callback_helper_s* helper;
    bool state = false;
    ge_rs232_status_t status;

    if (light_index < 0 || light == NULL) {
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    }

    dbus_message_get_args(
        message, NULL,
        DBUS_TYPE_BOOLEAN, &state,
        DBUS_TYPE_INVALID
    );

    helper = concordd_dbus_callback_helper_new(self, message);

    status = concordd_set_light(
        self->instance,
        partition_index,
        light_index,
        state,
        &concordd_dbus_callback_helper,
        (void*)helper);

    if (status < 0) {
        if (status == GE_RS232_STATUS_ALREADY) {
            status = GE_RS232_STATUS_OK;
        }
        concordd_dbus_callback_helper((void*)helper, status);
    }

    return DBUS_HANDLER_RESULT_HANDLED;
}

static DBusHandlerResult
concordd_dbus_handle_output_set_value(
    concordd_dbus_server_t self,
    DBusConnection *connection,
    DBusMessage *   message
) {
    DBusHandlerResult ret = DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    const char* path = dbus_message_get_path(message);
    const int partition_index = concordd_partition_index_from_dbus_path(path, self->instance);
    const int output_index = concordd_output_index_from_dbus_path(path, self->instance);
	concordd_output_t output = concordd_output_from_dbus_path(path, self->instance);
    struct concordd_dbus_callback_helper_s* helper;
    bool state = false;
    ge_rs232_status_t status;

    if (output_index < 0 || output == NULL) {
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    }

    dbus_message_get_args(
        message, NULL,
        DBUS_TYPE_BOOLEAN, &state,
        DBUS_TYPE_INVALID
    );

    helper = concordd_dbus_callback_helper_new(self, message);

    status = concordd_set_output(
        self->instance,
        output_index,
        state,
        &concordd_dbus_callback_helper,
        (void*)helper);

    if (status < 0) {
        if (status == GE_RS232_STATUS_ALREADY) {
            status = GE_RS232_STATUS_OK;
        }
        concordd_dbus_callback_helper((void*)helper, status);
    }

    return DBUS_HANDLER_RESULT_HANDLED;
}

static DBusHandlerResult
concordd_dbus_handle_partition_press_keys(
    concordd_dbus_server_t self,
    DBusConnection *connection,
    DBusMessage *   message
) {
    const char* path = dbus_message_get_path(message);
    int partition_index = concordd_partition_index_from_dbus_path(path, self->instance);
    struct concordd_dbus_callback_helper_s* helper;
    ge_rs232_status_t status;
    const char* keys = NULL;

    if (partition_index < 0) {
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    }

    dbus_message_get_args(
        message, NULL,
        DBUS_TYPE_STRING, &keys,
        DBUS_TYPE_INVALID
    );

    helper = concordd_dbus_callback_helper_new(self, message);

    status = concordd_press_keys(
        self->instance,
        partition_index,
        keys,
        &concordd_dbus_callback_helper,
        (void*)helper);

    if (status < 0) {
        concordd_dbus_callback_helper((void*)helper, status);
    }

    return DBUS_HANDLER_RESULT_HANDLED;
}

static DBusHandlerResult
concordd_dbus_handle_refresh(
    concordd_dbus_server_t self,
    DBusConnection *connection,
    DBusMessage *   message
) {
    const char* path = dbus_message_get_path(message);
    struct concordd_dbus_callback_helper_s* helper;
    ge_rs232_status_t status;

    helper = concordd_dbus_callback_helper_new(self, message);

    status = concordd_refresh(
        self->instance,
        &concordd_dbus_callback_helper,
        (void*)helper);

    if (status < 0) {
        concordd_dbus_callback_helper((void*)helper, status);
    }

    return DBUS_HANDLER_RESULT_HANDLED;
}


static DBusHandlerResult
concordd_dbus_handle_partition_get_info(
    concordd_dbus_server_t self,
    DBusConnection *connection,
    DBusMessage *   message
) {
    DBusHandlerResult ret = DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    const char* path = dbus_message_get_path(message);
    int partition_index = concordd_partition_index_from_dbus_path(path, self->instance);
    concordd_partition_t partition = concordd_get_partition(self->instance, partition_index);
    DBusMessage *reply = dbus_message_new_method_return(message);
    ge_rs232_status_t status;

    if (partition_index < 0 || partition == NULL) {
        goto bail;
    }

    syslog(LOG_DEBUG, "Sending DBus response for \"%s\" to \"%s\"", dbus_message_get_member(message), dbus_message_get_sender(message));

    if (!reply) {
        goto bail;
    }

    DBusMessageIter iter;
    DBusMessageIter dict;
    dbus_message_iter_init_append(reply, &iter);

    if (!dbus_message_iter_open_container(
        &iter,
        DBUS_TYPE_ARRAY,
        DBUS_DICT_ENTRY_BEGIN_CHAR_AS_STRING
        DBUS_TYPE_STRING_AS_STRING
        DBUS_TYPE_VARIANT_AS_STRING
        DBUS_DICT_ENTRY_END_CHAR_AS_STRING,
        &dict
    )) {
        goto bail;
    }

    const char* cstr = NULL;
    int i = -1;
    dbus_bool_t b = false;

    cstr = CONCORDD_DBUS_CLASS_NAME_PARTITION;
    append_dict_entry(&dict,
                      CONCORDD_DBUS_INFO_CLASS_NAME,
                      DBUS_TYPE_STRING,
                      &cstr);

    cstr = ge_text_to_ascii_one_line(
        partition->encoded_touchpad_text,
        partition->encoded_touchpad_text_len
    );
    append_dict_entry(&dict,
                      CONCORDD_DBUS_INFO_TOUCHPAD_TEXT,
                      DBUS_TYPE_STRING,
                      &cstr);

    i = partition->arm_level;
    append_dict_entry(&dict,
                      CONCORDD_DBUS_INFO_ARM_LEVEL,
                      DBUS_TYPE_INT32,
                      &i);

	i = partition->siren_repeat;
    append_dict_entry(&dict,
                      CONCORDD_DBUS_INFO_SIREN_REPEAT,
                      DBUS_TYPE_INT32,
                      &i);

	b = partition->programming_mode;
	append_dict_entry(&dict,
					  CONCORDD_DBUS_INFO_PROGRAMMING_MODE,
					  DBUS_TYPE_BOOLEAN,
					  &b);

	{
		char cadence[33];
		for(i=0;i<32;i++) {
			cadence[i] = (partition->siren_cadence&(1<<(31-i)))
				? '1'
				: '0';
		}
		cadence[32] = 0;
		cstr = cadence;
		append_dict_entry(&dict,
						  CONCORDD_DBUS_INFO_SIREN_CADENCE,
						  DBUS_TYPE_STRING,
						  &cstr);
	}

	i = partition->siren_started_at;
    append_dict_entry(&dict,
                      CONCORDD_DBUS_INFO_SIREN_STARTED_AT,
                      DBUS_TYPE_INT32,
                      &i);

    cstr = ge_user_to_cstr(NULL,partition->arm_level_user);
    append_dict_entry(&dict,
                      CONCORDD_DBUS_INFO_ARM_LEVEL_USER,
                      DBUS_TYPE_STRING,
                      &cstr);

    b = !!(partition->feature_state & GE_RS232_FEATURE_STATE_CHIME);
    append_dict_entry(&dict,
                      CONCORDD_DBUS_INFO_CHIME,
                      DBUS_TYPE_BOOLEAN,
                      &b);

    b = !!(partition->feature_state & GE_RS232_FEATURE_STATE_ENERGY_SAVER);
    append_dict_entry(&dict,
                      CONCORDD_DBUS_INFO_ENERGY_SAVER,
                      DBUS_TYPE_BOOLEAN,
                      &b);

    b = !!(partition->feature_state & GE_RS232_FEATURE_STATE_NO_DELAY);
    append_dict_entry(&dict,
                      CONCORDD_DBUS_INFO_NO_DELAY,
                      DBUS_TYPE_BOOLEAN,
                      &b);

    b = !!(partition->feature_state & GE_RS232_FEATURE_STATE_LATCHKEY);
    append_dict_entry(&dict,
                      CONCORDD_DBUS_INFO_LATCH_KEY,
                      DBUS_TYPE_BOOLEAN,
                      &b);

    b = !!(partition->feature_state & GE_RS232_FEATURE_STATE_SILENT_ARMING);
    append_dict_entry(&dict,
                      CONCORDD_DBUS_INFO_SILENT_ARM,
                      DBUS_TYPE_BOOLEAN,
                      &b);

    b = !!(partition->feature_state & GE_RS232_FEATURE_STATE_QUICK_ARM);
    append_dict_entry(&dict,
                      CONCORDD_DBUS_INFO_QUICK_ARM,
                      DBUS_TYPE_BOOLEAN,
                      &b);

    dbus_message_iter_close_container(&iter, &dict);

    dbus_connection_send(self->dbus_connection, reply, NULL);

    ret = DBUS_HANDLER_RESULT_HANDLED;
bail:
    dbus_message_unref(reply);
    return ret;
}

static DBusHandlerResult
concordd_dbus_handle_system_get_info(
    concordd_dbus_server_t self,
    DBusConnection *connection,
    DBusMessage *   message
) {
    DBusHandlerResult ret = DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    const char* path = dbus_message_get_path(message);
    DBusMessage *reply = dbus_message_new_method_return(message);
    ge_rs232_status_t status;

    syslog(LOG_DEBUG, "Sending DBus response for \"%s\" to \"%s\"", dbus_message_get_member(message), dbus_message_get_sender(message));

    if (!reply) {
        goto bail;
    }

    DBusMessageIter iter;
    DBusMessageIter dict;
    dbus_message_iter_init_append(reply, &iter);

    if (!dbus_message_iter_open_container(
        &iter,
        DBUS_TYPE_ARRAY,
        DBUS_DICT_ENTRY_BEGIN_CHAR_AS_STRING
        DBUS_TYPE_STRING_AS_STRING
        DBUS_TYPE_VARIANT_AS_STRING
        DBUS_DICT_ENTRY_END_CHAR_AS_STRING,
        &dict
    )) {
        goto bail;
    }

    const char* cstr = NULL;
    int i = -1;
    dbus_bool_t b = false;

    i = self->instance->panel_type;
    append_dict_entry(&dict,
                      CONCORDD_DBUS_INFO_PANEL_TYPE,
                      DBUS_TYPE_INT32,
                      &i);

    i = self->instance->hw_rev;
    append_dict_entry(&dict,
                      CONCORDD_DBUS_INFO_HW_REVISION,
                      DBUS_TYPE_INT32,
                      &i);

    i = self->instance->sw_rev;
    append_dict_entry(&dict,
                      CONCORDD_DBUS_INFO_SW_REVISION,
                      DBUS_TYPE_INT32,
                      &i);

    i = self->instance->serial_number;
    append_dict_entry(&dict,
                      CONCORDD_DBUS_INFO_SERIAL_NUMBER,
                      DBUS_TYPE_INT32,
                      &i);

	b = self->instance->ac_power_failure;
    append_dict_entry(&dict,
                      CONCORDD_DBUS_INFO_AC_POWER_FAILURE,
                      DBUS_TYPE_BOOLEAN,
                      &b);

	i = self->instance->ac_power_failure_changed_timestamp;
    append_dict_entry(&dict,
                      CONCORDD_DBUS_INFO_AC_POWER_FAILURE_CHANGED_TIMESTAMP,
                      DBUS_TYPE_INT32,
                      &i);

    dbus_message_iter_close_container(&iter, &dict);

    dbus_connection_send(self->dbus_connection, reply, NULL);

    ret = DBUS_HANDLER_RESULT_HANDLED;
bail:
    dbus_message_unref(reply);
    return ret;
}

static DBusHandlerResult
concordd_dbus_handle_output_get_info(
    concordd_dbus_server_t self,
    DBusConnection *connection,
    DBusMessage *   message
) {
    DBusHandlerResult ret = DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    const char* path = dbus_message_get_path(message);
    const int partition_index = concordd_partition_index_from_dbus_path(path, self->instance);
    const int output_index = concordd_output_index_from_dbus_path(path, self->instance);
	concordd_output_t output = concordd_output_from_dbus_path(path, self->instance);
    DBusMessage *reply = dbus_message_new_method_return(message);
    ge_rs232_status_t status;

    if (output_index < 0 || output == NULL || !output->active) {
        goto bail;
    }

    syslog(LOG_DEBUG, "Sending DBus response for \"%s\" to \"%s\"", dbus_message_get_member(message), dbus_message_get_sender(message));

    if (!reply) {
        goto bail;
    }

    DBusMessageIter iter;
    DBusMessageIter dict;
    dbus_message_iter_init_append(reply, &iter);

    if (!dbus_message_iter_open_container(
        &iter,
        DBUS_TYPE_ARRAY,
        DBUS_DICT_ENTRY_BEGIN_CHAR_AS_STRING
        DBUS_TYPE_STRING_AS_STRING
        DBUS_TYPE_VARIANT_AS_STRING
        DBUS_DICT_ENTRY_END_CHAR_AS_STRING,
        &dict
    )) {
        goto bail;
    }

    const char* cstr = NULL;
    int i = -1;
    dbus_bool_t b = false;

    cstr = CONCORDD_DBUS_CLASS_NAME_OUTPUT;
    append_dict_entry(&dict,
                      CONCORDD_DBUS_INFO_CLASS_NAME,
                      DBUS_TYPE_STRING,
                      &cstr);

	i = output_index;
    append_dict_entry(&dict,
                      CONCORDD_DBUS_INFO_OUTPUT_ID,
                      DBUS_TYPE_INT32,
                      &i);

	if (output->encoded_name_len > 0) {
		cstr = ge_text_to_ascii_one_line(
			output->encoded_name,
			output->encoded_name_len
		);
		append_dict_entry(&dict,
						  CONCORDD_DBUS_INFO_NAME,
						  DBUS_TYPE_STRING,
						  &cstr);
	}

    i = output->partition_id;
    append_dict_entry(&dict,
                      CONCORDD_DBUS_INFO_PARTITION_ID,
                      DBUS_TYPE_INT32,
                      &i);

	b = output->output_state;
    append_dict_entry(&dict,
                      CONCORDD_DBUS_INFO_VALUE,
                      DBUS_TYPE_BOOLEAN,
                      &b);

	b = output->pulse;
    append_dict_entry(&dict,
                      CONCORDD_DBUS_INFO_PULSE,
                      DBUS_TYPE_BOOLEAN,
                      &b);

	cstr = ge_user_to_cstr(NULL, output->last_changed_by);
    append_dict_entry(&dict,
                      CONCORDD_DBUS_INFO_LAST_CHANGED_BY,
                      DBUS_TYPE_STRING,
                      &cstr);

	i = (int32_t)output->last_changed_at;
    append_dict_entry(&dict,
                      CONCORDD_DBUS_INFO_LAST_CHANGED_AT,
                      DBUS_TYPE_INT32,
                      &i);

    dbus_message_iter_close_container(&iter, &dict);

    dbus_connection_send(self->dbus_connection, reply, NULL);

    ret = DBUS_HANDLER_RESULT_HANDLED;
bail:
    dbus_message_unref(reply);
    return ret;
}


static DBusHandlerResult
concordd_dbus_handle_light_get_info(
    concordd_dbus_server_t self,
    DBusConnection *connection,
    DBusMessage *   message
) {
    DBusHandlerResult ret = DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    const char* path = dbus_message_get_path(message);
    const int partition_index = concordd_partition_index_from_dbus_path(path, self->instance);
    const int light_index = concordd_light_index_from_dbus_path(path, self->instance);
	concordd_light_t light = concordd_light_from_dbus_path(path, self->instance);
    DBusMessage *reply = dbus_message_new_method_return(message);
    ge_rs232_status_t status;

    if (light_index < 0 || light == NULL) {
        goto bail;
    }

    syslog(LOG_DEBUG, "Sending DBus response for \"%s\" to \"%s\"", dbus_message_get_member(message), dbus_message_get_sender(message));

    if (!reply) {
        goto bail;
    }

    DBusMessageIter iter;
    DBusMessageIter dict;
    dbus_message_iter_init_append(reply, &iter);

    if (!dbus_message_iter_open_container(
        &iter,
        DBUS_TYPE_ARRAY,
        DBUS_DICT_ENTRY_BEGIN_CHAR_AS_STRING
        DBUS_TYPE_STRING_AS_STRING
        DBUS_TYPE_VARIANT_AS_STRING
        DBUS_DICT_ENTRY_END_CHAR_AS_STRING,
        &dict
    )) {
        goto bail;
    }

    const char* cstr = NULL;
    int i = -1;
    dbus_bool_t b = false;

    cstr = CONCORDD_DBUS_CLASS_NAME_LIGHT;
    append_dict_entry(&dict,
                      CONCORDD_DBUS_INFO_CLASS_NAME,
                      DBUS_TYPE_STRING,
                      &cstr);

	i = light_index;
    append_dict_entry(&dict,
                      CONCORDD_DBUS_INFO_LIGHT_ID,
                      DBUS_TYPE_INT32,
                      &i);

    i = partition_index;
    append_dict_entry(&dict,
                      CONCORDD_DBUS_INFO_PARTITION_ID,
                      DBUS_TYPE_INT32,
                      &i);

	i = light->zone_id;
    append_dict_entry(&dict,
                      CONCORDD_DBUS_INFO_ZONE_ID,
                      DBUS_TYPE_INT32,
                      &i);

	b = light->light_state;
    append_dict_entry(&dict,
                      CONCORDD_DBUS_INFO_VALUE,
                      DBUS_TYPE_BOOLEAN,
                      &b);

	/*
	cstr = ge_user_to_cstr(NULL, light->last_changed_by);
    append_dict_entry(&dict,
                      CONCORDD_DBUS_INFO_LAST_CHANGED_BY,
                      DBUS_TYPE_STRING,
                      &cstr);
	*/

	i = (int)light->last_changed_at;
    append_dict_entry(&dict,
                      CONCORDD_DBUS_INFO_LAST_CHANGED_AT,
                      DBUS_TYPE_INT32,
                      &i);

    dbus_message_iter_close_container(&iter, &dict);

    dbus_connection_send(self->dbus_connection, reply, NULL);

    ret = DBUS_HANDLER_RESULT_HANDLED;
bail:
    dbus_message_unref(reply);
    return ret;
}

static DBusHandlerResult
concordd_dbus_handle_zone_get_info(
    concordd_dbus_server_t self,
    DBusConnection *connection,
    DBusMessage *   message
) {
    DBusHandlerResult ret = DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    const char* path = dbus_message_get_path(message);
    const int zone_index = concordd_zone_index_from_dbus_path(path, self->instance);
    concordd_zone_t zone = concordd_get_zone(self->instance, zone_index);
    DBusMessage *reply = dbus_message_new_method_return(message);
    ge_rs232_status_t status;

    if (zone_index < 0 || zone == NULL || !zone->active) {
        goto bail;
    }

    syslog(LOG_DEBUG, "Sending DBus response for \"%s\" to \"%s\"", dbus_message_get_member(message), dbus_message_get_sender(message));

    if (!reply) {
        goto bail;
    }

    DBusMessageIter iter;
    DBusMessageIter dict;
    dbus_message_iter_init_append(reply, &iter);

    if (!dbus_message_iter_open_container(
        &iter,
        DBUS_TYPE_ARRAY,
        DBUS_DICT_ENTRY_BEGIN_CHAR_AS_STRING
        DBUS_TYPE_STRING_AS_STRING
        DBUS_TYPE_VARIANT_AS_STRING
        DBUS_DICT_ENTRY_END_CHAR_AS_STRING,
        &dict
    )) {
        goto bail;
    }

    const char* cstr = NULL;
    int i = -1;
    dbus_bool_t b = false;

    cstr = CONCORDD_DBUS_CLASS_NAME_ZONE;
    append_dict_entry(&dict,
                      CONCORDD_DBUS_INFO_CLASS_NAME,
                      DBUS_TYPE_STRING,
                      &cstr);

	i = zone_index;
    append_dict_entry(&dict,
                      CONCORDD_DBUS_INFO_ZONE_ID,
                      DBUS_TYPE_INT32,
                      &i);

    cstr = ge_text_to_ascii_one_line(
        zone->encoded_name,
        zone->encoded_name_len
    );
    append_dict_entry(&dict,
                      CONCORDD_DBUS_INFO_NAME,
                      DBUS_TYPE_STRING,
                      &cstr);

    i = zone->partition_id;
    append_dict_entry(&dict,
                      CONCORDD_DBUS_INFO_PARTITION_ID,
                      DBUS_TYPE_INT32,
                      &i);

	i = zone->type;
    append_dict_entry(&dict,
                      CONCORDD_DBUS_INFO_TYPE,
                      DBUS_TYPE_INT32,
                      &i);

	if (zone->last_changed_at != 0) {
		i = zone->last_changed_at;
		append_dict_entry(&dict,
						  CONCORDD_DBUS_INFO_LAST_CHANGED_AT,
						  DBUS_TYPE_INT32,
						  &i);
	}

	if (zone->last_tripped_at != 0) {
		i = zone->last_tripped_at;
		append_dict_entry(&dict,
						  CONCORDD_DBUS_INFO_LAST_TRIPPED_AT,
						  DBUS_TYPE_INT32,
						  &i);
	}

    i = zone->group;
    append_dict_entry(&dict,
                      CONCORDD_DBUS_INFO_GROUP,
                      DBUS_TYPE_INT32,
                      &i);

    b = !!(zone->zone_state & GE_RS232_ZONE_STATUS_TRIPPED);
    append_dict_entry(&dict,
                      CONCORDD_DBUS_INFO_IS_TRIPPED,
                      DBUS_TYPE_BOOLEAN,
                      &b);

    b = !!(zone->zone_state & GE_RS232_ZONE_STATUS_BYPASSED);
    append_dict_entry(&dict,
                      CONCORDD_DBUS_INFO_IS_BYPASSED,
                      DBUS_TYPE_BOOLEAN,
                      &b);

    b = !!(zone->zone_state & GE_RS232_ZONE_STATUS_TROUBLE);
    append_dict_entry(&dict,
                      CONCORDD_DBUS_INFO_IS_TROUBLE,
                      DBUS_TYPE_BOOLEAN,
                      &b);

    b = !!(zone->zone_state & GE_RS232_ZONE_STATUS_ALARM);
    append_dict_entry(&dict,
                      CONCORDD_DBUS_INFO_IS_ALARM,
                      DBUS_TYPE_BOOLEAN,
                      &b);

    b = !!(zone->zone_state & GE_RS232_ZONE_STATUS_FAULT);
    append_dict_entry(&dict,
                      CONCORDD_DBUS_INFO_IS_FAULT,
                      DBUS_TYPE_BOOLEAN,
                      &b);

	i = zone->last_kc;
	append_dict_entry(&dict,
					  CONCORDD_DBUS_INFO_LAST_KC,
					  DBUS_TYPE_INT32,
					  &i);

	i = (int32_t)zone->last_kc_changed_at;
	append_dict_entry(&dict,
					  CONCORDD_DBUS_INFO_LAST_KC_CHANGED_AT,
					  DBUS_TYPE_INT32,
					  &i);

    dbus_message_iter_close_container(&iter, &dict);

    dbus_connection_send(self->dbus_connection, reply, NULL);

    ret = DBUS_HANDLER_RESULT_HANDLED;
bail:
    dbus_message_unref(reply);
    return ret;
}

static bool
append_dict_event(DBusMessageIter *dict, concordd_event_t event)
{
    const char* cstr = NULL;
    int i = -1;
    dbus_bool_t b = false;
	const char* category = "UNKNOWN";
	const char* specific_desc = "";

	switch(event->general_type) {
	case GE_RS232_ALARM_GENERAL_TYPE_SYSTEM_TROUBLE:
	case GE_RS232_ALARM_GENERAL_TYPE_SYSTEM_TROUBLE_RESTORAL:
		category = "SYSTEM-TROUBLE";
		specific_desc = ge_specific_system_trouble_to_cstr(NULL, event->specific_type);
		break;

	case GE_RS232_ALARM_GENERAL_TYPE_ALARM:
	case GE_RS232_ALARM_GENERAL_TYPE_ALARM_RESTORAL:
	case GE_RS232_ALARM_GENERAL_TYPE_ALARM_CANCEL:
		category = "PARTITION-ALARM";
		specific_desc = ge_specific_alarm_to_cstr(NULL, event->specific_type);
		break;

	case GE_RS232_ALARM_GENERAL_TYPE_FIRE_TROUBLE:
	case GE_RS232_ALARM_GENERAL_TYPE_NONFIRE_TROUBLE:
	case GE_RS232_ALARM_GENERAL_TYPE_FIRE_TROUBLE_RESTORAL:
	case GE_RS232_ALARM_GENERAL_TYPE_NONFIRE_TROUBLE_RESTORAL:
		category = "PARTITION-TROUBLE";
		specific_desc = ge_specific_trouble_to_cstr(NULL, event->specific_type);
		break;

	case GE_RS232_ALARM_GENERAL_TYPE_PARTITION_EVENT:
		category = "PARTITION-EVENT";
		specific_desc = ge_specific_partition_to_cstr(NULL, event->specific_type);
		break;

	case GE_RS232_ALARM_GENERAL_TYPE_SYSTEM_EVENT:
		category = "SYSTEM-EVENT";
		break;
	}

	switch (event->status) {
	case CONCORDD_EVENT_STATUS_ONGOING:
		cstr = "ONGOING";
		break;
	case CONCORDD_EVENT_STATUS_CANCELED:
		cstr = "CANCELED";
		break;
	case CONCORDD_EVENT_STATUS_RESTORED:
		cstr = "RESTORED";
		break;
	default:
	case CONCORDD_EVENT_STATUS_UNSPECIFIED:
		cstr = "TRIGGERED";
		break;
	}
    append_dict_entry(dict,
                      CONCORDD_DBUS_EXCEPTION_STATUS,
                      DBUS_TYPE_STRING,
                      &cstr);

    cstr = category;
	append_dict_entry(dict,
                      CONCORDD_DBUS_EXCEPTION_CATEGORY,
                      DBUS_TYPE_STRING,
                      &cstr);

    cstr = specific_desc;
	append_dict_entry(dict,
                      CONCORDD_DBUS_EXCEPTION_DESCRIPTION,
                      DBUS_TYPE_STRING,
                      &cstr);

    i = event->general_type;
    append_dict_entry(dict,
                      CONCORDD_DBUS_EXCEPTION_GENERAL_TYPE,
                      DBUS_TYPE_INT32,
                      &i);

    i = event->specific_type;
    append_dict_entry(dict,
                      CONCORDD_DBUS_EXCEPTION_SPECIFIC_TYPE,
                      DBUS_TYPE_INT32,
                      &i);

    i = event->partition_id;
    append_dict_entry(dict,
                      CONCORDD_DBUS_INFO_PARTITION_ID,
                      DBUS_TYPE_INT32,
                      &i);

    i = event->source_type;
    append_dict_entry(dict,
                      CONCORDD_DBUS_EXCEPTION_SOURCE_TYPE,
                      DBUS_TYPE_INT32,
                      &i);

    i = event->extra_data;
    append_dict_entry(dict,
                      CONCORDD_DBUS_EXCEPTION_EXTRA_DATA,
                      DBUS_TYPE_INT32,
                      &i);

    if (event->zone_id != 0xFFFF) {
        i = event->zone_id;
        append_dict_entry(dict,
                          CONCORDD_DBUS_EXCEPTION_ZONE_ID,
                          DBUS_TYPE_INT32,
                          &i);
    } else {
        i = event->device_id;
        append_dict_entry(dict,
                          CONCORDD_DBUS_EXCEPTION_UNIT_ID,
                          DBUS_TYPE_INT32,
                          &i);
    }

    return true;
}

void
concordd_dbus_event_func(concordd_dbus_server_t self, concordd_instance_t instance, concordd_event_t event)
{
    char path[120] = {0};
    const char* name = CONCORDD_DBUS_SIGNAL_EVENT;
    DBusMessageIter iter;
    DBusMessageIter dict;
    DBusMessage *message = NULL;

    switch(event->general_type) {
    case GE_RS232_ALARM_GENERAL_TYPE_SYSTEM_TROUBLE:
    case GE_RS232_ALARM_GENERAL_TYPE_SYSTEM_TROUBLE_RESTORAL:
        snprintf(path, sizeof(path), "%s", CONCORDD_DBUS_PATH_ROOT);
        name = CONCORDD_DBUS_SIGNAL_TROUBLE;
        break;

    case GE_RS232_ALARM_GENERAL_TYPE_ALARM:
    case GE_RS232_ALARM_GENERAL_TYPE_ALARM_RESTORAL:
    case GE_RS232_ALARM_GENERAL_TYPE_ALARM_CANCEL:
        snprintf(path, sizeof(path), "%s%d", CONCORDD_DBUS_PATH_PARTITION, event->partition_id);
        name = CONCORDD_DBUS_SIGNAL_ALARM;
        break;

    case GE_RS232_ALARM_GENERAL_TYPE_FIRE_TROUBLE:
    case GE_RS232_ALARM_GENERAL_TYPE_NONFIRE_TROUBLE:
    case GE_RS232_ALARM_GENERAL_TYPE_FIRE_TROUBLE_RESTORAL:
    case GE_RS232_ALARM_GENERAL_TYPE_NONFIRE_TROUBLE_RESTORAL:
        snprintf(path, sizeof(path), "%s%d", CONCORDD_DBUS_PATH_PARTITION, event->partition_id);
        name = CONCORDD_DBUS_SIGNAL_TROUBLE;
        break;

	case GE_RS232_ALARM_GENERAL_TYPE_PARTITION_CONFIG_CHANGE:
	case GE_RS232_ALARM_GENERAL_TYPE_PARTITION_EVENT:
	case GE_RS232_ALARM_GENERAL_TYPE_PARTITION_TEST:
        snprintf(path, sizeof(path), "%s%d", CONCORDD_DBUS_PATH_PARTITION, event->partition_id);
		break;

    default:
    case GE_RS232_ALARM_GENERAL_TYPE_SYSTEM_EVENT:
        snprintf(path, sizeof(path), "%s", CONCORDD_DBUS_PATH_ROOT);
        break;
    }

    message = dbus_message_new_signal(
        path,
        CONCORDD_DBUS_INTERFACE,
        name
    );

    dbus_message_iter_init_append(message, &iter);

    if (!dbus_message_iter_open_container(
        &iter,
        DBUS_TYPE_ARRAY,
        DBUS_DICT_ENTRY_BEGIN_CHAR_AS_STRING
        DBUS_TYPE_STRING_AS_STRING
        DBUS_TYPE_VARIANT_AS_STRING
        DBUS_DICT_ENTRY_END_CHAR_AS_STRING,
        &dict
    )) {
        goto bail;
    }

    if (!append_dict_event(&dict, event)) {
        goto bail;
    }

    dbus_message_iter_close_container(&iter, &dict);

    dbus_connection_send(self->dbus_connection, message, NULL);

bail:
    if (message != NULL) {
        dbus_message_unref(message);
    }
}

static DBusHandlerResult
concordd_dbus_handle_partition_get_troubles(
    concordd_dbus_server_t self,
    DBusConnection *connection,
    DBusMessage *   message
) {
    DBusHandlerResult ret = DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

	return ret;
}

static DBusHandlerResult
concordd_dbus_handle_partition_get_alarms(
    concordd_dbus_server_t self,
    DBusConnection *connection,
    DBusMessage *   message
) {
    DBusHandlerResult ret = DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

	return ret;
}

static DBusHandlerResult
concordd_dbus_handle_system_get_troubles(
    concordd_dbus_server_t self,
    DBusConnection *connection,
    DBusMessage *   message
) {
    DBusHandlerResult ret = DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

	return ret;
}

static DBusHandlerResult
concordd_dbus_handle_system_get_event_log(
    concordd_dbus_server_t self,
    DBusConnection *connection,
    DBusMessage *   message
) {
    DBusHandlerResult ret = DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

	return ret;
}

static DBusHandlerResult
concordd_dbus_handle_zone_set_bypassed(
    concordd_dbus_server_t self,
    DBusConnection *connection,
    DBusMessage *   message
) {
    DBusHandlerResult ret = DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    const char* path = dbus_message_get_path(message);
    const int zone_index = concordd_zone_index_from_dbus_path(path, self->instance);
    struct concordd_dbus_callback_helper_s* helper;
    int user_index = -1;
    bool state = false;
    ge_rs232_status_t status;

    if (zone_index < 0) {
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    }

    dbus_message_get_args(
        message, NULL,
        DBUS_TYPE_BOOLEAN, &state,
        DBUS_TYPE_INT32, &user_index,
        DBUS_TYPE_INVALID
    );

    helper = concordd_dbus_callback_helper_new(self, message);

    status = concordd_set_zone_bypass(
        self->instance,
        zone_index,
        state,
		user_index,
        &concordd_dbus_callback_helper,
        (void*)helper);

    if (status < 0) {
        if (status == GE_RS232_STATUS_ALREADY) {
            status = GE_RS232_STATUS_OK;
        }
        concordd_dbus_callback_helper((void*)helper, status);
    }

    return DBUS_HANDLER_RESULT_HANDLED;


	return ret;
}

void
concordd_dbus_system_info_changed_func(concordd_dbus_server_t self, concordd_instance_t instance, int changed)
{
    DBusMessageIter iter;
    DBusMessageIter dict;
    DBusMessage *message = NULL;

	if (changed == 0) {
		goto bail;
	}

    message = dbus_message_new_signal(
        CONCORDD_DBUS_PATH_ROOT,
        CONCORDD_DBUS_INTERFACE,
        CONCORDD_DBUS_SIGNAL_CHANGED
    );

    dbus_message_iter_init_append(message, &iter);

    if (!dbus_message_iter_open_container(
        &iter,
        DBUS_TYPE_ARRAY,
        DBUS_DICT_ENTRY_BEGIN_CHAR_AS_STRING
        DBUS_TYPE_STRING_AS_STRING
        DBUS_TYPE_VARIANT_AS_STRING
        DBUS_DICT_ENTRY_END_CHAR_AS_STRING,
        &dict
    )) {
        goto bail;
    }

    const char* cstr = NULL;
    int i = -1;
    dbus_bool_t b = false;

	if (changed & CONCORDD_INSTANCE_PANEL_TYPE_CHANGED) {
		i = self->instance->panel_type;
		append_dict_entry(&dict,
						  CONCORDD_DBUS_INFO_PANEL_TYPE,
						  DBUS_TYPE_INT32,
						  &i);
	}

	if (changed & CONCORDD_INSTANCE_HW_REV_CHANGED) {
		i = self->instance->hw_rev;
		append_dict_entry(&dict,
						  CONCORDD_DBUS_INFO_HW_REVISION,
						  DBUS_TYPE_INT32,
						  &i);
	}

	if (changed & CONCORDD_INSTANCE_SW_REV_CHANGED) {
		i = self->instance->sw_rev;
		append_dict_entry(&dict,
						  CONCORDD_DBUS_INFO_SW_REVISION,
						  DBUS_TYPE_INT32,
						  &i);
	}

	if (changed & CONCORDD_INSTANCE_SERIAL_NUMBER_CHANGED) {
		i = self->instance->serial_number;
		append_dict_entry(&dict,
						  CONCORDD_DBUS_INFO_SERIAL_NUMBER,
						  DBUS_TYPE_INT32,
						  &i);
	}

	if (changed & CONCORDD_INSTANCE_AC_POWER_FAILURE_CHANGED) {
		b = self->instance->ac_power_failure;
		append_dict_entry(&dict,
						  CONCORDD_DBUS_INFO_AC_POWER_FAILURE,
						  DBUS_TYPE_BOOLEAN,
						  &b);
	}

    dbus_message_iter_close_container(&iter, &dict);

    dbus_connection_send(self->dbus_connection, message, NULL);

bail:
    if (message != NULL) {
        dbus_message_unref(message);
    }
}

void
concordd_dbus_partition_info_changed_func(concordd_dbus_server_t self, concordd_instance_t instance, concordd_partition_t partition, int changed)
{
    char path[120] = {0};
    const char* name = CONCORDD_DBUS_SIGNAL_CHANGED;
    DBusMessageIter iter;
    DBusMessageIter dict;
    DBusMessage *message = NULL;
	int partition_id = concordd_get_partition_index(instance, partition);

	if (changed == 0) {
		goto bail;
	}

	snprintf(path, sizeof(path), "%s%d", CONCORDD_DBUS_PATH_PARTITION, partition_id);

    message = dbus_message_new_signal(
        path,
        CONCORDD_DBUS_INTERFACE,
        name
    );

    dbus_message_iter_init_append(message, &iter);

    if (!dbus_message_iter_open_container(
        &iter,
        DBUS_TYPE_ARRAY,
        DBUS_DICT_ENTRY_BEGIN_CHAR_AS_STRING
        DBUS_TYPE_STRING_AS_STRING
        DBUS_TYPE_VARIANT_AS_STRING
        DBUS_DICT_ENTRY_END_CHAR_AS_STRING,
        &dict
    )) {
        goto bail;
    }

    const char* cstr = NULL;
    int i = -1;
    dbus_bool_t b = false;

	if (changed & CONCORDD_PARTITION_TOUCHPAD_TEXT_CHANGED) {
		cstr = ge_text_to_ascii_one_line(
			partition->encoded_touchpad_text,
			partition->encoded_touchpad_text_len
		);
		append_dict_entry(&dict,
						  CONCORDD_DBUS_INFO_TOUCHPAD_TEXT,
						  DBUS_TYPE_STRING,
						  &cstr);
	}

	if (changed & CONCORDD_PARTITION_SIREN_REPEAT_CHANGED) {
		i = partition->siren_repeat;
		append_dict_entry(&dict,
						  CONCORDD_DBUS_INFO_SIREN_REPEAT,
						  DBUS_TYPE_INT32,
						  &i);
	}

	if (changed & CONCORDD_PARTITION_PROGRAMMING_MODE_CHANGED) {
		b = partition->programming_mode;
		append_dict_entry(&dict,
						  CONCORDD_DBUS_INFO_PROGRAMMING_MODE,
						  DBUS_TYPE_BOOLEAN,
						  &b);
	}


	if (changed & CONCORDD_PARTITION_SIREN_CADENCE_CHANGED) {
		char cadence[33];
		for(i=0;i<32;i++) {
			cadence[i] = (partition->siren_cadence&(1<<(31-i)))
				? '1'
				: '0';
		}
		cadence[32] = 0;
		cstr = cadence;
		append_dict_entry(&dict,
						  CONCORDD_DBUS_INFO_SIREN_CADENCE,
						  DBUS_TYPE_STRING,
						  &cstr);
	}

	if (changed & CONCORDD_PARTITION_SIREN_STARTED_AT_CHANGED) {
		i = partition->siren_started_at;
		append_dict_entry(&dict,
						  CONCORDD_DBUS_INFO_SIREN_STARTED_AT,
						  DBUS_TYPE_INT32,
						  &i);
	}

	if (changed & CONCORDD_PARTITION_ARM_LEVEL_CHANGED) {
		i = partition->arm_level;
		append_dict_entry(&dict,
						  CONCORDD_DBUS_INFO_ARM_LEVEL,
						  DBUS_TYPE_INT32,
						  &i);
		cstr = ge_user_to_cstr(NULL,partition->arm_level_user);
		append_dict_entry(&dict,
						  CONCORDD_DBUS_INFO_ARM_LEVEL_USER,
						  DBUS_TYPE_STRING,
						  &cstr);
	}

	if (changed & CONCORDD_PARTITION_CHIME_CHANGED) {
		b = !!(partition->feature_state & GE_RS232_FEATURE_STATE_CHIME);
		append_dict_entry(&dict,
						  CONCORDD_DBUS_INFO_CHIME,
						  DBUS_TYPE_BOOLEAN,
						  &b);

	}

	if (changed & CONCORDD_PARTITION_ENERGY_SAVER_CHANGED) {
		b = !!(partition->feature_state & GE_RS232_FEATURE_STATE_ENERGY_SAVER);
		append_dict_entry(&dict,
						  CONCORDD_DBUS_INFO_ENERGY_SAVER,
						  DBUS_TYPE_BOOLEAN,
						  &b);

	}

	if (changed & CONCORDD_PARTITION_NO_DELAY_CHANGED) {
		b = !!(partition->feature_state & GE_RS232_FEATURE_STATE_NO_DELAY);
		append_dict_entry(&dict,
						  CONCORDD_DBUS_INFO_NO_DELAY,
						  DBUS_TYPE_BOOLEAN,
						  &b);

	}

	if (changed & CONCORDD_PARTITION_LATCH_KEY_CHANGED) {
		b = !!(partition->feature_state & GE_RS232_FEATURE_STATE_LATCHKEY);
		append_dict_entry(&dict,
						  CONCORDD_DBUS_INFO_LATCH_KEY,
						  DBUS_TYPE_BOOLEAN,
						  &b);

	}

	if (changed & CONCORDD_PARTITION_SILENT_ARMING_CHANGED) {
		b = !!(partition->feature_state & GE_RS232_FEATURE_STATE_SILENT_ARMING);
		append_dict_entry(&dict,
						  CONCORDD_DBUS_INFO_SILENT_ARM,
						  DBUS_TYPE_BOOLEAN,
						  &b);

	}

	if (changed & CONCORDD_PARTITION_QUICK_ARM_CHANGED) {
		b = !!(partition->feature_state & GE_RS232_FEATURE_STATE_QUICK_ARM);
		append_dict_entry(&dict,
						  CONCORDD_DBUS_INFO_QUICK_ARM,
						  DBUS_TYPE_BOOLEAN,
						  &b);
	}

    dbus_message_iter_close_container(&iter, &dict);

    dbus_connection_send(self->dbus_connection, message, NULL);

bail:
    if (message != NULL) {
        dbus_message_unref(message);
    }
}

void
concordd_dbus_siren_sync_func(concordd_dbus_server_t self, concordd_instance_t instance)
{
    DBusMessage *message = NULL;

    message = dbus_message_new_signal(
        CONCORDD_DBUS_PATH_ROOT,
        CONCORDD_DBUS_INTERFACE,
        CONCORDD_DBUS_SIGNAL_SIREN_SYNC
    );

    if (message) {
		dbus_connection_send(self->dbus_connection, message, NULL);
	}

bail:
    if (message != NULL) {
        dbus_message_unref(message);
    }
}

void
concordd_dbus_zone_info_changed_func(concordd_dbus_server_t self, concordd_instance_t instance, concordd_zone_t zone, int changed)
{
    char path[120] = {0};
    const char* name = CONCORDD_DBUS_SIGNAL_CHANGED;
    DBusMessageIter iter;
    DBusMessageIter dict;
    DBusMessage *message = NULL;
	int zone_id = concordd_get_zone_index(instance, zone);

	if (changed == 0) {
		goto bail;
	}

	snprintf(path, sizeof(path), "%s%d", CONCORDD_DBUS_PATH_ZONE, zone_id);

    message = dbus_message_new_signal(
        path,
        CONCORDD_DBUS_INTERFACE,
        name
    );

    dbus_message_iter_init_append(message, &iter);

    if (!dbus_message_iter_open_container(
        &iter,
        DBUS_TYPE_ARRAY,
        DBUS_DICT_ENTRY_BEGIN_CHAR_AS_STRING
        DBUS_TYPE_STRING_AS_STRING
        DBUS_TYPE_VARIANT_AS_STRING
        DBUS_DICT_ENTRY_END_CHAR_AS_STRING,
        &dict
    )) {
        goto bail;
    }

    const char* cstr = NULL;
    int i = -1;
    dbus_bool_t b = false;

	if (changed & CONCORDD_ZONE_LAST_TRIPPED_AT_CHANGED) {
		i = (int32_t)zone->last_tripped_at;
		append_dict_entry(&dict,
						  CONCORDD_DBUS_INFO_LAST_CHANGED_AT,
						  DBUS_TYPE_INT32,
						  &i);
	}

	if (changed & CONCORDD_ZONE_ENCODED_NAME_CHANGED) {
		cstr = ge_text_to_ascii_one_line(
			zone->encoded_name,
			zone->encoded_name_len
		);
		append_dict_entry(&dict,
						  CONCORDD_DBUS_INFO_NAME,
						  DBUS_TYPE_STRING,
						  &cstr);
	}

	if (changed & CONCORDD_ZONE_PARTITION_ID_CHANGED) {
		i = zone->partition_id;
		append_dict_entry(&dict,
						  CONCORDD_DBUS_INFO_PARTITION_ID,
						  DBUS_TYPE_INT32,
						  &i);
	}

	if (changed & CONCORDD_ZONE_TYPE_CHANGED) {
		i = zone->type;
		append_dict_entry(&dict,
						  CONCORDD_DBUS_INFO_TYPE,
						  DBUS_TYPE_INT32,
						  &i);
	}

	if (changed & CONCORDD_ZONE_GROUP_CHANGED) {
		i = zone->group;
		append_dict_entry(&dict,
						  CONCORDD_DBUS_INFO_GROUP,
						  DBUS_TYPE_INT32,
						  &i);
	}

	if (changed & CONCORDD_ZONE_TRIPPED_CHANGED) {
		b = (zone->zone_state&GE_RS232_ZONE_STATUS_TRIPPED) == GE_RS232_ZONE_STATUS_TRIPPED;
		append_dict_entry(&dict,
						  CONCORDD_DBUS_INFO_IS_TRIPPED,
						  DBUS_TYPE_BOOLEAN,
						  &b);
	}

	if (changed & CONCORDD_ZONE_LAST_KC_CHANGED) {
		i = zone->last_kc;
		append_dict_entry(&dict,
						  CONCORDD_DBUS_INFO_LAST_KC,
						  DBUS_TYPE_INT32,
						  &i);
	}

	if (changed & CONCORDD_ZONE_LAST_KC_CHANGED_AT_CHANGED) {
		i = (int32_t)zone->last_kc_changed_at;
		append_dict_entry(&dict,
						  CONCORDD_DBUS_INFO_LAST_KC_CHANGED_AT,
						  DBUS_TYPE_INT32,
						  &i);
	}

	if (changed & CONCORDD_ZONE_FAULT_CHANGED) {
		b = (zone->zone_state&GE_RS232_ZONE_STATUS_FAULT) == GE_RS232_ZONE_STATUS_FAULT;
		append_dict_entry(&dict,
						  CONCORDD_DBUS_INFO_IS_FAULT,
						  DBUS_TYPE_BOOLEAN,
						  &b);
	}

	if (changed & CONCORDD_ZONE_ALARM_CHANGED) {
		b = (zone->zone_state&GE_RS232_ZONE_STATUS_ALARM) == GE_RS232_ZONE_STATUS_ALARM;
		append_dict_entry(&dict,
						  CONCORDD_DBUS_INFO_IS_ALARM,
						  DBUS_TYPE_BOOLEAN,
						  &b);
	}

	if (changed & CONCORDD_ZONE_TROUBLE_CHANGED) {
		b = (zone->zone_state&GE_RS232_ZONE_STATUS_TROUBLE) == GE_RS232_ZONE_STATUS_TROUBLE;
		append_dict_entry(&dict,
						  CONCORDD_DBUS_INFO_IS_TROUBLE,
						  DBUS_TYPE_BOOLEAN,
						  &b);
	}

	if (changed & CONCORDD_ZONE_BYPASSED_CHANGED) {
		b = (zone->zone_state&GE_RS232_ZONE_STATUS_BYPASSED) == GE_RS232_ZONE_STATUS_BYPASSED;
		append_dict_entry(&dict,
						  CONCORDD_DBUS_INFO_IS_BYPASSED,
						  DBUS_TYPE_BOOLEAN,
						  &b);
	}

    dbus_message_iter_close_container(&iter, &dict);

    dbus_connection_send(self->dbus_connection, message, NULL);

bail:
    if (message != NULL) {
        dbus_message_unref(message);
    }
}

void
concordd_dbus_light_info_changed_func(concordd_dbus_server_t self,  concordd_instance_t instance, concordd_partition_t partition, concordd_light_t light, int changed)
{
    char path[120] = {0};
    const char* name = CONCORDD_DBUS_SIGNAL_CHANGED;
    DBusMessageIter iter;
    DBusMessageIter dict;
    DBusMessage *message = NULL;
	int partition_id = concordd_get_partition_index(instance, partition);
	int light_id = concordd_get_light_index(instance, partition, light);

	if (changed == 0) {
		goto bail;
	}

	snprintf(path, sizeof(path), "%s%d%s%d", CONCORDD_DBUS_PATH_PARTITION, partition_id, "/light/", light_id);

    message = dbus_message_new_signal(
        path,
        CONCORDD_DBUS_INTERFACE,
        name
    );

    dbus_message_iter_init_append(message, &iter);

    if (!dbus_message_iter_open_container(
        &iter,
        DBUS_TYPE_ARRAY,
        DBUS_DICT_ENTRY_BEGIN_CHAR_AS_STRING
        DBUS_TYPE_STRING_AS_STRING
        DBUS_TYPE_VARIANT_AS_STRING
        DBUS_DICT_ENTRY_END_CHAR_AS_STRING,
        &dict
    )) {
        goto bail;
    }

    const char* cstr = NULL;
    int i = -1;
    dbus_bool_t b = false;

	if (changed & CONCORDD_LIGHT_LIGHT_STATE_CHANGED) {
		b = light->light_state;
		append_dict_entry(&dict,
						  CONCORDD_DBUS_INFO_VALUE,
						  DBUS_TYPE_BOOLEAN,
						  &b);
	}

	if (changed & CONCORDD_LIGHT_ZONE_ID_CHANGED) {
		i = light->zone_id;
		append_dict_entry(&dict,
						  CONCORDD_DBUS_INFO_ZONE_ID,
						  DBUS_TYPE_INT32,
						  &i);
	}

	if (changed & CONCORDD_LIGHT_LAST_CHANGED_AT_CHANGED) {
		i = (int32_t)light->last_changed_at;
		append_dict_entry(&dict,
						  CONCORDD_DBUS_INFO_LAST_CHANGED_AT,
						  DBUS_TYPE_INT32,
						  &i);
	}

	if (changed & CONCORDD_LIGHT_LAST_CHANGED_BY_CHANGED) {
		i = (int32_t)light->last_changed_by;
		append_dict_entry(&dict,
						  CONCORDD_DBUS_INFO_LAST_CHANGED_BY,
						  DBUS_TYPE_INT32,
						  &i);
	}

    dbus_message_iter_close_container(&iter, &dict);

    dbus_connection_send(self->dbus_connection, message, NULL);

bail:
    if (message != NULL) {
        dbus_message_unref(message);
    }
}

void
concordd_dbus_output_info_changed_func(concordd_dbus_server_t self, concordd_instance_t instance, concordd_output_t output, int changed)
{
    char path[120] = {0};
    const char* name = CONCORDD_DBUS_SIGNAL_CHANGED;
    DBusMessageIter iter;
    DBusMessageIter dict;
    DBusMessage *message = NULL;
	int output_id = concordd_get_output_index(instance, output);

	if (changed == 0) {
		goto bail;
	}

	snprintf(path, sizeof(path), "%s%d", CONCORDD_DBUS_PATH_OUTPUT, output_id);

    message = dbus_message_new_signal(
        path,
        CONCORDD_DBUS_INTERFACE,
        name
    );

    dbus_message_iter_init_append(message, &iter);

    if (!dbus_message_iter_open_container(
        &iter,
        DBUS_TYPE_ARRAY,
        DBUS_DICT_ENTRY_BEGIN_CHAR_AS_STRING
        DBUS_TYPE_STRING_AS_STRING
        DBUS_TYPE_VARIANT_AS_STRING
        DBUS_DICT_ENTRY_END_CHAR_AS_STRING,
        &dict
    )) {
        goto bail;
    }

    const char* cstr = NULL;
    int i = -1;
    dbus_bool_t b = false;

	if (changed & CONCORDD_OUTPUT_OUTPUT_STATE_CHANGED) {
		b = output->output_state;
		append_dict_entry(&dict,
						  CONCORDD_DBUS_INFO_VALUE,
						  DBUS_TYPE_BOOLEAN,
						  &b);
	}

	if (changed & CONCORDD_OUTPUT_LAST_CHANGED_AT_CHANGED) {
		i = (int32_t)output->last_changed_at;
		append_dict_entry(&dict,
						  CONCORDD_DBUS_INFO_LAST_CHANGED_AT,
						  DBUS_TYPE_INT32,
						  &i);
	}

	if (changed & CONCORDD_OUTPUT_LAST_CHANGED_BY_CHANGED) {
		i = (int32_t)output->last_changed_by;
		append_dict_entry(&dict,
						  CONCORDD_DBUS_INFO_LAST_CHANGED_BY,
						  DBUS_TYPE_INT32,
						  &i);
	}


	if (changed & CONCORDD_OUTPUT_ENCODED_NAME_CHANGED) {
		cstr = ge_text_to_ascii_one_line(
			output->encoded_name,
			output->encoded_name_len
		);
		append_dict_entry(&dict,
						  CONCORDD_DBUS_INFO_NAME,
						  DBUS_TYPE_STRING,
						  &cstr);
	}

	if (changed & CONCORDD_OUTPUT_PARTITION_ID_CHANGED) {
		i = output->partition_id;
		append_dict_entry(&dict,
						  CONCORDD_DBUS_INFO_PARTITION_ID,
						  DBUS_TYPE_INT32,
						  &i);
	}

    dbus_message_iter_close_container(&iter, &dict);

    dbus_connection_send(self->dbus_connection, message, NULL);

bail:
    if (message != NULL) {
        dbus_message_unref(message);
    }
}

static DBusHandlerResult
dbus_message_handler(
    DBusConnection *connection,
    DBusMessage *   message,
    void *                  user_data
    )
{
    //syslog(LOG_NOTICE, "Got DBus Message");

    concordd_dbus_server_t self = (concordd_dbus_server_t)user_data;
    const char* path = dbus_message_get_path(message);

    DBusHandlerResult ret = DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

    if (dbus_message_is_method_call(message, CONCORDD_DBUS_INTERFACE,
                                    CONCORDD_DBUS_CMD_GET_PARTITIONS)) {
        if (concordd_dbus_path_is_system(path)) {
            return concordd_dbus_handle_system_get_partitions(self, connection, message);
        }

    } else if (dbus_message_is_method_call(message, CONCORDD_DBUS_INTERFACE,
                                    CONCORDD_DBUS_CMD_GET_ZONES)) {
        if (concordd_dbus_path_is_partition(path)) {
//            return concordd_dbus_handle_partition_get_zones(self, connection, message);
        } else if (concordd_dbus_path_is_system(path)) {
//            return concordd_dbus_handle_system_get_zones(self, connection, message);
        }

    } else if (dbus_message_is_method_call(message, CONCORDD_DBUS_INTERFACE,
                                    CONCORDD_DBUS_CMD_GET_BUS_DEVICES)) {
        if (concordd_dbus_path_is_system(path)) {
//            return concordd_dbus_handle_system_get_bus_devices(self, connection, message);
        }

    } else if (dbus_message_is_method_call(message, CONCORDD_DBUS_INTERFACE,
                                    CONCORDD_DBUS_CMD_SEND_RAW_FRAME)) {
        if (concordd_dbus_path_is_system(path)) {
//            return concordd_dbus_handle_system_send_raw_frame(self, connection, message);
        }

    } else if (dbus_message_is_method_call(message, CONCORDD_DBUS_INTERFACE,
                                    CONCORDD_DBUS_CMD_GET_USERS)) {
        if (concordd_dbus_path_is_partition(path)) {
//            return concordd_dbus_handle_partition_get_users(self, connection, message);
        } else if (concordd_dbus_path_is_system(path)) {
//            return concordd_dbus_handle_system_get_users(self, connection, message);
        }

    } else if (dbus_message_is_method_call(message, CONCORDD_DBUS_INTERFACE,
                                    CONCORDD_DBUS_CMD_GET_OUTPUTS)) {
        if (concordd_dbus_path_is_system(path)) {
//            return concordd_dbus_handle_system_get_outputs(self, connection, message);
        }

    } else if (dbus_message_is_method_call(message, CONCORDD_DBUS_INTERFACE,
                                    CONCORDD_DBUS_CMD_SET_ARM_LEVEL)) {
        if (concordd_dbus_path_is_partition(path)) {
            return concordd_dbus_handle_partition_set_arm_level(self, connection, message);
        }
    } else if (dbus_message_is_method_call(message, CONCORDD_DBUS_INTERFACE,
                                    CONCORDD_DBUS_CMD_GET_TROUBLES)) {
        if (concordd_dbus_path_is_partition(path)) {
            return concordd_dbus_handle_partition_get_troubles(self, connection, message);
        } else if (concordd_dbus_path_is_system(path)) {
            return concordd_dbus_handle_system_get_troubles(self, connection, message);
        }
    } else if (dbus_message_is_method_call(message, CONCORDD_DBUS_INTERFACE,
                                    CONCORDD_DBUS_CMD_GET_ALARMS)) {
        if (concordd_dbus_path_is_partition(path)) {
            return concordd_dbus_handle_partition_get_alarms(self, connection, message);
        }
    } else if (dbus_message_is_method_call(message, CONCORDD_DBUS_INTERFACE,
                                    CONCORDD_DBUS_CMD_GET_EVENTLOG)) {
        if (concordd_dbus_path_is_system(path)) {
            return concordd_dbus_handle_system_get_event_log(self, connection, message);
        }

    } else if (dbus_message_is_method_call(message, CONCORDD_DBUS_INTERFACE,
                                    CONCORDD_DBUS_CMD_SET_VALUE)) {
        if (concordd_dbus_path_is_output(path)) {
            return concordd_dbus_handle_output_set_value(self, connection, message);
        } else if (concordd_dbus_path_is_light(path)) {
            return concordd_dbus_handle_light_set_value(self, connection, message);
        }

    } else if (dbus_message_is_method_call(message, CONCORDD_DBUS_INTERFACE,
                                    CONCORDD_DBUS_CMD_SET_BYPASSED)) {
        if (concordd_dbus_path_is_zone(path)) {
            return concordd_dbus_handle_zone_set_bypassed(self, connection, message);
		}
    } else if (dbus_message_is_method_call(message, CONCORDD_DBUS_INTERFACE,
                                    CONCORDD_DBUS_CMD_GET_INFO)) {
        if (concordd_dbus_path_is_output(path)) {
            return concordd_dbus_handle_output_get_info(self, connection, message);
        } else if (concordd_dbus_path_is_zone(path)) {
            return concordd_dbus_handle_zone_get_info(self, connection, message);
        } else if (concordd_dbus_path_is_light(path)) {
            return concordd_dbus_handle_light_get_info(self, connection, message);
        } else if (concordd_dbus_path_is_partition(path)) {
            return concordd_dbus_handle_partition_get_info(self, connection, message);
        } else if (concordd_dbus_path_is_system(path)) {
            return concordd_dbus_handle_system_get_info(self, connection, message);
        }

    } else if (dbus_message_is_method_call(message, CONCORDD_DBUS_INTERFACE,
                                    CONCORDD_DBUS_CMD_REFRESH)) {
        if (concordd_dbus_path_is_system(path)) {
            return concordd_dbus_handle_refresh(self, connection, message);
        }

    } else if (dbus_message_is_method_call(message, CONCORDD_DBUS_INTERFACE,
                                    CONCORDD_DBUS_CMD_PRESS_KEYS)) {
        if (concordd_dbus_path_is_partition(path)) {
            return concordd_dbus_handle_partition_press_keys(self, connection, message);
        }
    }

    return ret;
}

static DBusConnection *
get_dbus_connection()
{
    static DBusConnection* connection = NULL;
    DBusError error;
    dbus_error_init(&error);

    if (connection == NULL) {
        syslog(LOG_DEBUG, "Getting DBus connection");

        connection = dbus_bus_get(DBUS_BUS_STARTER, &error);

        if (!connection) {
            dbus_error_free(&error);
            dbus_error_init(&error);
            connection = dbus_bus_get(DBUS_BUS_SYSTEM, &error);
        }

        require_string(connection != NULL, bail, error.message);

        syslog(LOG_DEBUG, "Registering DBus connection");

        dbus_bus_register(connection, &error);

        require_string(error.name == NULL, bail, error.message);

        syslog(LOG_DEBUG, "Requesting DBus connection name %s", CONCORDD_DBUS_NAME);

        dbus_bus_request_name(
            connection,
            CONCORDD_DBUS_NAME,
            0,
            &error
        );

        require_string(error.name == NULL, bail, error.message);
    }

    syslog(LOG_DEBUG, "Got DBus connection name %s", CONCORDD_DBUS_NAME);

bail:
    if (error.message != NULL) {
        if (connection != NULL) {
            dbus_connection_unref(connection);
            connection = NULL;
        }
    }
    dbus_error_free(&error);

    return connection;
}

concordd_dbus_server_t
concordd_dbus_server_init(concordd_dbus_server_t self, concordd_instance_t instance)
{
    DBusError error;

    memset(self, 0, sizeof(*self));

    self->dbus_connection = get_dbus_connection();

    if (self->dbus_connection == NULL) {
        syslog(LOG_ERR, "No DBus connection\n");
        self = NULL;
        goto bail;
    }

    self->instance = instance;

    dbus_error_init(&error);

    static const DBusObjectPathVTable ipc_interface_vtable = {
        NULL,
        &dbus_message_handler,
    };

    require_action(
        dbus_connection_register_fallback(
            self->dbus_connection,
            CONCORDD_DBUS_PATH_ROOT,
            &ipc_interface_vtable,
            (void*)self
        ),
        bail,
        (self = NULL)
    );

    dbus_connection_add_filter(self->dbus_connection, &dbus_message_handler, (void*)self, NULL);

    syslog(LOG_NOTICE, "Ready. Using DBUS bus \"%s\"", dbus_bus_get_unique_name(self->dbus_connection));

bail:
    if (error.message) {
        self = NULL;
    }
    dbus_error_free(&error);
    return self;
}

int
concordd_dbus_server_process(concordd_dbus_server_t self)
{
    dbus_connection_read_write_dispatch(self->dbus_connection, 0);
    return 0;
}

int
concordd_dbus_server_update_fd_set(
    concordd_dbus_server_t self,
    fd_set *read_fd_set,
    fd_set *write_fd_set,
    fd_set *error_fd_set,
    int *max_fd, cms_t *timeout
) {
    int ret = -1;
    int unix_fd = -1;

    require(dbus_connection_get_unix_fd(self->dbus_connection, &unix_fd), bail);

    if (read_fd_set != NULL) {
        FD_SET(unix_fd, read_fd_set);
    }

    if (error_fd_set != NULL) {
        FD_SET(unix_fd, error_fd_set);
    }

    if ((write_fd_set != NULL) && dbus_connection_has_messages_to_send(self->dbus_connection)) {
        FD_SET(unix_fd, write_fd_set);
    }

    if ((max_fd != NULL)) {
        *max_fd = (*max_fd > unix_fd) ? *max_fd : unix_fd;
    }

    if (timeout != NULL) {
        if (dbus_connection_get_dispatch_status(self->dbus_connection) == DBUS_DISPATCH_DATA_REMAINS) {
            *timeout = 0;
        }

        if (dbus_connection_has_messages_to_send(self->dbus_connection)) {
            *timeout = 0;
        }
    }

    ret = 0;
bail:
    return ret;
}
