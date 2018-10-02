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
#include "time-utils.h"
#include <syslog.h>
#include <stdlib.h>
#include <string.h>

concordd_partition_t
concordd_get_partition(concordd_instance_t self, int i)
{
	if (i >= 0 && i < sizeof(self->partition)/sizeof(*self->partition)) {
		return &self->partition[i];
	}
	return NULL;
}

int
concordd_get_partition_index(concordd_instance_t self, concordd_partition_t partition)
{
    if (partition == NULL) {
        return -1;
    }
    return (int)(partition - self->partition);
}

void
concordd_instance_info_changed(concordd_instance_t self, int changed)
{
    if (self->instance_info_changed_func != NULL) {
        (*self->instance_info_changed_func)(self->context, self, changed);
    }
}

void
concordd_partition_info_changed(concordd_instance_t self, concordd_partition_t partition, int changed)
{
    if (self->partition_info_changed_func != NULL) {
        (*self->partition_info_changed_func)(self->context, self, partition, changed);
    }
}

void
concordd_zone_info_changed(concordd_instance_t self, concordd_zone_t zone, int changed)
{
    if (self->zone_info_changed_func != NULL) {
        (*self->zone_info_changed_func)(self->context, self, zone, changed);
    }
}

struct concordd_device_s *
concordd_get_device(concordd_instance_t self, int deviceid)
{
	int i;
	for (i = 0; i<self->bus_device_count; i++) {
		struct concordd_device_s *device = &self->bus_device[i];
		if (device->active == true && device->device_id == deviceid) {
			return device;
		}
	}
	return NULL;
}


concordd_zone_t
concordd_get_zone(concordd_instance_t self, int i)
{
	if (i >= 0 && i < sizeof(self->zone)/sizeof(*self->zone)) {
		return &self->zone[i];
	}
	return NULL;
}

concordd_user_t
concordd_get_user(concordd_instance_t self, int i)
{
	if (i >= 0 && i < sizeof(self->user)/sizeof(*self->user)) {
		return &self->user[i];
	}
	return NULL;
}

concordd_output_t
concordd_get_output(concordd_instance_t self, int i)
{
    if (i >= 0 && i < sizeof(self->output)/sizeof(*self->output)) {
        return &self->output[i];
    }
    return NULL;
}

int
concordd_get_zone_index(concordd_instance_t self, concordd_zone_t zone)
{
    if (zone == NULL) {
        return -1;
    }
    return (int)(zone - self->zone);
}


int
concordd_get_output_index(concordd_instance_t self, concordd_output_t output)
{
    if (output == NULL) {
        return -1;
    }
    return (int)(output - self->output);
}

int
concordd_get_light_index(concordd_instance_t self, concordd_partition_t partition, concordd_light_t light)
{
    if (light == NULL) {
        return -1;
    }
    return (int)(light-partition->light);
}

concordd_light_t
concordd_partition_get_light(concordd_partition_t self, int i)
{
	if ((i >= 0) && (i <= 9)) {
		return &self->light[i];
	}
	return NULL;
}


static ge_rs232_status_t
concordd_send_bytes_(concordd_instance_t self, const uint8_t* bytes, int len,struct ge_rs232_s* instance)
{
    return self->send_bytes_func(self->context, bytes, len, instance);
}

concordd_instance_t
concordd_init(concordd_instance_t self)
{
	memset(self, 0, sizeof(*self));
	ge_rs232_init(&self->ge_rs232);

    self->ge_rs232.context = (void*)self;
	self->ge_rs232.received_message = (void*)&concordd_handle_frame;
    self->ge_rs232.send_bytes = (ge_rs232_send_bytes_func_t)concordd_send_bytes_;

	ge_queue_init(&self->ge_queue, &self->ge_rs232);

    return self;
}

ge_rs232_status_t
concordd_process(concordd_instance_t self)
{
    if (!self->ge_rs232.reading_message) {
        ge_queue_update(&self->ge_queue);
    }
    return 0;
}

ge_rs232_status_t
concordd_equipment_refresh(concordd_instance_t self, void (*finished)(void* context,ge_rs232_status_t status), void* context)
{
	static const uint8_t refresh_equipment_msg[] = { GE_RS232_ATP_EQUIP_LIST_REQUEST };
	self->refresh_pending = true;
	self->bus_device_count = 0;
    // TODO: Invalidate all alarm/trouble events (but not log)
	int i;

	// Deactivate all zones.
	for (i = 0; i < sizeof(self->zone)/sizeof(self->zone[0]); ++i) {
		self->zone[i].active = false;
	}

	// Deactivate all partitions.
	for (i = 0; i < sizeof(self->partition)/sizeof(self->partition[0]); ++i) {
		self->partition[i].active = false;
		self->partition[i].entry_delay_active = false;
		self->partition[i].exit_delay_active = false;
	}

	// Deactivate all outputs.
	for (i = 0; i < sizeof(self->output)/sizeof(self->output[0]); ++i) {
		self->output[i].active = false;
	}

	// Deactivate all bus devices.
	for (i = 0; i < sizeof(self->bus_device)/sizeof(self->bus_device[0]); ++i) {
		self->bus_device[i].active = false;
	}

	return ge_queue_message(&self->ge_queue, refresh_equipment_msg, sizeof(refresh_equipment_msg), finished, context);
}

ge_rs232_status_t
concordd_dynamic_data_refresh(concordd_instance_t self, void (*finished)(void* context,ge_rs232_status_t status), void* context)
{
    static const uint8_t dynamic_data_refresh_msg[] = { GE_RS232_ATP_DYNAMIC_DATA_REFRESH };
    return ge_queue_message(&self->ge_queue, dynamic_data_refresh_msg, sizeof(dynamic_data_refresh_msg), finished, context);
}

ge_rs232_status_t
concordd_refresh(concordd_instance_t self, void (*finished)(void* context,ge_rs232_status_t status), void* context)
{
    return concordd_equipment_refresh(self, finished, context);
//    return concordd_dynamic_data_refresh(self, finished, context);
}

ge_rs232_status_t
concordd_press_keys(concordd_instance_t self, int partition, const char* keys, void (*finished)(void* context,ge_rs232_status_t status),void* context)
{
	uint8_t msg[GE_RS232_MAX_MESSAGE_SIZE] = {
		GE_RS232_ATP_KEYPRESS,
		partition,	// Partition
		0,	// Area
	};
	uint8_t len = 3;
    //syslog(LOG_DEBUG, "Will send key sequence \"%s\" to partition %d", keys, partition);

	for(;*keys && len < GE_RS232_MAX_MESSAGE_SIZE;keys++) {
		uint8_t code = 255;
		switch(*keys) {
			case '0':
			case '1':
			case '2':
			case '3':
			case '4':
			case '5':
			case '6':
			case '7':
			case '8':
			case '9':
				code = *keys - '0';
				break;
			case '*':code = 0x0A;break;
			case '#':code = 0x0B;break;
			case 'A':case 'a':code = 0x2C;break;
			case 'B':case 'b':code = 0x30;break;
			case 'C':case 'c':code = 0x2D;break;
			case 'D':case 'd':code = 0x33;break;
			case 'E':case 'e':code = 0x2E;break;
			case 'F':case 'f':code = 0x36;break;
            case '!': msg[len-1] |= (1<<6);
			case '[':
				if(keys[1]!=0) {
					keys++;
					code = strtol(keys,(char**)&keys,16);
					if(*keys!=']') {
						syslog(LOG_WARNING,"send_keypress: '[' without ']'");
						return GE_RS232_STATUS_ERROR;
					}
				}
				break;
			default:
				syslog(LOG_WARNING,"send_keypress: bad key code %d '%c'",*keys,*keys);
				return GE_RS232_STATUS_ERROR;
				break;
		}
		if (code==255) {
			continue;
		}
		msg[len++] = code;
	}
	return ge_queue_message(&self->ge_queue, msg, len, finished, context);
}

