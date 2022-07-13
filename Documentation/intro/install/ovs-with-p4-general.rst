..
      Licensed under the Apache License, Version 2.0 (the "License"); you may
      not use this file except in compliance with the License. You may obtain
      a copy of the License at

          http://www.apache.org/licenses/LICENSE-2.0

      Unless required by applicable law or agreed to in writing, software
      distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
      WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
      License for the specific language governing permissions and limitations
      under the License.

      Convention for heading levels in Open vSwitch documentation:

      =======  Heading 0 (reserved for the title in a document)
      -------  Heading 1
      ~~~~~~~  Heading 2
      +++++++  Heading 3
      '''''''  Heading 4

      Avoid deeper levels because they do not render well.

====================
Open vSwitch with P4
====================

This document describes steps to pull latest P4-OvS code repository, build and
install on Ubuntu and Fedora 33 servers.

Pre-requisite
-------------
P4-OVS needs TDI library to be installed. To build P4 target Library (TDI) for
P4-DPDK, download and install from ``https://github.com/p4lang/p4-dpdk-target``.

Obtaining P4 Open vSwitch Sources
---------------------------------
The canonical location for P4 Open vSwitch source code is its Git
repository, which you can clone into a default directory named "P4-OVS" with::

    $ git clone https://github.com/ipdk-io/p4-ovs

Submodules
----------
P4-OvS intergates with multiple submodules as mentioned below::

    $ p4runtime
    $ stratum
    $ googleapis
    $ SAI

To update code from these submodules we need to execute command::

    $ git submodule update --init --recursive

Install python dependent packages as mentioned in P4-OVS::

    $ pip3 install -r Documentation/requirements.txt
    $ cd p4runtime/py ; python setup.py build ; python setup.py install_lib

Python utilitiy available in P4-OvS assumes to run on Python3. Need to install
below python dependent packages via pip3::

    $ pip3 install ovspy
    $ pip3 install Cython

Install Dependent packages::
----------------------------

To build and run P4-OvS there are few dependent packages that should be
installed on the server. These packages can be installed at pre-defined default
path available on the server or user can choose a customized path to pull
source code and install packages.

P4-OVS uses protobuf version 3.18.1 and grpc version 1.42.0 as a dependent package.
If older versions of these packages exist on the system, any remnants of old version
needs to be removed before installing new version in the next step. Refer to
'Uninstall Dependent packages' section for removing older versions of dependent packages.

User needs to execute command::

    $ ./install_dep_packages.sh <SRC_FOLDER> [INSTALL_FOLDER]

.. note::

    ``SRC_FOLDER``: This is a mandatory argument, refers to location where
    source code for dependent packages needs to be downloaded. Creates directory
    P4OVS_DEPS_SRC_CODE under SRC_FOLDER and downloads source code.
    ``INSTALL_FOLDER``: This is an optional argument, refers to location where
    to install dependent packages. Creates directory P4OVS_DEPS_INSTALL under
    INSTALL_FOLDER and installs dependent packages.

Build and Install P4-OvS
------------------------
All the commands referred in below sections need to be executed from top-level
directory within P4-OVS.

Option 1: Build and Install using a script
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
P4-OvS repository has a script `build-p4ovs.sh` which will update environment
variables, create dependent configuration files and build P4-OvS.
Command to run build script::

    $ ./build-p4ovs.sh <SDE_INSTALL> [P4OVS_DEPS_INSTALL]

.. note::

    ``SDE_INSTALL``: This is a mandatory argument, refers to location where TDI
    library is located and $SDE_INSTALL path is pointing to.
    ``P4OVS_DEPS_INSTALL``: This is an optional argument. But, if user has
    opted for customized path while executing install_dep_packages.sh, then
    it is exepcted to pass the absolute path of P4OVS_DEPS_INSTALL directory.

Option 2: Build and Install manually
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Instead of using the build-p4ovs.sh script, users can update environment
variables, create dependent configuration files and build P4-OvS manually.

    a) Steps to install environment variables::

        $ source p4ovs_env_setup.sh <SDE_INSTALL> [P4OVS_DEPS_INSTALL]

    b) Steps to create dependent configuration files::

        $ ./apply_stratum_artifacts.sh <SDE_INSTALL>

    c) Steps to build and install P4-OvS::

        $ ./boot.sh
        $ ./configure [--prefix=$P4OVS_DEPS_INSTALL] --with-p4tdi=$SDE_INSTALL
        $ make
        $ make install

Pre-requisite for DPDK target before running ovs-vswitchd
---------------------------------------------------------
Enable hugepages for DPDK::

    $ ./set_hugepages.sh

.. note::

   set_hugepages.sh script can be found in the ipdk repository under
   ipdk/build/scripts/

While running ovs-vswithd with P4, use --no-chir with --detach::

    $ Ex: ovs-vswitchd --pidfile --detach --no-chdir --mlockall \
          --log-file=/tmp/ovs-vswitchd.log

or alternatively use the script::

    $ ./run_ovs.sh [P4OVS_DEPS_INSTALL]

.. note::

    ``P4OVS_DEPS_INSTALL``: This is an optional argument. But, if user has
    opted for customized path while executing install_dep_packages.sh, then
    it is exepcted to pass the absolute path of P4OVS_DEPS_INSTALL directory.

Uninstall Dependent packages
----------------------------
Following command is used to delete the previously installed packages.

User needs to execute command::

    $ ./uninstall_dep_pacakges.sh <SRC_FOLDER> [INSTALL_FOLDER]

.. note::

    ``SRC_FOLDER``: This is a mandatory argument, refers to location where
    source code for dependent packages was downloaded. Deletes directory
    P4OVS_DEPS_SRC_CODE under SRC_FOLDER once all dependent packages are uninstalled.
    ``INSTALL_FOLDER``: This is an optional argument, refers to location where
    dependent packages were installed. Deletes directory P4OVS_DEPS_INSTALL under
    INSTALL_FOLDER once all dependent packages are uninstalled.

Limitations with DPDK target
----------------------------
When backend DPDK target is used, we have few limitations that are imposed by
the target::

    - Number of ports (vhost/link/TAP) created by the user for DPDK target
      should always be power of 2.
      Eg: 2, 4, ... 2^n
    - Port addition or creation for the target is not allowed once PIPELINE
      is loaded and enabled.
