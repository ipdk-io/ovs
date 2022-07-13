# Copyright (c) 2021 Intel Corporation.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at:
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#! /bin/bash
set -e

get_os_ver_details()
{
  if [ -f /etc/os-release ]; then
      # freedesktop.org and systemd
      # shellcheck source=/dev/null
      . /etc/os-release
      OS=$NAME
      VER=$VERSION_ID
  elif type lsb_release >/dev/null 2>&1; then
      # linuxbase.org
      OS=$(lsb_release -si)
      VER=$(lsb_release -sr)
  elif [ -f /etc/lsb-release ]; then
      # For some versions of Debian/Ubuntu without lsb_release command
      # shellcheck source=/dev/null
      . /etc/lsb-release
      OS=$DISTRIB_ID
      VER=$DISTRIB_RELEASE
  elif [ -f /etc/debian_version ]; then
      # Older Debian/Ubuntu/etc.
      OS=Debian
      VER=$(cat /etc/debian_version)
  else
      # Fall back to uname, e.g. "Linux <version>", also works for BSD, etc.
      OS=$(uname -s)
      export OS
      VER=$(uname -r)
      export VER
  fi
}

#
# NOTE: We set NUM_THREADS to NUM_CORES / 4 to ensure the build process doesn't overrun
#       the amount of memory on the build host. It's not an exact science, but it helps
#       prevent the OOM process from killing compilers and causing issues with the
#       build.
#
get_num_cores()
{
   # First check if NUM_CORES and NUM_THREADS are already set, and exit if so.
   if [[ -n $NUM_CORES && -n $NUM_THREADS ]]
   then
      echo "NUM_CORES and NUM_THREADS already set."
      return
   fi

   nproc_exist=$(command -v nproc)
   if [ -n "$nproc_exist" ];
   then
       NUM_CORES=$(nproc --all)
       echo "Num cores on a system: $NUM_CORES"
       if [ "$NUM_CORES" -gt 4 ]
       then
           NUM_THREADS=$((NUM_CORES / 4))
           NUM_THREADS=-j$NUM_THREADS
       else
           NUM_THREADS=-j${NUM_CORES}
       fi
    else
        NUM_CORES=1
        NUM_THREADS=-j1
    fi
}