static ge_rs232_status_t
concordd_handle_alarm(concordd_instance_t self, uint8_t partitioni, uint8_t st, uint32_t source, uint8_t type_g, uint8_t type_s, uint16_t esd)
{
    struct concordd_event_s event;
    concordd_partition_t partition = concordd_get_partition(self, partitioni);
    const char* status_string = "";

    event.valid = true;
    event.partition_id = partitioni;
    event.source_type = st;
    event.zone_id = 0xFFFF;
    event.device_id = 0xFFFFFFFF;
    switch (st) {
    case GE_RS232_ALARM_SOURCE_TYPE_BUS_DEVICE:
        event.device_id = source;
        break;
    default:
    case GE_RS232_ALARM_SOURCE_TYPE_ZONE:
        event.zone_id = (uint16_t)source;
        break;
    }
    event.general_type = type_g;
    event.specific_type = type_s;
    event.extra_data = esd;
    event.timestamp = time(NULL);
    event.status = CONCORDD_EVENT_STATUS_UNSPECIFIED;

    // Figure out our status.
    switch(type_g) {
    case GE_RS232_ALARM_GENERAL_TYPE_ALARM:
    case GE_RS232_ALARM_GENERAL_TYPE_FIRE_TROUBLE:
    case GE_RS232_ALARM_GENERAL_TYPE_NONFIRE_TROUBLE:
    case GE_RS232_ALARM_GENERAL_TYPE_SYSTEM_TROUBLE:
        event.status = CONCORDD_EVENT_STATUS_ONGOING;
        break;
    case GE_RS232_ALARM_GENERAL_TYPE_ALARM_RESTORAL:
    case GE_RS232_ALARM_GENERAL_TYPE_FIRE_TROUBLE_RESTORAL:
    case GE_RS232_ALARM_GENERAL_TYPE_NONFIRE_TROUBLE_RESTORAL:
    case GE_RS232_ALARM_GENERAL_TYPE_SYSTEM_TROUBLE_RESTORAL:
        event.status = CONCORDD_EVENT_STATUS_RESTORED;
        status_string = "-RESTORAL";
        break;
    case GE_RS232_ALARM_GENERAL_TYPE_ALARM_CANCEL:
        event.status = CONCORDD_EVENT_STATUS_CANCELED;
        status_string = "-CANCELED";
        break;
    }

    // Add the event to the event log
    self->event_log_last = (self->event_log_last + 1) % CONCORDD_EVENT_LOG_MAX;
    self->event_log[self->event_log_last] = event;

    // Update alarm/trouble lists
    switch(type_g) {
    case GE_RS232_ALARM_GENERAL_TYPE_SYSTEM_TROUBLE:
    case GE_RS232_ALARM_GENERAL_TYPE_SYSTEM_TROUBLE_RESTORAL:
        if (type_s < CONCORDD_SYSTEM_TROUBLE_TYPE_MAX) {
            self->trouble_events[type_s] = event;
        }
		if (type_s == GE_RS232_SYSTEM_TROUBLE_SPECIFIC_MAIN_AC_FAIL) {
			self->ac_power_failure = (type_g == GE_RS232_ALARM_GENERAL_TYPE_SYSTEM_TROUBLE);
			self->ac_power_failure_changed_timestamp = time(NULL);
			concordd_instance_info_changed(self, CONCORDD_INSTANCE_AC_POWER_FAILURE_CHANGED);
		}
        syslog(LOG_NOTICE, "[SYSTEM-TROUBLE%s] \"%s\" CODE:%d.%d SOURCE:%d", status_string, ge_specific_system_trouble_to_cstr(NULL, type_s), type_g, type_s, source);
        break;

    case GE_RS232_ALARM_GENERAL_TYPE_ALARM:
    case GE_RS232_ALARM_GENERAL_TYPE_ALARM_RESTORAL:
    case GE_RS232_ALARM_GENERAL_TYPE_ALARM_CANCEL:
        if (partition != NULL && type_s < CONCORDD_ALARM_TYPE_MAX) {
            partition->alarm_events[type_s] = event;
        }
        syslog(LOG_NOTICE, "[ALARM%s] \"%s\" CODE:%d.%d PN:%d SOURCE:%d", status_string, ge_specific_alarm_to_cstr(NULL, type_s), type_g, type_s, partitioni, source);
        break;

    case GE_RS232_ALARM_GENERAL_TYPE_FIRE_TROUBLE:
    case GE_RS232_ALARM_GENERAL_TYPE_NONFIRE_TROUBLE:
    case GE_RS232_ALARM_GENERAL_TYPE_FIRE_TROUBLE_RESTORAL:
    case GE_RS232_ALARM_GENERAL_TYPE_NONFIRE_TROUBLE_RESTORAL:
        if (partition != NULL && type_s < CONCORDD_TROUBLE_TYPE_MAX) {
            partition->trouble_events[type_s] = event;
        }
        syslog(LOG_NOTICE, "[TROUBLE%s] \"%s\" CODE:%d.%d PN:%d SOURCE:%d", status_string, ge_specific_trouble_to_cstr(NULL, type_s), type_g, type_s, partitioni, source);
        break;

    case GE_RS232_ALARM_GENERAL_TYPE_PARTITION_EVENT:
        syslog(LOG_NOTICE, "[EVENT] \"%s\" CODE:%d.%d PN:%d SOURCE_TYPE:%d SOURCE_ID:%d EXTRA:%d",
            ge_specific_partition_to_cstr(NULL, type_s),
            type_g,
            type_s,
            partitioni,
            st,
            source,
            esd);
        break;

    case GE_RS232_ALARM_GENERAL_TYPE_SYSTEM_CONFIG_CHANGE:
		if ((partition == NULL) || (type_s > 2)) {
			syslog(LOG_NOTICE, "[EVENT] CODE:%d.%d PN:%d SOURCE_TYPE:%d SOURCE_ID:%d EXTRA:%d",
				type_g,
				type_s,
				partitioni,
				st,
				source,
				esd);
		} else {
			if (type_s == 0) {
				partition->programming_mode = true;
				concordd_instance_info_changed(self, CONCORDD_PARTITION_PROGRAMMING_MODE_CHANGED);
				syslog(LOG_NOTICE, "[BEGIN_PROGRAMMING_MODE] PN:%d SOURCE_TYPE:%d SOURCE_ID:%d EXTRA:%d", partitioni, st, source, esd);
			} else if (type_s == 1) {
				syslog(LOG_NOTICE, "[END_PROGRAMMING_MODE] PN:%d SOURCE_TYPE:%d SOURCE_ID:%d EXTRA:%d", partitioni, st, source, esd);
				partition->programming_mode = false;
				concordd_instance_info_changed(self, CONCORDD_PARTITION_PROGRAMMING_MODE_CHANGED);
			} else if (type_s == 2) {
				syslog(LOG_NOTICE, "[END_PROGRAMMING_MODE] PN:%d SOURCE_TYPE:%d SOURCE_ID:%d EXTRA:%d", partitioni, st, source, esd);
				partition->programming_mode = false;
				concordd_instance_info_changed(self, CONCORDD_PARTITION_PROGRAMMING_MODE_CHANGED);
			}
		}
		break;
    case GE_RS232_ALARM_GENERAL_TYPE_SYSTEM_EVENT:
        if (type_s == GE_RS232_SYSTEM_EVENT_OUTPUT_ON) {
            concordd_output_t output = concordd_get_output(self,esd);
            if (output != NULL && output->output_state != 1) {
                output->active = true;
                output->output_state = 1;
                output->partition_id = partitioni;
                output->last_changed_at = time(NULL);
                output->last_changed_by = source;
                if (self->output_info_changed_func != NULL) {
                    (*self->output_info_changed_func)(self->context, self, output, CONCORDD_OUTPUT_OUTPUT_STATE_CHANGED|
                        CONCORDD_OUTPUT_LAST_CHANGED_AT_CHANGED|
                        CONCORDD_OUTPUT_LAST_CHANGED_BY_CHANGED);
                }
            }
            syslog(LOG_NOTICE, "[OUTPUT-%d-ON]: SOURCE:%d(0x%06X)", esd, source, source);
            // Stop processing.
            return GE_RS232_STATUS_OK;
        } else if (type_s == GE_RS232_SYSTEM_EVENT_OUTPUT_OFF) {
            concordd_output_t output = concordd_get_output(self,esd);
            if (output != NULL && output->output_state != 0) {
                output->active = true;
                output->output_state = 0;
                output->partition_id = partitioni;
                output->last_changed_at = time(NULL);
                output->last_changed_by = source;
                if (self->output_info_changed_func != NULL) {
                    (*self->output_info_changed_func)(self->context, self, output,
                        CONCORDD_OUTPUT_OUTPUT_STATE_CHANGED|
                        CONCORDD_OUTPUT_LAST_CHANGED_AT_CHANGED|
                        CONCORDD_OUTPUT_LAST_CHANGED_BY_CHANGED);

                }

            }
            syslog(LOG_NOTICE, "[OUTPUT-%d-OFF]: SOURCE:%d(0x%06X)", esd, source, source);
            // Stop processing.
            return GE_RS232_STATUS_OK;
		}
    default:
        syslog(LOG_NOTICE, "[EVENT] CODE:%d.%d PN:%d SOURCE_TYPE:%d SOURCE_ID:%d EXTRA:%d",
            type_g,
            type_s,
            partitioni,
            st,
            source,
            esd);
        break;
    }

    // Disatch event
    if (self->event_func != NULL) {
        (*self->event_func)(self->context, self, &self->event_log[self->event_log_last]);
    }

    return GE_RS232_STATUS_OK;
}

