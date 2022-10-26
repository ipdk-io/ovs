# Copyright (c) 2022 Intel Corporation.
# SPDX-License-Identifier: Apache-2.0
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

"""
ECMP HotPlug test with 2VM on local host with P4OVS and 2 NS VM on remote host with Stand OVS
"""

# in-built module imports
import time
import sys
from itertools import dropwhile

# Unittest related imports
import unittest

# ptf related imports
import ptf
import ptf.dataplane as dataplane
from ptf.base_tests import BaseTest
from ptf.testutils import *
from ptf import config

# scapy related imports
from scapy.packet import *
from scapy.fields import *
from scapy.all import *

# framework related imports
import common.utils.ovsp4ctl_utils as ovs_p4ctl
import common.utils.test_utils as test_utils
import common.utils.ovs_utils as ovs_utils
import common.utils.gnmi_cli_utils as gnmi_cli_utils
from common.utils.config_file_utils import get_config_dict, get_gnmi_params_simple, get_interface_ipv4_dict, get_gnmi_params_hotplug,get_interface_ipv4_dict_hotplug
from common.lib.telnet_connection import connectionManager
import common.utils.tcpdump_utils as tcpdump_utils

class LNT_ECMP_2VM_Hotplug(BaseTest):

    def setUp(self):
        BaseTest.setUp(self)
        self.result = unittest.TestResult()
        test_params = test_params_get()
        config_json = test_params['config_json']

        try:
            self.vm_cred = test_params['vm_cred']
        except KeyError:
            self.vm_cred = ""

        self.config_data = get_config_dict(config_json,pci_bdf=test_params['pci_bdf'],
                    vm_location_list=test_params['vm_location_list'],
                          vm_cred=self.vm_cred, client_cred=test_params['client_cred'],
                                              remote_port = test_params['remote_port']) 
        self.gnmicli_params = get_gnmi_params_simple(self.config_data)
        self.gnmicli_hotplug_params = get_gnmi_params_hotplug(self.config_data)
        self.tap_port_list =  gnmi_cli_utils.get_tap_port_list(self.config_data)
        self.link_port_list = gnmi_cli_utils.get_link_port_list(self.config_data)
        self.interface_ip_list = get_interface_ipv4_dict(self.config_data)
        self.conn_obj_list = []

    def runTest(self):
        # Generate P4c artifacts
        if not test_utils.gen_dep_files_p4c_dpdk_pna_ovs_pipeline_builder(self.config_data):
            self.result.addFailure(self, sys.exc_info())
            self.fail("Failed to generate P4C artifacts or pb.bin")

        # Create VMs
        result, vm_name = test_utils.vm_create_with_hotplug(self.config_data)
        if not result:
            self.result.addFailure(self, sys.exc_info())
            self.fail(f"VM creation failed for {vm_name}")

        # Create telnet instance for VMs created
        self.conn_obj_list = []
        vm_id = 0
        for vm in self.config_data['vm']:
           globals()["conn"+str(vm_id+1)] = connectionManager("127.0.0.1", f"655{vm_id}", vm['vm_username'], vm['vm_password'])
           self.conn_obj_list.append(globals()["conn"+str(vm_id+1)])
           vm_id += 1

        # Get the list of interfaces on VMs
        vm1_command_list = ["ip a | egrep \"[0-9]*: \" | cut -d ':' -f 2"]
        result = test_utils.sendCmd_and_recvResult(conn1, vm1_command_list)[0]
        result = result.split("\n")
        vm1result1 = list(dropwhile(lambda x: 'lo\r' not in x, result))
        vm2_command_list = ["ip a | egrep \"[0-9]*: \" | cut -d ':' -f 2"]
        result = test_utils.sendCmd_and_recvResult(conn2, vm2_command_list)[0]
        result = result.split("\n")
        vm2result1 = list(dropwhile(lambda x: 'lo\r' not in x, result))

        # Create ports using gnmi cli
        if not gnmi_cli_utils.gnmi_cli_set_and_verify(self.gnmicli_params):
            self.result.addFailure(self, sys.exc_info())
            self.fail("Failed to configure gnmi cli ports")

        # Hotplug port to VM
        if not gnmi_cli_utils.gnmi_cli_set_and_verify(self.gnmicli_hotplug_params):
            self.result.addFailure(self, sys.exc_info())
            self.fail("Failed to configure hotplug through gnmi")

        # Verify ports are added to VM
        result = test_utils.sendCmd_and_recvResult(conn1, vm1_command_list)[0]
        result = result.split("\n")
        vm1result2 = list(dropwhile(lambda x: 'lo\r' not in x, result))
        result = test_utils.sendCmd_and_recvResult(conn2, vm2_command_list)[0]
        result = result.split("\n")
        vm2result2 = list(dropwhile(lambda x: 'lo\r' not in x, result))

        vm1interfaces = list(set(vm1result2) - set(vm1result1))
        vm1interfaces = [x.strip() for x in vm1interfaces]
        vm2interfaces = list(set(vm2result2) - set(vm2result1))
        vm2interfaces = [x.strip() for x in vm2interfaces]

        if not vm1interfaces:
            print("FAIL: Hotplug add failed for vm1")
            self.result.addFailure(self, sys.exc_info())
            self.fail("Fail to add hotplug through gnmi")
        print("PASS: Added hotplug interface for vm1 ",vm1interfaces)
        if not vm2interfaces:
            print("FAIL: Hotplug add failed for vm2")
            self.result.addFailure(self, sys.exc_info())
            self.fail("Fail to add hotplug through gnmi")
        print("PASS: Added hotplug interface for vm2 ",vm2interfaces)

        # Configuring VMs
        vm_cmd_list = []
        vm_id = 0
        for vm, port,intf in zip(self.config_data['vm'], self.config_data['port'], [vm1interfaces[0], vm2interfaces[0]]):
            globals()["vm"+str(vm_id+1)+"_command_list"] = [f"ip addr add {port['ip']} dev {intf}", f"ip link set dev {intf} up", f"ip link set dev {intf} address {port['mac_local']}"]
            vm_cmd_list.append(globals()["vm"+str(vm_id+1)+"_command_list"])
            vm_id += 1
        for i in range(len(self.conn_obj_list)):
            print ("Configuring VM....")
            test_utils.configure_vm(self.conn_obj_list[i], vm_cmd_list[i])

        # Bring up TAP ports 
        if not gnmi_cli_utils.ip_set_dev_up(self.tap_port_list[0]):
            self.result.addFailure(self, sys.exc_info())
            self.fail("Failed to bring up {self.tap_port_list[0]}")

        # configure IP on TEP and TAP ports
        if not gnmi_cli_utils.iplink_add_dev(self.config_data['vxlan']['tep_intf'], "dummy"):
                self.result.addFailure(self, sys.exc_info())
                self.fail(f"Failed to add dev {self.config_data['vxlan']['tep_intf']} type dummy")
        gnmi_cli_utils.ip_set_ipv4([{self.config_data['vxlan']['tep_intf']: self.config_data['vxlan']['tep_ip'][0]}])
        ecmp_local_ports = {}
        for i in range(len(self.config_data['ecmp']['local_ports'])):
            ecmp_local_ports[self.config_data['ecmp']['local_ports'][i]] =  self.config_data['ecmp']['local_ports_ip'][i]
        gnmi_cli_utils.ip_set_ipv4([ecmp_local_ports])

        # Set pipe line
        if not ovs_p4ctl.ovs_p4ctl_set_pipe(self.config_data['switch'], 
                                          self.config_data['pb_bin'], self.config_data['p4_info']):
            self.result.addFailure(self, sys.exc_info())
            self.fail("Failed to set pipe")

        # Create bridge
        if not ovs_utils.add_bridge_to_ovs(self.config_data['bridge']):
            self.result.addFailure(self, sys.exc_info())
            self.fail(f"Failed to add bridge {self.config_data['bridge']} to ovs")
        # Bring up bridge
        if not gnmi_cli_utils.ip_set_dev_up(self.config_data['bridge']):
            self.result.addFailure(self, sys.exc_info())
            self.fail(f"Failed to bring up {self.config_data['bridge']}")

        print (f"Configure VXLAN ")
        if not ovs_utils.add_vxlan_port_to_ovs(self.config_data['bridge'],
                self.config_data['vxlan']['vxlan_name'][0],
                    self.config_data['vxlan']['tep_ip'][0].split('/')[0], 
                        self.config_data['vxlan']['tep_ip'][1].split('/')[0],
                            self.config_data['vxlan']['dst_port'][0]):

            self.result.addFailure(self, sys.exc_info())
            self.fail(f"Failed to add vxlan {self.config_data['vxlan']['vxlan_name'][0]} to bridge {self.config_data['bridge']}")

        for i in range(len(self.conn_obj_list)):
            id = self.config_data['port'][i]['vlan']
            vlanname = "vlan"+id
            #add vlan to TAP0, e.g. ip link add link TAP0 name vlan1 type vlan id 1
            if not gnmi_cli_utils.iplink_add_vlan_port(id, vlanname, self.tap_port_list[0]):
                self.result.addFailure(self, sys.exc_info())
                self.fail(f"Failed to add vlan {vlanname} to {self.tap_port_list[0]}")

            #add vlan to the bridge, e.g. ovs-vsctl add-port br-int vlan1
            if not ovs_utils.add_vlan_to_bridge(self.config_data['bridge'], vlanname):
                self.result.addFailure(self, sys.exc_info())
                self.fail(f"Failed to add vlan {vlanname} to {self.config_data['bridge']}")

            #bring up vlan 
            if not gnmi_cli_utils.ip_set_dev_up(vlanname):
                self.result.addFailure(self, sys.exc_info())
                self.fail(f"Failed to bring up {vlanname}")

        # Program rules
        print (f"Program rules")
        for table in self.config_data['table']:
            print(f"Scenario : {table['description']}")
            print(f"Adding {table['description']} rules")
            for match_action in table['match_action']:
                if not ovs_p4ctl.ovs_p4ctl_add_entry(table['switch'],table['name'], match_action):
                    self.result.addFailure(self, sys.exc_info())
                    self.fail(f"Failed to add table entry {match_action}")

        # Remote host configuration Start 
        print (f"\nConfigure standard OVS on remote host {self.config_data['client_hostname']}")
        if not ovs_utils.add_bridge_to_ovs(self.config_data['bridge'], remote=True,
                hostname=self.config_data['client_hostname'],
                        username=self.config_data['client_username'],
                                passwd=self.config_data['client_password']):

            self.result.addFailure(self, sys.exc_info())
            self.fail(f"Failed to add bridge {self.config_data['bridge']} to \
                     ovs {self.config_data['bridge']} on {self.config_data['client_hostname']}" )

        # bring up the bridge
        if not gnmi_cli_utils.ip_set_dev_up(self.config_data['bridge'],remote=True,
                      hostname=self.config_data['client_hostname'],
                             username=self.config_data['client_username'],
                                    password=self.config_data['client_password']):

            self.result.addFailure(self, sys.exc_info())
            self.fail(f"Failed to bring up {self.config_data['bridge']}")

        # create ip netns VMs
        for namespace in self.config_data['net_namespace']: 
            print (f"creating namespace {namespace['name']} on {self.config_data['client_hostname']}")
            if not test_utils.create_ipnetns_vm(namespace, remote=True,
                hostname=self.config_data['client_hostname'],
                            username=self.config_data['client_username'],
                                    password=self.config_data['client_password']):

                self.result.addFailure(self, sys.exc_info())
                self.fail(f"Failed to add VM namesapce {namespace['name']} on on {self.config_data['client_hostname']}")

            if not ovs_utils.add_port_to_ovs(self.config_data['bridge'], namespace['peer_name'],
                    remote=True, hostname=self.config_data['client_hostname'],
                            username=self.config_data['client_username'],
                                                   password =self.config_data['client_password']):

                self.result.addFailure(self, sys.exc_info())
                self.fail(f"Failed to add port {namespace['peer_name']} to bridge {self.config_data['bridge']}")

        print (f"Configure vxlan port on remote host on {self.config_data['client_hostname']}")
        if not ovs_utils.add_vxlan_port_to_ovs(self.config_data['bridge'],
                self.config_data['vxlan']['vxlan_name'][0],
                    self.config_data['vxlan']['tep_ip'][1].split('/')[0], 
                        self.config_data['vxlan']['tep_ip'][0].split('/')[0],
                            self.config_data['vxlan']['dst_port'][0],remote=True, 
                                    hostname=self.config_data['client_hostname'],
                                        username=self.config_data['client_username'],
                                             password=self.config_data['client_password']):

            self.result.addFailure(self, sys.exc_info())
            self.fail(f"Failed to add vxlan {self.config_data['vxlan']['vxlan_name'][0]} to \
                         bridge {self.config_data['bridge']} on on {self.config_data['client_hostname']}")

        print (f"Add device TEP1")
        if not gnmi_cli_utils.iplink_add_dev("TEP1", "dummy",remote=True,
           hostname=self.config_data['client_hostname'],username=self.config_data['client_username'],
           password=self.config_data['client_password']):
             self.result.addFailure(self, sys.exc_info())
             self.fail(f"Failed to add TEP1")

        print (f"Bring up remote ports")
        remote_port_list = ["TEP1"] + self.config_data['remote_port']
        remote_port_ip_list = [self.config_data['vxlan']['tep_ip'][1]] + self.config_data['ecmp']['remote_ports_ip']
        for remote_port,remote_port_ip in zip(remote_port_list,remote_port_ip_list):
            if not gnmi_cli_utils.ip_set_dev_up(remote_port,remote=True, 
                        hostname=self.config_data['client_hostname'],
                            username=self.config_data['client_username'],
                                    password=self.config_data['client_password']):
               self.result.addFailure(self, sys.exc_info())
               self.fail(f"Failed to bring up {remote_port} on {self.config_data['client_hostname']}")
            if not gnmi_cli_utils.ip_add_addr(remote_port,remote_port_ip,remote=True,
              hostname=self.config_data['client_hostname'],username=self.config_data['client_username'],
              passwd=self.config_data['client_password']):
               self.result.addFailure(self, sys.exc_info())
               self.fail("Failed to configure IP {remote_port_ip} for {remote_port}")
        # Remote host configuration End

        #Ping test on underlay and overlay on local host
        dst = self.config_data['vxlan']['tep_ip'][0].split('/')[0]
        nexthop_list,device_list,weight_list = [],[],[]
        for i in self.config_data['ecmp']['local_ports_ip']:
            nexthop_list.append(i.split('/')[0])
            weight_list.append(1)
        for i in self.config_data['remote_port']:
            device_list.append(i.split('/')[0])
        if not gnmi_cli_utils.iproute_add(dst, nexthop_list, device_list, weight_list, remote=True,
            hostname=self.config_data['client_hostname'], username=self.config_data['client_username'],
            password=self.config_data['client_password']):
                self.result.addFailure(self, sys.exc_info())
                self.fail(f"Failed to add route")
        print("\nSleep before sending ping traffic")
        time.sleep(15)
        print (f"Ping test for underlay network")
        ip_list = []
        for i in self.config_data['ecmp']['remote_ports_ip']:
            ip_list.append(i.split('/')[0])
        for ip in ip_list:
            ping_cmd = f"ping {ip} -c 10"
            print(ping_cmd)
            if not test_utils.local_ping(ping_cmd):
               self.result.addFailure(self, sys.exc_info())
               self.fail(f"FAIL: Ping test failed for underlay network")
        # configure static routes for underlay 
        dst = self.config_data['vxlan']['tep_ip'][1].split('/')[0]
        nexthop_list,device_list,weight_list = [],[],[]
        for i in self.config_data['ecmp']['remote_ports_ip']:
            nexthop_list.append(i.split('/')[0])
            weight_list.append(1)
        for i in self.config_data['ecmp']['local_ports']:
            device_list.append(i.split('/')[0])
        if not gnmi_cli_utils.iproute_add(dst, nexthop_list, device_list, weight_list):
                self.result.addFailure(self, sys.exc_info())
                self.fail(f"Failed to add route")
        # ping remote tep
        ping_cmd = f"ping {self.config_data['vxlan']['tep_ip'][1].split('/')[0]} -c 10"
        print(ping_cmd)
        if not test_utils.local_ping(ping_cmd):
           self.result.addFailure(self, sys.exc_info())
           self.fail(f"FAIL: Ping test failed for underlay network")

        #overlay ping
        print (f"Ping test executed from VM on local host")
        for i in range(len(self.conn_obj_list)):
            print(f"Ping test from VM{i} to other VMs")
            for ip in self.config_data['vm'][i]['remote_ip']:
                if not test_utils.vm_to_vm_ping_test(self.conn_obj_list[i], ip):
                    self.result.addFailure(self, sys.exc_info())
                    self.fail(f"FAIL: Ping test failed for VM{i}")

        #Verify if the traffic is load balanced
        num = self.config_data['traffic']['number_pkts'][0]
        send_port_id= self.config_data['traffic']['send_port'][0]
        #Record port counter before sending traffic
        send_count_list_before = []
        for send_port_id in self.config_data['traffic']['send_port']:
            send_cont = gnmi_cli_utils.gnmi_get_params_counter(self.gnmicli_params[send_port_id])
            if not send_cont:
               self.result.addFailure(self, sys.exc_info())
               print (f"FAIL: unable to get counter of {self.config_data['port'][send_port_id]['name']}")
            send_count_list_before.append(send_cont)
        #Send ping traffic across ecmp links from VM
        print("Send ping traffic to verify load balancing")
        if not test_utils.vm_to_vm_ping_test(self.conn_obj_list[0], self.config_data['traffic']['in_pkt_header']['ip_dst_1'], count=num):
           self.result.addFailure(self, sys.exc_info())
           self.fail(f"FAIL: Ping test failed for VM{0}")
        #Record port counter after sending traffic
        send_count_list_after = []
        for send_port_id in self.config_data['traffic']['send_port']:
            send_cont = gnmi_cli_utils.gnmi_get_params_counter(self.gnmicli_params[send_port_id])
            if not send_cont:
               self.result.addFailure(self, sys.exc_info())
               print(f"FAIL: unable to get counter of {self.config_data['port'][send_port_id]['name']}")
            send_count_list_after.append(send_cont)
        #check if icmp pkts are forwarded on both ecmp links
        counter_type = "out-unicast-pkts"
        stat_total = 0
        for send_count_before,send_count_after in zip(send_count_list_before,send_count_list_after):
            stat = test_utils.compare_counter(send_count_after,send_count_before)
            if not stat[counter_type] > 0:
                print(f"FAIL: Packets are not forwarded on one of the ecmp links")
                self.result.addFailure(self, sys.exc_info())
            stat_total = stat_total + stat[counter_type]
        if stat_total >= num:
            print(f"PASS: Minimum {num} packets expected and {stat_total} received")
        else:
            print(f"FAIL: {num} packets expected but {stat_total} received")
            self.result.addFailure(self, sys.exc_info())

        print (f"close VM telnet session")
        for conn in self.conn_obj_list:
            conn.close()

    def tearDown(self):

        print("\nUnconfiguration on local host")
        print (f"Delete p4ovs match action rules on local host")
        for table in self.config_data['table']:
            print(f"Deleting {table['description']} rules")
            for del_action in table['del_action']:
                ovs_p4ctl.ovs_p4ctl_del_entry(table['switch'], table['name'], del_action)

        print (f"Delete vlan on local host")
        for i in range(len(self.conn_obj_list)):
            id = self.config_data['port'][i]['vlan']
            vlanname = "vlan"+id
            if not gnmi_cli_utils.iplink_del_port(vlanname):
                self.result.addFailure(self, sys.exc_info())
                self.fail(f"Failed to delete {vlanname}")

        print (f"Delete TEP interface")
        if not gnmi_cli_utils.iplink_del_port(self.config_data['vxlan']['tep_intf']):
            self.result.addFailure(self, sys.exc_info())
            self.fail(f"Failed to delete {self.config_data['vxlan']['tep_intf']}")

        print (f"Delete ip route")
        if not gnmi_cli_utils.iproute_del(self.config_data['vxlan']['tep_ip'][1].split('/')[0]):
            self.result.addFailure(self, sys.exc_info())
            self.fail(f"Failed to delete route to {self.config_data['vxlan']['tep_ip'][1].split('/')[0]}")

        print("\nUnconfiguration on remote host")
        print (f"Delete ip netns on remote host")
        for namespace in self.config_data['net_namespace']:
            # delete remote veth_host_vm port
            if not gnmi_cli_utils.iplink_del_port(namespace['peer_name'],remote=True,
                hostname=self.config_data['client_hostname'],
                        username=self.config_data['client_username'],
                                     passwd=self.config_data['client_password']):
                self.result.addFailure(self, sys.exc_info())
                self.fail(f"Failed to delete {namespace['peer_name']}")
                
            #delete name space
            if not test_utils.del_ipnetns_vm(namespace,remote=True,
                hostname=self.config_data['client_hostname'],
                            username=self.config_data['client_username'],
                                    password=self.config_data['client_password']):

                self.result.addFailure(self, sys.exc_info())
                self.fail(f"Failed to delete VM namesapce {namespace['name']} on {self.config_data['client_hostname']}")

        #remove local bridge
        if not ovs_utils.del_bridge_from_ovs(self.config_data['bridge']):
            self.result.addFailure(self, sys.exc_info())
            self.fail(f"Failed to delete bridge {self.config_data['bridge']} from ovs")

        #remote bridge
        if not ovs_utils.del_bridge_from_ovs(self.config_data['bridge'],
               remote=True,
                hostname=self.config_data['client_hostname'],
                            username=self.config_data['client_username'],
                                    passwd=self.config_data['client_password']):
            self.result.addFailure(self, sys.exc_info())
            self.fail(f"Failed to delete bridge {self.config_data['bridge']} on {self.config_data['client_hostname']}")
        
        # delete tep
        if not gnmi_cli_utils.iplink_del_port("TEP1",remote=True,hostname=self.config_data['client_hostname'],
           username=self.config_data['client_username'],passwd=self.config_data['client_password']):
            self.result.addFailure(self, sys.exc_info())
            self.fail(f"Failed to delete TEP1")

        # Delete route
        if not gnmi_cli_utils.iproute_del(self.config_data['vxlan']['tep_ip'][0].split('/')[0],remote=True,
          hostname=self.config_data['client_hostname'],username=self.config_data['client_username'],
          password=self.config_data['client_password']):
            self.result.addFailure(self, sys.exc_info())
            self.fail(f"Failed to delete route to {self.config_data['vxlan']['tep_ip'][0].split('/')[0]}")

        # Delete remote host Ip
        remote_port_list = self.config_data['remote_port']
        remote_port_ip_list = self.config_data['ecmp']['remote_ports_ip']
        for remote_port,remote_port_ip in zip(remote_port_list,remote_port_ip_list):
            if not gnmi_cli_utils.ip_del_addr(remote_port,remote_port_ip,remote=True,
              hostname=self.config_data['client_hostname'],username=self.config_data['client_username'],
              passwd=self.config_data['client_password']):
               self.result.addFailure(self, sys.exc_info())
               self.fail(f"Failed to delete ip {remote_port_ip} on {remote_port}")

        if self.result.wasSuccessful():
            print("Test has PASSED")
        else:
            print("Test has FAILED")