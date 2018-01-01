#!/bin/sh
#
# Copyright (c) 2016 Nest Labs, Inc.
# All rights reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#    http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

PREV_PATH="`pwd`"

die() {
	echo " *** ERROR: " $*
	exit 1
}

set -x

[ -e configure ] || ./bootstrap.sh || die

mkdir -p "${BUILD_MAKEPATH}" || die

cd "${BUILD_MAKEPATH}" || die

../configure ${BUILD_CONFIGFLAGS} || die

make ${BUILD_MAKEARGS} || die

cd ..