static ge_rs232_status_t
concordd_handle_subcmd2(concordd_instance_t self, const uint8_t* frame_bytes, int frame_len)
{
    int partitioni = frame_bytes[2];
    concordd_partition_t partition = NULL;
	concordd_zone_t zone = NULL;
    partition = concordd_get_partition(self, partitioni);

    switch (frame_bytes[1]) {
    case GE_RS232_PTA_SUBCMD2_LIGHTS_STATE:
        if (partition != NULL) {
            uint16_t light_state = (frame_bytes[5]<<8)+frame_bytes[4];
            int lighti;
            for (lighti=0; lighti<10; lighti++) {
                concordd_light_t light = &partition->light[lighti];

                if (light->light_state == !!(light_state&(1<<lighti))) {
                    continue;
                }

                light->light_state ^= 1;

                syslog(LOG_NOTICE,
                    "[LIGHT] PN:%d I:%d %s",
                    partitioni,
                    lighti,
                    light->light_state?"ON":"OFF"
                );

                if ( !self->refresh_pending
				  && (light->last_changed_at != 0)
				  && (self->light_info_changed_func != NULL)
				) {
					light->last_changed_at = time(NULL);
                    (*self->light_info_changed_func)(self->context, self,
                        partition, light,
                        CONCORDD_LIGHT_LIGHT_STATE_CHANGED
                        |CONCORDD_LIGHT_LAST_CHANGED_AT_CHANGED);
					// If this is light zero, we don't bother with the other lights.
					break;
                } else {
					light->last_changed_at = time(NULL);
				}
            }
        }
        break;
    case GE_RS232_PTA_SUBCMD2_USER_LIGHTS:
        syslog(LOG_NOTICE, "[USER_LIGHTS] PN:%d LN:%d LS:%d", partitioni, frame_bytes[8], frame_bytes[9]);
        if (partition != NULL) {
			concordd_light_t light = concordd_partition_get_light(partition, frame_bytes[8]);
			if (light != NULL) {
				light->light_state = (frame_bytes[9] != 0);
                light->last_changed_at = time(NULL);
                if (self->light_info_changed_func != NULL) {
                    (*self->light_info_changed_func)(self->context, self,
                        partition, light,
                        CONCORDD_LIGHT_LIGHT_STATE_CHANGED
                        |CONCORDD_LIGHT_LAST_CHANGED_AT_CHANGED);
                }
			}
        }
        break;
    case GE_RS232_PTA_SUBCMD2_KEYFOB:
		zone = concordd_get_zone(self, frame_bytes[5]);

		if ( (zone != NULL)
		  && (zone->last_kc != frame_bytes[6] || (frame_bytes[6] <= 3) || (time(NULL) != zone->last_kc_changed_at))
		) {
			zone->last_kc = frame_bytes[6];
			zone->last_kc_changed_at = zone->last_changed_at = time(NULL);
			zone->active = true;
			syslog(LOG_NOTICE, "[KEYFOB] PN:%d ZONE:%d KC:%d", partitioni, frame_bytes[5], frame_bytes[6]);
			concordd_zone_info_changed(self, zone, CONCORDD_ZONE_LAST_KC_CHANGED|CONCORDD_ZONE_LAST_KC_CHANGED_AT_CHANGED);
		}
        break;
    default:
        syslog(LOG_WARNING, "[UNHANDLED_SUBCMD2_%02X]",frame_bytes[1]);
        break;
    }

    return GE_RS232_STATUS_OK;
}

static ge_rs232_status_t
concordd_handle_subcmd(concordd_instance_t self, const uint8_t* frame_bytes, int frame_len)
{
    concordd_partition_t partition = NULL;
	switch (frame_bytes[1]) {
	case GE_RS232_PTA_SUBCMD_SIREN_SETUP:
        partition = concordd_get_partition(self, frame_bytes[2]);

        if (partition != NULL) {
            int changed = 0;
            uint32_t cadence = (frame_bytes[5]<<24)
                | (frame_bytes[6]<<16)
                | (frame_bytes[7]<<8)
                | (frame_bytes[8]<<0);

            partition->active = true;
			self->siren_go_partition_id = frame_bytes[2];

            partition->siren_repeat = frame_bytes[4];
            changed |= CONCORDD_PARTITION_SIREN_REPEAT_CHANGED;

            partition->siren_cadence = cadence;
            changed |= CONCORDD_PARTITION_SIREN_CADENCE_CHANGED;

			partition->siren_started_at = time(NULL);
            changed |= CONCORDD_PARTITION_SIREN_STARTED_AT_CHANGED;

            if (partition->siren_repeat == 0) {
				concordd_partition_info_changed(self, partition, changed);
			}
        }

		syslog(LOG_INFO,
			"[SIREN_SETUP] PN:%d AREA:%d RP:%d CD:%02x%02x%02x%02x",
			frame_bytes[2],
			frame_bytes[3],
			frame_bytes[4],
			frame_bytes[5],
			frame_bytes[6],
			frame_bytes[7],
			frame_bytes[8]
		);
		break;
	case GE_RS232_PTA_SUBCMD_SIREN_STOP:
		partition = concordd_get_partition(self, frame_bytes[2]);
		if (partition != NULL) {
			partition->siren_cadence = 0;
            concordd_partition_info_changed(self, partition, CONCORDD_PARTITION_SIREN_CADENCE_CHANGED);
		}
		break;
	case GE_RS232_PTA_SUBCMD_SIREN_GO:
		partition = concordd_get_partition(self, self->siren_go_partition_id);
        if ((partition != NULL) && partition->siren_cadence != 0) {
            int changed = 0;

			changed |= CONCORDD_PARTITION_SIREN_REPEAT_CHANGED;
            changed |= CONCORDD_PARTITION_SIREN_CADENCE_CHANGED;
            changed |= CONCORDD_PARTITION_SIREN_STARTED_AT_CHANGED;

            concordd_partition_info_changed(self, partition, changed);
		}
		break;
	case GE_RS232_PTA_SUBCMD_SIREN_SYNC:
		if (self->siren_sync_func != NULL) {
			(*self->siren_sync_func)(self->context, self);
		}
		break;
	case GE_RS232_PTA_SUBCMD_TOUCHPAD_DISPLAY:
		{
		int partitioni = frame_bytes[2];
		uint8_t type = frame_bytes[4]; // Seems to always be "1" on concord
		bool lcd_text_changed = true;
		concordd_partition_t partition = concordd_get_partition(self, partitioni);

		if (partition != NULL) {
			lcd_text_changed = (frame_len-5 != partition->encoded_touchpad_text_len)
				|| (0 != memcmp(frame_bytes+5, partition->encoded_touchpad_text, frame_len-5));
            partition->active = true;

			partition->encoded_touchpad_text_len = frame_len-5;
			memcpy(partition->encoded_touchpad_text, frame_bytes+5, frame_len-5);

			if (lcd_text_changed) {
                concordd_partition_info_changed(self, partition, CONCORDD_PARTITION_TOUCHPAD_TEXT_CHANGED);
            }
		}
		if (lcd_text_changed) {
			syslog(type==1?LOG_DEBUG:LOG_NOTICE,
				"[TOUCHPAD] PN:%d TYPE:%d \"%s\"",
				partitioni,
				type,
				ge_text_to_ascii_one_line(frame_bytes+5, frame_len-5)
			);
		}
		}

		break;
	case GE_RS232_PTA_SUBCMD_LEVEL:
		{
		int partitioni = frame_bytes[2];
		concordd_partition_t partition = concordd_get_partition(self, partitioni);
		if (partition) {
            partition->active = true;
			partition->arm_level = frame_bytes[6];
			partition->arm_level_user = (frame_bytes[4]<<8)+(frame_bytes[5]);
			partition->arm_level_timestamp = time(NULL);
			partition->entry_delay_active = false;
            syslog(LOG_NOTICE,"[ARM_LEVEL] PN:%d LEVEL:%d USER:%s",
                partitioni,
                partition->arm_level,
                ge_user_to_cstr(NULL,partition->arm_level_user)
            );
            concordd_partition_info_changed(self, partition, CONCORDD_PARTITION_ARM_LEVEL_CHANGED);
		} else {
            syslog(LOG_ERR,"[ARM_LEVEL] BAD PARTITION %d", partitioni);
        }
		}
		break;

	case GE_RS232_PTA_SUBCMD_ALARM_TROUBLE:
		return concordd_handle_alarm(
			self,
			frame_bytes[2],
			frame_bytes[4],
			(frame_bytes[5]<<16)+(frame_bytes[6]<<8)+frame_bytes[7],
			frame_bytes[8],
			frame_bytes[9],
			(frame_bytes[10]<<8)+frame_bytes[11]
		);
		break;

	case GE_RS232_PTA_SUBCMD_ENTRY_EXIT_DELAY:
		syslog(LOG_NOTICE,
			"[%s_%s_DELAY] PN:%d AREA:%d EXT:%d SECONDS:%d",
			frame_bytes[4]&(1<<7) ? "END" : "BEGIN",
			frame_bytes[4]&(1<<6) ? "EXIT" : "ENTRY",
			frame_bytes[2],
			frame_bytes[3],
			frame_bytes[4]&0x3,
			(frame_bytes[5]<<8)+frame_bytes[6]
		);

		concordd_partition_t partition = concordd_get_partition(self, frame_bytes[2]);
		if (partition) {
			bool new_value = !(frame_bytes[4]&(1<<7));
			if (frame_bytes[4]&(1<<6)) {
				partition->exit_delay_active = new_value;
			} else {
				partition->entry_delay_active = new_value;
			}
		}

		break;

	case GE_RS232_PTA_SUBCMD_FEATURE_STATE:
        syslog(LOG_INFO,"[FEATURE_STATE] 0x%02X", frame_bytes[4]);
		{
			int i = frame_bytes[2];
			concordd_partition_t partition = concordd_get_partition(self, i);
			if (partition != NULL) {
				uint8_t new_state = frame_bytes[4];
				int changed_features = partition->feature_state;

				changed_features ^= new_state;
				partition->feature_state = new_state;
                partition->active = true;

				if (changed_features != 0) {
					concordd_partition_info_changed(self, partition, changed_features<<8);
				}
			}
		}

		break;

	case GE_RS232_PTA_SUBCMD_TEMPERATURE:
		syslog(LOG_INFO,
			"[TEMPERATURE] PN:%d AREA:%d CUR:%d°F LOW:%d°F HIGH:%d°F",
			frame_bytes[2],
			frame_bytes[3],
			frame_bytes[4],
			frame_bytes[5],
            frame_bytes[6]
		);

		break;
	case GE_RS232_PTA_SUBCMD_TIME_AND_DATE:
		syslog(LOG_INFO,
			"[TIME_AND_DATE] %04d-%02d-%02d %02d:%02d",
			frame_bytes[6]+2000,
			frame_bytes[4],
			frame_bytes[5],
			frame_bytes[2],
			frame_bytes[3]
		);
		// TIME_AND_DATE
		self->refresh_pending = false;
		break;

	default:
        syslog(LOG_WARNING, "[UNHANDLED_SUBCMD_%02X]",frame_bytes[1]);
		break;
	}

	return GE_RS232_STATUS_OK;
}

