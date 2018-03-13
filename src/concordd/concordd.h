/*
 *
 * Copyright (c) 2017 Robert Quattlebaum
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

#ifndef concordd_h
#define concordd_h 1

#include <stdint.h>
#include "ge-rs232.h"

struct concordd_instance_s;
typedef struct concordd_instance_s *concordd_instance_t;

struct concordd_partition_s;
typedef struct concordd_partition_s *concordd_partition_t;

struct concordd_zone_s;
typedef struct concordd_zone_s *concordd_zone_t;

struct concordd_device_s;
typedef struct concordd_device_s *concordd_device_t;

struct concordd_device_cap_s;
typedef struct concordd_device_cap_s *concordd_device_cap_t;

struct concordd_output_s;
typedef struct concordd_output_s *concordd_output_t;

struct concordd_user_s;
typedef struct concordd_user_s *concordd_user_t;

struct concordd_light_s;
typedef struct concordd_light_s *concordd_light_t;

struct concordd_event_s;
typedef struct concordd_event_s *concordd_event_t;

#define CONCORDD_GENERAL_PARTITION_ID_CHANGED		(1<<0)
#define CONCORDD_GENERAL_ENCODED_NAME_CHANGED		(1<<1)
#define CONCORDD_GENERAL_LAST_CHANGED_BY_CHANGED	(1<<2)
#define CONCORDD_GENERAL_LAST_CHANGED_AT_CHANGED	(1<<3)
#define CONCORDD_GENERAL_STATE_CHANGED				(1<<4)

#define CONCORDD_LIGHT_LIGHT_STATE_CHANGED			CONCORDD_GENERAL_STATE_CHANGED
#define CONCORDD_LIGHT_LAST_CHANGED_BY_CHANGED		CONCORDD_GENERAL_LAST_CHANGED_BY_CHANGED
#define CONCORDD_LIGHT_LAST_CHANGED_AT_CHANGED		CONCORDD_GENERAL_LAST_CHANGED_AT_CHANGED
#define CONCORDD_LIGHT_ZONE_ID_CHANGED				(1<<8)
struct concordd_light_s {
	bool light_state;
	uint8_t zone_id;

	uint32_t last_changed_by;
	time_t last_changed_at;
};

#define CONCORDD_ZONE_PARTITION_ID_CHANGED		CONCORDD_GENERAL_PARTITION_ID_CHANGED
#define CONCORDD_ZONE_ENCODED_NAME_CHANGED		CONCORDD_GENERAL_ENCODED_NAME_CHANGED
#define CONCORDD_ZONE_LAST_TRIPPED_AT_CHANGED	CONCORDD_GENERAL_LAST_CHANGED_AT_CHANGED
#define CONCORDD_ZONE_ZONE_STATE_CHANGED		CONCORDD_GENERAL_STATE_CHANGED
#define CONCORDD_ZONE_TRIPPED_CHANGED			(GE_RS232_ZONE_STATUS_TRIPPED<<8)
#define CONCORDD_ZONE_FAULT_CHANGED				(GE_RS232_ZONE_STATUS_FAULT<<8)
#define CONCORDD_ZONE_ALARM_CHANGED				(GE_RS232_ZONE_STATUS_ALARM<<8)
#define CONCORDD_ZONE_TROUBLE_CHANGED			(GE_RS232_ZONE_STATUS_TROUBLE<<8)
#define CONCORDD_ZONE_BYPASSED_CHANGED			(GE_RS232_ZONE_STATUS_BYPASSED<<8)
#define CONCORDD_ZONE_TYPE_CHANGED				(1<<16)
#define CONCORDD_ZONE_GROUP_CHANGED				(1<<17)
struct concordd_zone_s {
	bool active;
	uint8_t partition_id;
	uint8_t type;
	uint8_t group;

	uint8_t zone_state;

	time_t last_tripped_at;

	uint8_t encoded_name[16];
	uint8_t encoded_name_len;
};

struct concordd_device_cap_s {
	uint8_t cap;
	uint8_t data;
};

struct concordd_device_s {
	bool active;
	uint32_t device_id;
	uint8_t device_status;
	concordd_device_cap_t cap[8];
};

#define CONCORDD_EVENT_STATUS_UNSPECIFIED   0
#define CONCORDD_EVENT_STATUS_ONGOING       1
#define CONCORDD_EVENT_STATUS_CANCELED      2
#define CONCORDD_EVENT_STATUS_RESTORED      3

struct concordd_event_s {
    bool valid;

    uint8_t status;

    uint8_t partition_id;
    uint8_t source_type;
    uint16_t zone_id;
    uint32_t device_id;

    uint8_t general_type;
    uint8_t specific_type;

    uint16_t extra_data;

    time_t timestamp;
};

#define CONCORDD_OUTPUT_PARTITION_ID_CHANGED		CONCORDD_GENERAL_PARTITION_ID_CHANGED
#define CONCORDD_OUTPUT_ENCODED_NAME_CHANGED		CONCORDD_GENERAL_ENCODED_NAME_CHANGED
#define CONCORDD_OUTPUT_LAST_CHANGED_BY_CHANGED		CONCORDD_GENERAL_LAST_CHANGED_BY_CHANGED
#define CONCORDD_OUTPUT_LAST_CHANGED_AT_CHANGED		CONCORDD_GENERAL_LAST_CHANGED_AT_CHANGED
#define CONCORDD_OUTPUT_OUTPUT_STATE_CHANGED		CONCORDD_GENERAL_STATE_CHANGED
struct concordd_output_s {
	bool active;
	uint8_t partition_id;
	uint8_t output_state;

	uint32_t last_changed_by;
	time_t last_changed_at;

	uint8_t id_bytes[5];

	uint8_t encoded_name[16];
	uint8_t encoded_name_len;
};

struct concordd_user_s {
	bool active;
	uint8_t partition_id;
	char code_str[7];
};

#define CONCORDD_ALARM_TYPE_MAX (40)
#define CONCORDD_TROUBLE_TYPE_MAX (20)
#define CONCORDD_SYSTEM_TROUBLE_TYPE_MAX (52)
#define CONCORDD_EVENT_LOG_MAX (256)

#define CONCORDD_PARTITION_CHIME_CHANGED					(GE_RS232_FEATURE_STATE_CHIME<<8)
#define CONCORDD_PARTITION_ENERGY_SAVER_CHANGED				(GE_RS232_FEATURE_STATE_ENERGY_SAVER<<8)
#define CONCORDD_PARTITION_NO_DELAY_CHANGED					(GE_RS232_FEATURE_STATE_NO_DELAY<<8)
#define CONCORDD_PARTITION_LATCH_KEY_CHANGED				(GE_RS232_FEATURE_STATE_LATCHKEY<<8)
#define CONCORDD_PARTITION_SILENT_ARMING_CHANGED			(GE_RS232_FEATURE_STATE_SILENT_ARMING<<8)
#define CONCORDD_PARTITION_QUICK_ARM_CHANGED				(GE_RS232_FEATURE_STATE_QUICK_ARM<<8)
#define CONCORDD_PARTITION_ARM_LEVEL_CHANGED				(1<<16)
#define CONCORDD_PARTITION_TOUCHPAD_TEXT_CHANGED			(1<<17)
#define CONCORDD_PARTITION_SIREN_REPEAT_CHANGED				(1<<18)
#define CONCORDD_PARTITION_SIREN_CADENCE_CHANGED			(1<<19)
#define CONCORDD_PARTITION_CURRENT_TEMP_CHANGED				(1<<20)
#define CONCORDD_PARTITION_ENERGY_SAVER_LOW_TEMP_CHANGED	(1<<21)
#define CONCORDD_PARTITION_ENERGY_SAVER_HIGH_TEMP_CHANGED	(1<<22)
#define CONCORDD_PARTITION_SIREN_STARTED_AT_CHANGED			(1<<23)
struct concordd_partition_s {
	bool active;
	uint8_t arm_level;
	uint16_t arm_level_user;
	time_t arm_level_timestamp;

	struct concordd_light_s light[10];
	uint8_t feature_state;

    struct concordd_event_s alarm_events[CONCORDD_ALARM_TYPE_MAX];
    struct concordd_event_s trouble_events[CONCORDD_TROUBLE_TYPE_MAX];

	uint32_t siren_repeat;
	uint32_t siren_cadence;
	time_t siren_started_at;

	uint8_t current_temp;
	uint8_t energy_saver_low_temp;
	uint8_t energy_saver_high_temp;

	uint8_t encoded_touchpad_text[32];
	uint8_t encoded_touchpad_text_len;
};



#define CONCORDD_INSTANCE_PANEL_TYPE_CHANGED		(1<<8)
#define CONCORDD_INSTANCE_HW_REV_CHANGED			(1<<9)
#define CONCORDD_INSTANCE_SW_REV_CHANGED			(1<<10)
#define CONCORDD_INSTANCE_SERIAL_NUMBER_CHANGED		(1<<11)

#define CONCORDD_MAX_PARTITIONS                 8
#define CONCORDD_MAX_ZONES                 96

struct concordd_instance_s {
	struct concordd_partition_s partition[8];
	struct concordd_zone_s zone[96];
	struct concordd_device_s bus_device[32];
	struct concordd_output_s output[70];
	struct concordd_user_s user[252];

	uint8_t panel_type;
	uint16_t hw_rev;
	uint16_t sw_rev;
	uint32_t serial_number;

	uint8_t siren_go_partition_id;

    struct concordd_event_s trouble_events[CONCORDD_SYSTEM_TROUBLE_TYPE_MAX];

    struct concordd_event_s event_log[CONCORDD_EVENT_LOG_MAX];
    uint8_t event_log_last;

	struct ge_rs232_s ge_rs232;
	struct ge_queue_s ge_queue;

	void* context;
    ge_rs232_send_bytes_func_t send_bytes_func;
	void (*instance_info_changed_func)(void* context, concordd_instance_t instance, int changed);
	void (*keyfob_button_pressed_func)(void* context, concordd_instance_t instance, int button);
	void (*partition_info_changed_func)(void* context, concordd_instance_t instance, concordd_partition_t partition, int changed);
	void (*siren_sync_func)(void* context, concordd_instance_t instance);
	void (*zone_info_changed_func)(void* context, concordd_instance_t instance, concordd_zone_t zone, int changed);
	void (*light_info_changed_func)(void* context, concordd_instance_t instance, concordd_partition_t partition, concordd_light_t light, int changed);
	void (*output_info_changed_func)(void* context, concordd_instance_t instance, concordd_output_t output, int changed);
    void (*event_func)(void* context, concordd_instance_t instance, concordd_event_t event);
};

concordd_instance_t concordd_init(concordd_instance_t self);
int concordd_get_timeout_cms(concordd_instance_t self);
ge_rs232_status_t concordd_process(concordd_instance_t self);
ge_rs232_status_t concordd_refresh(concordd_instance_t self, void (*finished)(void* context,ge_rs232_status_t status), void* context);
ge_rs232_status_t concordd_press_keys(concordd_instance_t self, int partition, const char* keys, void (*finished)(void* context,ge_rs232_status_t status),void* context);
ge_rs232_status_t concordd_handle_frame(concordd_instance_t self, const uint8_t* frame_bytes, int frame_len);
ge_rs232_status_t concordd_set_light(concordd_instance_t self, int partitioni, int light, bool state, void (*finished)(void* context,ge_rs232_status_t status),void* context);
ge_rs232_status_t concordd_set_output(concordd_instance_t self, int output, bool state, void (*finished)(void* context,ge_rs232_status_t status),void* context);
ge_rs232_status_t concordd_set_arm_level(concordd_instance_t self, int partition, int arm_level, void (*finished)(void* context,ge_rs232_status_t status),void* context);

int concordd_get_partition_index(concordd_instance_t self, concordd_partition_t partition);
concordd_partition_t concordd_get_partition(concordd_instance_t self, int i);

int concordd_get_zone_index(concordd_instance_t self, concordd_zone_t zone);
concordd_zone_t concordd_get_zone(concordd_instance_t self, int i);

concordd_user_t concordd_get_user(concordd_instance_t self, int i);

int concordd_get_light_index(concordd_instance_t self, concordd_partition_t partition, concordd_light_t light);
concordd_light_t concordd_partition_get_light(concordd_partition_t self, int i);

int concordd_get_output_index(concordd_instance_t self, concordd_output_t output);
concordd_output_t concordd_get_output(concordd_instance_t self, int i);

#endif
