/*
 *
 *    Copyright (c) 2010-2012 Nest Labs, Inc.
 *    All rights reserved.
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
 *    Description:
 *		This is the header for the configuration file parser.
 *
 */

#ifndef __CONFIG_FILE_H__
#define __CONFIG_FILE_H__ 1

__BEGIN_DECLS
extern char* get_next_arg(char *buf, char **rest);

typedef int (*config_param_set_func)(void* context, const char* key, const char* value);

extern int read_config(const char* filename, config_param_set_func setter, void* context);
__END_DECLS

#endif // __CONFIG_FILE_H__