static ge_rs232_status_t
concordd_handle_zone_status(concordd_instance_t self, const uint8_t* frame_bytes, int frame_len)
{
	uint16_t zonei = (frame_bytes[3]<<8)+frame_bytes[4];

	concordd_zone_t zone = concordd_get_zone(self, zonei);

	if (zone != NULL) {
		uint8_t state = frame_bytes[5];
		uint8_t changed_state = zone->zone_state^state;

		zone->zone_state = state;

		if ( (state&GE_RS232_ZONE_STATUS_TRIPPED)
		  && (changed_state&GE_RS232_ZONE_STATUS_TRIPPED)
		) {
			zone->last_tripped_at = time(NULL);
		}

		if ((changed_state != 0) && zone->active) {
			zone->last_changed_at = time(NULL);
			zone->active = true;
			concordd_zone_info_changed(self, zone, changed_state<<8);
		}

        zone->active = true;

        if (changed_state != 0) {
            syslog((changed_state & ~GE_RS232_ZONE_STATUS_TRIPPED) != 0?LOG_WARNING:LOG_INFO,
                "[ZONE] PN:%d ZONE:%d \"%s\" STATUS:%s%s%s%s%s",
                zone->partition_id,
                zonei,
                ge_text_to_ascii_one_line(zone->encoded_name,zone->encoded_name_len),
                zone->zone_state&GE_RS232_ZONE_STATUS_TRIPPED?"T":"-",
                zone->zone_state&GE_RS232_ZONE_STATUS_FAULT?"F":"-",
                zone->zone_state&GE_RS232_ZONE_STATUS_ALARM?"A":"-",
                zone->zone_state&GE_RS232_ZONE_STATUS_TROUBLE?"R":"-",
                zone->zone_state&GE_RS232_ZONE_STATUS_BYPASSED?"B":"-"
            );
        }
	}

	return GE_RS232_STATUS_OK;
}

static ge_rs232_status_t
concordd_handle_equip_list_partition_data(concordd_instance_t self, const uint8_t* frame_bytes, int frame_len)
{
	int partitioni = frame_bytes[1];
	concordd_partition_t partition = concordd_get_partition(self, partitioni);

	if (partition != NULL) {
		int changes = 0;

		partition->active = true;

		if (partition->arm_level != frame_bytes[3]) {
			changes |= CONCORDD_PARTITION_ARM_LEVEL_CHANGED;
			partition->arm_level = frame_bytes[3];
		}

		if (changes != 0) {
			concordd_partition_info_changed(self, partition, changes);
		}

		syslog(LOG_INFO, "[EQUIP_LIST_PARTITION_DATA] PN:%d ARM:%d TEXT:\"%s\"",
			frame_bytes[1],
			frame_bytes[3],
            ge_text_to_ascii_one_line(frame_bytes+4, frame_len-4)
		);
    }

    return GE_RS232_STATUS_OK;
}

static ge_rs232_status_t
concordd_handle_equip_list_superbus_dev_data(concordd_instance_t self, const uint8_t* frame_bytes, int frame_len)
{
	int partitioni = frame_bytes[1];
	int deviceid = (frame_bytes[3]<<16)+(frame_bytes[4]<<8)+frame_bytes[5];
	bool fault = (frame_bytes[6] != 0);
	struct concordd_device_s *device = &self->bus_device[self->bus_device_count++];

	device->active = true;
	device->device_id = deviceid;
	device->device_status = frame_bytes[6];

    syslog(
		fault?LOG_ERR:LOG_INFO,
		"[EQUIP_LIST_SUPERBUS_DEV_DATA] PN:%d DEVICEID:%08d(0x%06X) FAULT:%d",
		partitioni,
		deviceid,deviceid,
		fault,
		frame_len);

    return GE_RS232_STATUS_OK;
}

static ge_rs232_status_t
concordd_handle_equip_list_superbus_cap_data(concordd_instance_t self, const uint8_t* frame_bytes, int frame_len)
{
	int deviceid = (frame_bytes[1]<<16)+(frame_bytes[2]<<8)+frame_bytes[3];
	struct concordd_device_s *device = concordd_get_device(self, deviceid);
	int cap = frame_bytes[4];
	int data = frame_bytes[5];
	const char* cap_string = "UNKNOWN";
	if (device != NULL) {
		device->caps |= (1<<cap);
		if (cap == 0x08) {
			device->input_count = data;
		} else if (cap == 0x0b) {
			device->output_count = data;
		}
	}

	switch(cap) {
	case 0x00: cap_string = "Power Supervision"; break;
	case 0x01: cap_string = "Access Control"; break;
	case 0x02: cap_string = "Analog Smoke"; break;
	case 0x03: cap_string = "Audio Listen-In"; break;
	case 0x04: cap_string = "SnapCard Supervision"; break;
	case 0x05: cap_string = "Microburst"; break;
	case 0x06: cap_string = "Dual Phone Line"; break;
	case 0x07: cap_string = "Energy Management"; break;
	case 0x08: cap_string = "Input Zones"; break;
	case 0x09: cap_string = "Automation"; break;
	case 0x0a: cap_string = "Phone Interface"; break;
	case 0x0b: cap_string = "Relay Outputs"; break;
	case 0x0c: cap_string = "RF Receiver"; break;
	case 0x0d: cap_string = "RF Transmitter"; break;
	case 0x0e: cap_string = "Parallel Printer"; break;
	case 0x10: cap_string = "LED Touchpad"; break;
	case 0x11: cap_string = "LCD Text Touchpad"; break;
	case 0x12: cap_string = "GUI Touchpad"; break;
	case 0x13: cap_string = "Voice Evacuation"; break;
	case 0x14: cap_string = "Pager"; break;
	case 0x15: cap_string = "Downloadable code/data"; break;
	case 0x16: cap_string = "JTECH Premise Pager"; break;
	case 0x17: cap_string = "Cryptography"; break;
	case 0x18: cap_string = "LED Display"; break;
	default: break;
	}

    syslog(
		LOG_INFO,
		"[EQUIP_LIST_SUPERBUS_CAP_DATA] DEVICEID:%08d(0x%06X) CN:0x%02X(%s) CD:%d",
		deviceid,deviceid,
		cap,
		cap_string,
		data);
    return GE_RS232_STATUS_OK;
}

