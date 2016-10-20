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

#ifndef concordd_dbus_server_h
#define concordd_dbus_server_h 1

#include "concordd.h"
#include "concordd-dbus.h"
#include "time-utils.h"
#include <sys/select.h>
#include <dbus/dbus.h>

struct concordd_dbus_server_s;
typedef struct concordd_dbus_server_s *concordd_dbus_server_t;

struct concordd_dbus_server_s {
    DBusConnection *dbus_connection;
    concordd_instance_t instance;
};

concordd_dbus_server_t concordd_dbus_server_init(concordd_dbus_server_t self, concordd_instance_t instance);

int concordd_dbus_server_process(concordd_dbus_server_t self);

int concordd_dbus_server_update_fd_set(concordd_dbus_server_t self, fd_set *read_fd_set, fd_set *write_fd_set, fd_set *error_fd_set, int *max_fd, cms_t *timeout);

void concordd_dbus_event_func(concordd_dbus_server_t self, concordd_instance_t instance, concordd_event_t event);
void concordd_dbus_system_info_changed_func(concordd_dbus_server_t self, concordd_instance_t instance, int changed);
void concordd_dbus_keyfob_button_pressed_func(concordd_dbus_server_t self, concordd_instance_t instance, concordd_partition_t partition, int button);
void concordd_dbus_partition_info_changed_func(concordd_dbus_server_t self, concordd_instance_t instance, concordd_partition_t partition, int changed);
void concordd_dbus_siren_sync_func(concordd_dbus_server_t self, concordd_instance_t instance);
void concordd_dbus_zone_info_changed_func(concordd_dbus_server_t self, concordd_instance_t instance, concordd_zone_t zone, int changed);
void concordd_dbus_light_info_changed_func(concordd_dbus_server_t self, concordd_instance_t instance, concordd_partition_t partition, concordd_light_t light, int changed);
void concordd_dbus_output_info_changed_func(concordd_dbus_server_t self, concordd_instance_t instance, concordd_output_t output, int changed);


#endif // ifndef concordd_dbus_server_h
