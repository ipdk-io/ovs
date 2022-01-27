#!/bin/bash
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

. os_ver_details.sh

if [ -z "$1" ]
then
    echo "- Missing mandatory arguments:"
    echo " - Usage: ./uninstall_dep_packages.sh <SRC_FOLDER> [INSTALL_FOLDER]"
    exit 1
fi

echo "#### \
THIS SCRIPT UNINSTALLS THE DEPENDENCY MODULES \
FROM THE CUSTOMIZED INSTALL PATH ONLY \
####"

#First argument is taken as the directory path \
#for the source code and installables scratch area.
SRC_DIR=$1/P4OVS_DEPS_SRC_CODE
WS_DIR=$PWD

if [ -z "$2" ];
then
    CMAKE_PREFIX=""
    MAKE_PREFIX=""
else
    INSTALL_DIR=$2/P4OVS_DEPS_INSTALL
    CMAKE_PREFIX="-DCMAKE_INSTALL_PREFIX=$INSTALL_DIR"
    MAKE_PREFIX="prefix=$INSTALL_DIR"
fi

#Get the OS and Version details
get_os_ver_details
echo "OS and Version details..."
echo "$OS : $VER"
echo ""
#Read the number of CPUs in a system and derive the NUM threads
get_num_cores
echo ""
echo "Number of Parallel threads used: $NUM_THREADS ..."
echo ""

#gflags uninstall
MODULE="gflags"
echo "####  Uninstalling the '$MODULE' module ####"
cd $SRC_DIR/$MODULE/build
sudo make uninstall
sudo ldconfig

#gtest uninstall
MODULE="googletest"
echo "####  Uninstalling the '$MODULE' module ####"
cd $SRC_DIR/
sudo rm -rf $MODULE
sudo ldconfig

#gmock-global uninstall
MODULE="gmock-global"
echo "####  Removing the '$MODULE' module ####"
cd $SRC_DIR
sudo rm -rf $MODULE 
sudo ldconfig

#glog uninstall
MODULE="glog"
echo "####  Uninstalling the '$MODULE' module ####"
cd $SRC_DIR/$MODULE/build
cat install_manifest.txt | xargs rm -rf
sudo ldconfig

#abseil-cpp uninstall
MODULE="abseil-cpp"
echo "####  Uninstalling the '$MODULE' module ####"
cd $SRC_DIR/$MODULE/build
cat install_manifest.txt | xargs rm -rf
sudo ldconfig

#cctz uninstall
MODULE="cctz"
echo "####  Uninstalling the '$MODULE' module ####"
cd $SRC_DIR/$MODULE/build
cat install_manifest.txt | xargs rm -rf
sudo ldconfig

#Protobuf uninstall
MODULE="protobuf"
echo "####  Uninstalling the '$MODULE' module ####"
cd ${SRC_DIR}/$MODULE
#If install_manifest file exists, previous installation was using cmake
if [ -d "build" ]
then
  FILE=./build/install_manifest.txt
  if [ -f $FILE ]
  then
    cd build && cat install_manifest.txt | xargs rm -rf
  fi
else
  sudo make uninstall
fi
sudo ldconfig

#grpc uninstall
MODULE="grpc"
echo "####  Uninstalling the '$MODULE' module ####"
#mkdir -p $SRC_DIR/$MODULE/build
cd ${SRC_DIR}/$MODULE
if [ -d "build" ]
then
  FILE=./build/install_manifest.txt
  if [ -f $FILE ]
  then
    cd build && cat install_manifest.txt | xargs rm -rf
  fi
else
#If grpc v1.17.2 is installed on the system, since 'make uninstall'
#is not supported. This is the work-around to get install file list
echo "Rebuilding grpc with cmake to uninstall grpc cleanly since make uninstall is unsupported"
  #GRPC cmake build has a dependency on golang
  if [[ $OS =~ "Fedora" ]]; then
      sudo dnf install -y golang
  elif [[ $OS =~ "Ubuntu" ]]; then
      sudo apt-get install -y golang
  else
      sudo yum install -y golang
  fi
  cd ${SRC_DIR}/$MODULE/third_party/boringssl
  #PATCH-01-GRPC-BORINGSSL is required to build crypto module of boringssl
  git apply $WS_DIR/external/PATCH-01-GRPC-BORINGSSL
  cd ${SRC_DIR}/$MODULE
  mkdir build
  cd build
  cmake -DgRPC_INSTALL=ON -DBUILD_TESTING=OFF $CMAKE_PREFIX ..
  make $NUM_THREADS
  sudo make $NUM_THREADS install
  cat install_manifest.txt | xargs rm -rf
  if [[ $OS =~ "Fedora" ]]; then
    sudo dnf remove -y golang
  elif [[ $OS =~ "Ubuntu" ]]; then
    sudo apt-get remove -y golang
  else
    sudo yum remove -y golang
  fi
fi
sudo ldconfig

#nlohmann uninstall
MODULE="json"
echo "####  Uninstalling the '$MODULE' module ####"
cd $SRC_DIR/$MODULE/build
cat install_manifest.txt | xargs rm -rf
sudo ldconfig

echo "Removing SOURCE and INSTALL scratch directories, $SRC_DIR and $INSTALL_DIR"
rm -rf $SRC_DIR
if [ "$2" ]
then
    rm -rf $INSTALL_DIR
fi

#Uninstall dependencies of netlink library
if [[ $OS =~ "Fedora" ]]; then
    sudo dnf remove -y pkgconfig
    sudo dnf remove -y libnl3-devel
elif [[ $OS =~ "Ubuntu" ]]; then
    sudo apt-get remove -y pkg-config
    sudo apt-get remove -y libnl-route-3-dev
else
    sudo yum remove -y pkgconfig
    sudo yum remove -y libnl3-devel
fi