static ge_rs232_status_t
concordd_handle_equip_list_output_data(concordd_instance_t self, const uint8_t* frame_bytes, int frame_len)
{
	int outputi = frame_bytes[2];

	// For some reason concord panels have this bit set on the output
	outputi &= ~0x40;

	concordd_output_t output = concordd_get_output(self, outputi);

	if (output != NULL) {
		output->active       = true;
		output->output_state =   (frame_bytes[3] & 1);
		output->pulse        = !!(frame_bytes[3] & 2);
		memcpy(output->id_bytes, frame_bytes+4, 5);

		output->encoded_name_len = frame_len-9;
		memcpy(output->encoded_name, frame_bytes+9, frame_len-9);

		syslog(LOG_INFO, "[EQUIP_LIST_OUTPUT_DATA] OUT:%d(0x%02X) STATE:%d PULSE:%d ID:%02X%02X%02X%02X%02X NAME:\"%s\"",
			outputi,
			outputi,
			output->output_state,
			output->pulse,
			output->id_bytes[0],
			output->id_bytes[1],
			output->id_bytes[2],
			output->id_bytes[3],
			output->id_bytes[4],
            ge_text_to_ascii_one_line(output->encoded_name, output->encoded_name_len)
		);
	}
    return GE_RS232_STATUS_OK;
}

static ge_rs232_status_t
concordd_handle_equip_list_schedule_data(concordd_instance_t self, const uint8_t* frame_bytes, int frame_len)
{
    // TODO: Writeme
    syslog(LOG_DEBUG, "[EQUIP_LIST_SCHEDULE_DATA]");
    return GE_RS232_STATUS_OK;
}

static ge_rs232_status_t
concordd_handle_equip_list_scheduled_event_data(concordd_instance_t self, const uint8_t* frame_bytes, int frame_len)
{
    // TODO: Writeme
    syslog(LOG_DEBUG, "[EQUIP_LIST_SCHEDULED_EVENT_DATA]");
    return GE_RS232_STATUS_OK;
}

static ge_rs232_status_t
concordd_handle_equip_list_light_to_sensor_data(concordd_instance_t self, const uint8_t* frame_bytes, int frame_len)
{
	int partitioni = frame_bytes[1];
	concordd_partition_t partition = concordd_get_partition(self, partitioni);

	if (partition != NULL) {
		int i = 0;
		partition->active = true;

		for (i=1;i<10;i++) {
			concordd_light_t light = concordd_partition_get_light(partition, i);
			if (light == NULL) {
				continue;
			}
			light->zone_id = frame_bytes[2+i];
			if (light->zone_id) {
				syslog(LOG_INFO, "[EQUIP_LIST_LIGHT_TO_SENSOR_DATA] PN:%d LIGHT:%d ZONE:%d",
					frame_bytes[2],
					i,
					light->zone_id
				);
			}
		}
    }

    return GE_RS232_STATUS_OK;
}

static ge_rs232_status_t
concordd_handle_equip_list_zone_data(concordd_instance_t self, const uint8_t* frame_bytes, int frame_len)
{
	uint16_t zonei = (frame_bytes[4]<<8)+frame_bytes[5];
	concordd_zone_t zone = concordd_get_zone(self, zonei);

	if (zone != NULL) {
		int changes = 0;

		if (zone->partition_id != frame_bytes[1]) {
			changes |= CONCORDD_ZONE_PARTITION_ID_CHANGED;
			zone->partition_id = frame_bytes[1];
		}
		if (zone->group != frame_bytes[3]) {
			changes |= CONCORDD_ZONE_GROUP_CHANGED;
			zone->group = frame_bytes[3];
		}
		if (zone->type != frame_bytes[6]) {
			changes |= CONCORDD_ZONE_TYPE_CHANGED;
			zone->type = frame_bytes[6];
		}
		if (zone->zone_state != frame_bytes[7]) {
			uint8_t changed_state = (zone->zone_state^frame_bytes[7]);
			zone->zone_state = frame_bytes[7];
			changes |= (changed_state<<8);
		}

		zone->encoded_name_len = frame_len-8;
		memcpy(zone->encoded_name,frame_bytes+8, frame_len-8);

		if ((changes != 0) && zone->active) {
			zone->active = true;
			concordd_zone_info_changed(self, zone, changes);
		}

		zone->active = true;

        syslog(LOG_INFO,"[EQUIP_LIST_ZONE_INFO] ZONE:%d PN:%d AREA:%d TYPE:%d GROUP:\"%s\"(%d) STATUS:%s%s%s%s%s TEXT:\"%s\"",
            zonei,
            zone->partition_id,
            frame_bytes[2],
            zone->type,
			concordd_zone_group_get_name(zone->group),
            zone->group,
            frame_bytes[7]&GE_RS232_ZONE_STATUS_TRIPPED?"T":"-",
            frame_bytes[7]&GE_RS232_ZONE_STATUS_FAULT?"F":"-",
            frame_bytes[7]&GE_RS232_ZONE_STATUS_ALARM?"A":"-",
            frame_bytes[7]&GE_RS232_ZONE_STATUS_TROUBLE?"R":"-",
            frame_bytes[7]&GE_RS232_ZONE_STATUS_BYPASSED?"B":"-",
            ge_text_to_ascii_one_line(zone->encoded_name, zone->encoded_name_len)

        );
    } else {
        syslog(LOG_WARNING,"[EQUIP_LIST_ZONE_INFO] ZONE:%d *ERROR*",zonei);
    }

	return GE_RS232_STATUS_OK;
}

static ge_rs232_status_t
concordd_handle_equip_list_user_data(concordd_instance_t self, const uint8_t* frame_bytes, int frame_len)
{
	uint16_t useri = (frame_bytes[1]<<8)+frame_bytes[2];
	concordd_user_t user = concordd_get_user(self, useri);

    syslog(LOG_DEBUG,
        "[EQUIP_LIST_USER_DATA] USER:\"%s\"(%d) CODE=\"****\"",
        ge_user_to_cstr(NULL,useri),
        useri
    );

	if (user != NULL) {
		char* code = user->code_str;

		user->active = true;

		code[0] = (frame_bytes[4]>>4)+'0';
		code[1] = (frame_bytes[4]&0xF)+'0';
		code[2] = (frame_bytes[5]>>4)+'0';
		code[3] = (frame_bytes[5]&0xF)+'0';
		code[4] = 0;

		// Check for the zero code, which is invalid.
		if (strcmp(code,"0000") == 0) {
			code[0] = 0;
		}

//        syslog(LOG_DEBUG,
//            "[EQUIP_LIST_USER_DATA] USER:\"%s\"(%d) CODE=\"%s\"",
//            ge_user_to_cstr(NULL,useri),
//            useri,
//            code
//        );
    } else {
        syslog(LOG_WARNING,
            "[EQUIP_LIST_USER_DATA] USER:\"%s\"(%d) (No associated struct!)",
            ge_user_to_cstr(NULL,useri),
            useri
        );
    }

    return GE_RS232_STATUS_OK;
}

static ge_rs232_status_t
concordd_handle_panel_type(concordd_instance_t self, const uint8_t* frame_bytes, int frame_len)
{
	self->panel_type = frame_bytes[1];
	self->hw_rev = (frame_bytes[2]<<8) + frame_bytes[3];
	self->sw_rev = (frame_bytes[4]<<8) + frame_bytes[5];
	self->serial_number = (frame_bytes[6]<<24) + (frame_bytes[7]<<16) + (frame_bytes[8]<<8) + frame_bytes[9];
    syslog(LOG_NOTICE, "[PANEL_TYPE] PT:0x%02X HR:0x%04X SR:0x%04X SN:0x%08x",
		frame_bytes[1],
		(frame_bytes[2]<<8) + frame_bytes[3],
		(frame_bytes[4]<<8) + frame_bytes[5],
		(frame_bytes[6]<<24) + (frame_bytes[7]<<16) + (frame_bytes[8]<<8) + frame_bytes[9]
		);
    return GE_RS232_STATUS_OK;
}

ge_rs232_status_t
concordd_handle_frame(concordd_instance_t self, const uint8_t* frame_bytes, int frame_len)
{
	switch (frame_bytes[0]) {
	case GE_RS232_PTA_SUBCMD:
		return concordd_handle_subcmd(self, frame_bytes, frame_len);
	case GE_RS232_PTA_SUBCMD2:
		return concordd_handle_subcmd2(self, frame_bytes, frame_len);
	case GE_RS232_PTA_AUTOMATION_EVENT_LOST:
        syslog(LOG_WARNING, "[AUTOMATION_EVENT_LOST]");
//        return concordd_dynamic_data_refresh(self, NULL, NULL);
		break;
	case GE_RS232_PTA_CLEAR_AUTOMATION_DYNAMIC_IMAGE:
        syslog(LOG_NOTICE, "[CLEAR_AUTOMATION_DYNAMIC_IMAGE]");
        return concordd_refresh(self, NULL, NULL);
		break;
    case GE_RS232_PTA_EQUIP_LIST_COMPLETE:
        syslog(LOG_NOTICE, "[EQUIP_LIST_COMPLETE]");
		concordd_dynamic_data_refresh(self, NULL, NULL);

        break;
	case GE_RS232_PTA_ZONE_STATUS:
		return concordd_handle_zone_status(self, frame_bytes, frame_len);
	case GE_RS232_PTA_EQUIP_LIST_PARTITION_DATA:
		return concordd_handle_equip_list_partition_data(self, frame_bytes, frame_len);
	case GE_RS232_PTA_EQUIP_LIST_ZONE_DATA:
		return concordd_handle_equip_list_zone_data(self, frame_bytes, frame_len);
	case GE_RS232_PTA_EQUIP_LIST_USER_DATA:
		return concordd_handle_equip_list_user_data(self, frame_bytes, frame_len);
	case GE_RS232_PTA_EQUIP_LIST_SUPERBUS_DEV_DATA:
		return concordd_handle_equip_list_superbus_dev_data(self, frame_bytes, frame_len);
	case GE_RS232_PTA_EQUIP_LIST_SUPERBUS_CAP_DATA:
		return concordd_handle_equip_list_superbus_cap_data(self, frame_bytes, frame_len);
	case GE_RS232_PTA_EQUIP_LIST_OUTPUT_DATA:
		return concordd_handle_equip_list_output_data(self, frame_bytes, frame_len);
	case GE_RS232_PTA_EQUIP_LIST_SCHEDULE_DATA:
		return concordd_handle_equip_list_schedule_data(self, frame_bytes, frame_len);
	case GE_RS232_PTA_EQUIP_LIST_SCHEDULED_EVENT_DATA:
		return concordd_handle_equip_list_scheduled_event_data(self, frame_bytes, frame_len);
	case GE_RS232_PTA_EQUIP_LIST_LIGHT_TO_SENSOR_DATA:
		return concordd_handle_equip_list_light_to_sensor_data(self, frame_bytes, frame_len);
	case GE_RS232_PTA_PANEL_TYPE:
		return concordd_handle_panel_type(self, frame_bytes, frame_len);
		break;
    default:
        syslog(LOG_DEBUG, "[UNHANDLED_CMD_%02X]", frame_bytes[0]);
        break;
	}

	return 0;
}

int
concordd_get_timeout_cms(concordd_instance_t self)
{
    if (self->ge_rs232.last_response == GE_RS232_ACK) {
        return CMS_DISTANT_FUTURE;
    } else if (self->ge_rs232.last_response == GE_RS232_NAK) {
        return 0;
    } else {
        return MSEC_PER_SEC;
    }
    return CMS_DISTANT_FUTURE;
}

ge_rs232_status_t
concordd_set_light(concordd_instance_t self, int partitioni, int lighti, bool state, void (*finished)(void* context,ge_rs232_status_t status),void* context)
{
	if (self->refresh_pending) {
        return GE_RS232_STATUS_WAIT;
	}

    concordd_partition_t partition = concordd_get_partition(self, partitioni);
    const char cmd[] = { '[','1','1'-state,']','0'+lighti,0};

    if (partition == NULL || !partition->active) {
        return GE_RS232_STATUS_INVALID_ARGUMENT;
    }

	if (partition->programming_mode) {
        return GE_RS232_STATUS_ERROR;
	}

    if (lighti < 0 || lighti > 9) {
        return GE_RS232_STATUS_INVALID_ARGUMENT;
    }

    return concordd_press_keys(self, partitioni, cmd, finished, context);
}

ge_rs232_status_t
concordd_set_zone_bypass(concordd_instance_t self, int zonei, bool bypass, int useri, void (*finished)(void* context,ge_rs232_status_t status),void* context)
{
	if (self->refresh_pending) {
        return GE_RS232_STATUS_WAIT;
	}

	concordd_zone_t zone = concordd_get_zone(self, zonei);

    if (zone == NULL || !zone->active) {
        return GE_RS232_STATUS_INVALID_ARGUMENT;
    }

	if (useri == -1) {
		useri = GE_RS232_USER_SYSTEM;
	}

	concordd_partition_t partition = concordd_get_partition(self, zone->partition_id);
    concordd_user_t user = concordd_get_user(self, useri);
	char cmd[15];

    if (partition == NULL || !partition->active) {
        return GE_RS232_STATUS_INVALID_ARGUMENT;
    }

	if (partition->programming_mode) {
        return GE_RS232_STATUS_ERROR;
	}

	if (((zone->zone_state&GE_RS232_ZONE_STATUS_BYPASSED)==GE_RS232_ZONE_STATUS_BYPASSED)==bypass) {
		// Already in that state.
		return GE_RS232_STATUS_ALREADY;
	}

	if (user == NULL || !user->active) {
        return GE_RS232_STATUS_INVALID_ARGUMENT;
	}

	if (user->code_str[0] == 0) {
		// We can't bypass/unbypass without a code
        return GE_RS232_STATUS_ERROR;
	}

	snprintf(cmd, sizeof(cmd), "#%s%02d", user->code_str, zonei);

	return concordd_press_keys(self, zone->partition_id, cmd, finished, context);
}

ge_rs232_status_t
concordd_set_output(concordd_instance_t self, int outputi, bool state, void (*finished)(void* context,ge_rs232_status_t status),void* context)
{
	if (self->refresh_pending) {
        return GE_RS232_STATUS_WAIT;
	}

    concordd_output_t output = concordd_get_output(self, outputi);

    const char cmd[] = { '7','7','0'+outputi,0};

    if (output == NULL || output->active == false || outputi < 0) {
        return GE_RS232_STATUS_INVALID_ARGUMENT;
    }

    if (output->output_state == (uint8_t)state) {
        return GE_RS232_STATUS_ALREADY;
    }

	concordd_partition_t partition = concordd_get_partition(self, 1);

	if ((partition != NULL) && partition->programming_mode) {
        return GE_RS232_STATUS_ERROR;
	}

    return concordd_press_keys(self, 1, cmd, finished, context);
}

ge_rs232_status_t
concordd_set_arm_level(concordd_instance_t self, int partitioni, int arm_level, void (*finished)(void* context,ge_rs232_status_t status),void* context)
{
    concordd_partition_t partition = concordd_get_partition(self, partitioni);

    if (partition == NULL || !partition->active) {
        return GE_RS232_STATUS_INVALID_ARGUMENT;
    }

	/* If the arm level is different,
	 * OR the siren is wailing,
	 * OR an entry delay is active...
	 */
    if ( (partition->arm_level != arm_level)
	  || ((partition->siren_repeat == 0) && (partition->siren_started_at != 0))
	  || partition->entry_delay_active
	) {
		if (partition->programming_mode) {
			return GE_RS232_STATUS_ERROR;
		}

        switch (arm_level) {
            case 1:
                return concordd_press_keys(self, partitioni, "[20]", finished, context);
            case 2:
                return concordd_press_keys(self, partitioni, "[28]", finished, context);
            case 3:
                return concordd_press_keys(self, partitioni, "[27]", finished, context);
            default:
                return GE_RS232_STATUS_INVALID_ARGUMENT;
        }
    }

    return GE_RS232_STATUS_ALREADY;
}


#define CONCORDD_ZONE_PROPERTY_INTERIOR        (1<<0)
#define CONCORDD_ZONE_PROPERTY_EXTERIOR        (1<<1)
#define CONCORDD_ZONE_PROPERTY_POLICE          (1<<2)
#define CONCORDD_ZONE_PROPERTY_AUXILIARY       (1<<3)
#define CONCORDD_ZONE_PROPERTY_FIRE            (1<<4)
#define CONCORDD_ZONE_PROPERTY_RESTORAL        (1<<5)
#define CONCORDD_ZONE_PROPERTY_SUPERVISORY     (1<<6)
#define CONCORDD_ZONE_PROPERTY_CS_REPORT       (1<<7)
#define CONCORDD_ZONE_PROPERTY_CHIME           (1<<8)
#define CONCORDD_ZONE_PROPERTY_DELAY           (1<<9)
#define CONCORDD_ZONE_PROPERTY_LEVEL_1         (1<<10)
#define CONCORDD_ZONE_PROPERTY_LEVEL_2         (1<<11)
#define CONCORDD_ZONE_PROPERTY_LEVEL_3         (1<<12)
#define CONCORDD_ZONE_PROPERTY_PANIC           (1<<13)

int
concordd_zone_group_get_properties(int group)
{
	int group_prop[] = {
		[0] = CONCORDD_ZONE_PROPERTY_PANIC
			| CONCORDD_ZONE_PROPERTY_SUPERVISORY
			| CONCORDD_ZONE_PROPERTY_CS_REPORT
			| CONCORDD_ZONE_PROPERTY_POLICE
			| CONCORDD_ZONE_PROPERTY_LEVEL_1
			| CONCORDD_ZONE_PROPERTY_LEVEL_2
			| CONCORDD_ZONE_PROPERTY_LEVEL_3,
		[1] = CONCORDD_ZONE_PROPERTY_PANIC
			| CONCORDD_ZONE_PROPERTY_CS_REPORT
			| CONCORDD_ZONE_PROPERTY_POLICE
			| CONCORDD_ZONE_PROPERTY_LEVEL_1
			| CONCORDD_ZONE_PROPERTY_LEVEL_2
			| CONCORDD_ZONE_PROPERTY_LEVEL_3,
		[2] = CONCORDD_ZONE_PROPERTY_PANIC
			| CONCORDD_ZONE_PROPERTY_SUPERVISORY
			| CONCORDD_ZONE_PROPERTY_CS_REPORT
			| CONCORDD_ZONE_PROPERTY_LEVEL_1
			| CONCORDD_ZONE_PROPERTY_LEVEL_2
			| CONCORDD_ZONE_PROPERTY_LEVEL_3,
		[3] = CONCORDD_ZONE_PROPERTY_PANIC
			| CONCORDD_ZONE_PROPERTY_CS_REPORT
			| CONCORDD_ZONE_PROPERTY_LEVEL_1
			| CONCORDD_ZONE_PROPERTY_LEVEL_2
			| CONCORDD_ZONE_PROPERTY_LEVEL_3,
		[4] = CONCORDD_ZONE_PROPERTY_PANIC
			| CONCORDD_ZONE_PROPERTY_CS_REPORT
			| CONCORDD_ZONE_PROPERTY_SUPERVISORY
			| CONCORDD_ZONE_PROPERTY_AUXILIARY
			| CONCORDD_ZONE_PROPERTY_LEVEL_1
			| CONCORDD_ZONE_PROPERTY_LEVEL_2
			| CONCORDD_ZONE_PROPERTY_LEVEL_3,
		[5] = CONCORDD_ZONE_PROPERTY_PANIC
			| CONCORDD_ZONE_PROPERTY_CS_REPORT
			| CONCORDD_ZONE_PROPERTY_SUPERVISORY
			| CONCORDD_ZONE_PROPERTY_AUXILIARY
			| CONCORDD_ZONE_PROPERTY_LEVEL_1
			| CONCORDD_ZONE_PROPERTY_LEVEL_2
			| CONCORDD_ZONE_PROPERTY_LEVEL_3,
		[6] = CONCORDD_ZONE_PROPERTY_PANIC
			| CONCORDD_ZONE_PROPERTY_CS_REPORT
			| CONCORDD_ZONE_PROPERTY_AUXILIARY
			| CONCORDD_ZONE_PROPERTY_LEVEL_1
			| CONCORDD_ZONE_PROPERTY_LEVEL_2
			| CONCORDD_ZONE_PROPERTY_LEVEL_3,
		[7] = CONCORDD_ZONE_PROPERTY_PANIC
			| CONCORDD_ZONE_PROPERTY_CS_REPORT
			| CONCORDD_ZONE_PROPERTY_AUXILIARY
			| CONCORDD_ZONE_PROPERTY_LEVEL_1
			| CONCORDD_ZONE_PROPERTY_LEVEL_2
			| CONCORDD_ZONE_PROPERTY_LEVEL_3,
		[8] = CONCORDD_ZONE_PROPERTY_RESTORAL
			| CONCORDD_ZONE_PROPERTY_SUPERVISORY
			| CONCORDD_ZONE_PROPERTY_CS_REPORT
			| CONCORDD_ZONE_PROPERTY_POLICE
			| CONCORDD_ZONE_PROPERTY_LEVEL_1
			| CONCORDD_ZONE_PROPERTY_LEVEL_2
			| CONCORDD_ZONE_PROPERTY_LEVEL_3,
		[9] = CONCORDD_ZONE_PROPERTY_RESTORAL
			| CONCORDD_ZONE_PROPERTY_SUPERVISORY
			| CONCORDD_ZONE_PROPERTY_CS_REPORT
			| CONCORDD_ZONE_PROPERTY_POLICE
			| CONCORDD_ZONE_PROPERTY_DELAY
			| CONCORDD_ZONE_PROPERTY_LEVEL_1
			| CONCORDD_ZONE_PROPERTY_LEVEL_2
			| CONCORDD_ZONE_PROPERTY_LEVEL_3,
		[10] = CONCORDD_ZONE_PROPERTY_RESTORAL
			| CONCORDD_ZONE_PROPERTY_EXTERIOR
			| CONCORDD_ZONE_PROPERTY_SUPERVISORY
			| CONCORDD_ZONE_PROPERTY_CS_REPORT
			| CONCORDD_ZONE_PROPERTY_POLICE
			| CONCORDD_ZONE_PROPERTY_DELAY
			| CONCORDD_ZONE_PROPERTY_CHIME
			| CONCORDD_ZONE_PROPERTY_LEVEL_2
			| CONCORDD_ZONE_PROPERTY_LEVEL_3,
		[11] = CONCORDD_ZONE_PROPERTY_RESTORAL
			| CONCORDD_ZONE_PROPERTY_EXTERIOR
			| CONCORDD_ZONE_PROPERTY_SUPERVISORY
			| CONCORDD_ZONE_PROPERTY_CS_REPORT
			| CONCORDD_ZONE_PROPERTY_POLICE
			| CONCORDD_ZONE_PROPERTY_DELAY
			| CONCORDD_ZONE_PROPERTY_CHIME
			| CONCORDD_ZONE_PROPERTY_LEVEL_2
			| CONCORDD_ZONE_PROPERTY_LEVEL_3,
		[12] = CONCORDD_ZONE_PROPERTY_RESTORAL
			| CONCORDD_ZONE_PROPERTY_EXTERIOR
			| CONCORDD_ZONE_PROPERTY_SUPERVISORY
			| CONCORDD_ZONE_PROPERTY_CS_REPORT
			| CONCORDD_ZONE_PROPERTY_POLICE
			| CONCORDD_ZONE_PROPERTY_DELAY
			| CONCORDD_ZONE_PROPERTY_CHIME
			| CONCORDD_ZONE_PROPERTY_LEVEL_2
			| CONCORDD_ZONE_PROPERTY_LEVEL_3,
		[13] = CONCORDD_ZONE_PROPERTY_RESTORAL
			| CONCORDD_ZONE_PROPERTY_EXTERIOR
			| CONCORDD_ZONE_PROPERTY_SUPERVISORY
			| CONCORDD_ZONE_PROPERTY_CS_REPORT
			| CONCORDD_ZONE_PROPERTY_POLICE
			| CONCORDD_ZONE_PROPERTY_CHIME
			| CONCORDD_ZONE_PROPERTY_LEVEL_2
			| CONCORDD_ZONE_PROPERTY_LEVEL_3,
		[14] = CONCORDD_ZONE_PROPERTY_RESTORAL
			| CONCORDD_ZONE_PROPERTY_INTERIOR
			| CONCORDD_ZONE_PROPERTY_SUPERVISORY
			| CONCORDD_ZONE_PROPERTY_CS_REPORT
			| CONCORDD_ZONE_PROPERTY_POLICE
			| CONCORDD_ZONE_PROPERTY_FOLLOWER
			| CONCORDD_ZONE_PROPERTY_LEVEL_2
			| CONCORDD_ZONE_PROPERTY_LEVEL_3,
		[15] = CONCORDD_ZONE_PROPERTY_INTERIOR
			| CONCORDD_ZONE_PROPERTY_SUPERVISORY
			| CONCORDD_ZONE_PROPERTY_CS_REPORT
			| CONCORDD_ZONE_PROPERTY_POLICE
			| CONCORDD_ZONE_PROPERTY_FOLLOWER
			| CONCORDD_ZONE_PROPERTY_LEVEL_2
			| CONCORDD_ZONE_PROPERTY_LEVEL_3,
		[16] = CONCORDD_ZONE_PROPERTY_RESTORAL
			| CONCORDD_ZONE_PROPERTY_INTERIOR
			| CONCORDD_ZONE_PROPERTY_SUPERVISORY
			| CONCORDD_ZONE_PROPERTY_CS_REPORT
			| CONCORDD_ZONE_PROPERTY_POLICE
			| CONCORDD_ZONE_PROPERTY_FOLLOWER
			| CONCORDD_ZONE_PROPERTY_LEVEL_3,
		[17] = CONCORDD_ZONE_PROPERTY_INTERIOR
			| CONCORDD_ZONE_PROPERTY_SUPERVISORY
			| CONCORDD_ZONE_PROPERTY_CS_REPORT
			| CONCORDD_ZONE_PROPERTY_POLICE
			| CONCORDD_ZONE_PROPERTY_FOLLOWER
			| CONCORDD_ZONE_PROPERTY_LEVEL_3,
		[18] = CONCORDD_ZONE_PROPERTY_INTERIOR
			| CONCORDD_ZONE_PROPERTY_SUPERVISORY
			| CONCORDD_ZONE_PROPERTY_CS_REPORT
			| CONCORDD_ZONE_PROPERTY_POLICE
			| CONCORDD_ZONE_PROPERTY_FOLLOWER
			| CONCORDD_ZONE_PROPERTY_LEVEL_3,
		[19] = CONCORDD_ZONE_PROPERTY_RESTORAL
			| CONCORDD_ZONE_PROPERTY_INTERIOR
			| CONCORDD_ZONE_PROPERTY_SUPERVISORY
			| CONCORDD_ZONE_PROPERTY_CS_REPORT
			| CONCORDD_ZONE_PROPERTY_POLICE
			| CONCORDD_ZONE_PROPERTY_DELAY
			| CONCORDD_ZONE_PROPERTY_LEVEL_3,
		[20] = CONCORDD_ZONE_PROPERTY_INTERIOR
			| CONCORDD_ZONE_PROPERTY_SUPERVISORY
			| CONCORDD_ZONE_PROPERTY_CS_REPORT
			| CONCORDD_ZONE_PROPERTY_POLICE
			| CONCORDD_ZONE_PROPERTY_DELAY
			| CONCORDD_ZONE_PROPERTY_LEVEL_3,
		[21] = CONCORDD_ZONE_PROPERTY_RESTORAL
			| CONCORDD_ZONE_PROPERTY_INTERIOR
			| CONCORDD_ZONE_PROPERTY_SUPERVISORY
			| CONCORDD_ZONE_PROPERTY_POLICE
			| CONCORDD_ZONE_PROPERTY_LEVEL_1
			| CONCORDD_ZONE_PROPERTY_LEVEL_2
			| CONCORDD_ZONE_PROPERTY_LEVEL_3,
		[22] = CONCORDD_ZONE_PROPERTY_RESTORAL
			| CONCORDD_ZONE_PROPERTY_INTERIOR
			| CONCORDD_ZONE_PROPERTY_SUPERVISORY
			| CONCORDD_ZONE_PROPERTY_POLICE
			| CONCORDD_ZONE_PROPERTY_DELAY
			| CONCORDD_ZONE_PROPERTY_LEVEL_1
			| CONCORDD_ZONE_PROPERTY_LEVEL_2
			| CONCORDD_ZONE_PROPERTY_LEVEL_3,
		[23] = CONCORDD_ZONE_PROPERTY_RESTORAL
			| CONCORDD_ZONE_PROPERTY_INTERIOR
			| CONCORDD_ZONE_PROPERTY_SUPERVISORY
			| CONCORDD_ZONE_PROPERTY_AUXILIARY
			| CONCORDD_ZONE_PROPERTY_LEVEL_1
			| CONCORDD_ZONE_PROPERTY_LEVEL_2
			| CONCORDD_ZONE_PROPERTY_LEVEL_3,
		[24] = CONCORDD_ZONE_PROPERTY_RESTORAL
			| CONCORDD_ZONE_PROPERTY_INTERIOR
			| CONCORDD_ZONE_PROPERTY_SUPERVISORY
			| CONCORDD_ZONE_PROPERTY_AUXILIARY
			| CONCORDD_ZONE_PROPERTY_DELAY
			| CONCORDD_ZONE_PROPERTY_LEVEL_1
			| CONCORDD_ZONE_PROPERTY_LEVEL_2
			| CONCORDD_ZONE_PROPERTY_LEVEL_3,
		[25] = CONCORDD_ZONE_PROPERTY_SUPERVISORY
			| CONCORDD_ZONE_PROPERTY_CHIME
			| CONCORDD_ZONE_PROPERTY_LEVEL_1
			| CONCORDD_ZONE_PROPERTY_LEVEL_2
			| CONCORDD_ZONE_PROPERTY_LEVEL_3,
		[26] = CONCORDD_ZONE_PROPERTY_RESTORAL
			| CONCORDD_ZONE_PROPERTY_INTERIOR
			| CONCORDD_ZONE_PROPERTY_SUPERVISORY
			| CONCORDD_ZONE_PROPERTY_FIRE
			| CONCORDD_ZONE_PROPERTY_CS_REPORT
			| CONCORDD_ZONE_PROPERTY_LEVEL_1
			| CONCORDD_ZONE_PROPERTY_LEVEL_2
			| CONCORDD_ZONE_PROPERTY_LEVEL_3,
		[27] = CONCORDD_ZONE_PROPERTY_RESTORAL
			| CONCORDD_ZONE_PROPERTY_SUPERVISORY
			| CONCORDD_ZONE_PROPERTY_LEVEL_1
			| CONCORDD_ZONE_PROPERTY_LEVEL_2
			| CONCORDD_ZONE_PROPERTY_LEVEL_3,
		[28] = CONCORDD_ZONE_PROPERTY_SUPERVISORY
			| CONCORDD_ZONE_PROPERTY_LEVEL_1
			| CONCORDD_ZONE_PROPERTY_LEVEL_2
			| CONCORDD_ZONE_PROPERTY_LEVEL_3,
		[29] = CONCORDD_ZONE_PROPERTY_RESTORAL
			| CONCORDD_ZONE_PROPERTY_SUPERVISORY
			| CONCORDD_ZONE_PROPERTY_AUXILIARY
			| CONCORDD_ZONE_PROPERTY_CS_REPORT
			| CONCORDD_ZONE_PROPERTY_LEVEL_1
			| CONCORDD_ZONE_PROPERTY_LEVEL_2
			| CONCORDD_ZONE_PROPERTY_LEVEL_3,
		[32] = CONCORDD_ZONE_PROPERTY_LEVEL_1
			| CONCORDD_ZONE_PROPERTY_LEVEL_2
			| CONCORDD_ZONE_PROPERTY_LEVEL_3,
		[33] = CONCORDD_ZONE_PROPERTY_SUPERVISORY
			| CONCORDD_ZONE_PROPERTY_CS_REPORT
			| CONCORDD_ZONE_PROPERTY_LEVEL_1
			| CONCORDD_ZONE_PROPERTY_LEVEL_2
			| CONCORDD_ZONE_PROPERTY_LEVEL_3,
		[34] = CONCORDD_ZONE_PROPERTY_RESTORAL
			| CONCORDD_ZONE_PROPERTY_SUPERVISORY
			| CONCORDD_ZONE_PROPERTY_AUXILIARY
			| CONCORDD_ZONE_PROPERTY_CS_REPORT
			| CONCORDD_ZONE_PROPERTY_LEVEL_1
			| CONCORDD_ZONE_PROPERTY_LEVEL_2
			| CONCORDD_ZONE_PROPERTY_LEVEL_3,
		[35] = CONCORDD_ZONE_PROPERTY_SUPERVISORY
			| CONCORDD_ZONE_PROPERTY_CS_REPORT
			| CONCORDD_ZONE_PROPERTY_POLICE
			| CONCORDD_ZONE_PROPERTY_RESTORAL
			| CONCORDD_ZONE_PROPERTY_LEVEL_1
			| CONCORDD_ZONE_PROPERTY_LEVEL_2
			| CONCORDD_ZONE_PROPERTY_LEVEL_3,
		[38] = CONCORDD_ZONE_PROPERTY_SUPERVISORY
			| CONCORDD_ZONE_PROPERTY_CS_REPORT
			| CONCORDD_ZONE_PROPERTY_AUXILIARY
			| CONCORDD_ZONE_PROPERTY_RESTORAL
			| CONCORDD_ZONE_PROPERTY_LEVEL_1
			| CONCORDD_ZONE_PROPERTY_LEVEL_2
			| CONCORDD_ZONE_PROPERTY_LEVEL_3,
	};
	if (group >= 0 && group < (sizeof(group_prop)/sizeof(group_prop[0]))) {
		return group_prop[group];
	}
	return 0;
}

const char*
concordd_zone_group_get_name(int group)
{
	const char* group_name[] = {
		[0] = "Fixed panic",
		[1] = "Portable panic",
		[2] = "Fixed silent panic",
		[3] = "Portable silent panic",
		[4] = "Fixed auxiliary",
		[5] = "Fixed auxiliary w/confirm",
		[6] = "Portable auxiliary panic",
		[7] = "Portable auxiliary panic w/confirm",
		[8] = "Special intrusion",
		[9] = "Special intrusion w/delay",
		[10] = "Entry/exit door w/delay",
		[11] = "Entry/exit door w/ext-delay",
		[12] = "Entry/exit door w/2ext-delay",
		[13] = "Exterior door/window",
		[14] = "Interior door/window",
		[15] = "Interior motion",
		[16] = "Interior door",
		[17] = "Interior motion",
		[18] = "Interior motion (cross-zone)",
		[19] = "Interior door w/delay",
		[20] = "Interior motion w/delay",
		[21] = "Local interior",
		[22] = "Local interior w/delay",
		[23] = "Local auxiliary",
		[24] = "Local auxiliary (siren off at restoral)",
		[25] = "Local special chime",
		[26] = "Fire",
		[27] = "Hardware Output Module",
		[28] = "Hardware Output Module",
		[29] = "Freeze sensor",
		[32] = "Hardware Output Module",
		[33] = "Wireless siren supervision",
		[34] = "Carbon monoxide gas detector",
		[35] = "Local police",
		[38] = "Water sensor",
	};
	if (group >= 0 && group < (sizeof(group_name)/sizeof(group_name[0]))) {
		return group_name[group];
	}
	return NULL;
}
