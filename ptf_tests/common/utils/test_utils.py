"""
Generic utility scripts for P4OVS PTF scripts.
"""

from ptf import *
from ptf.testutils import *
import common.lib.local_connection as local_connection

import json
import os
import subprocess
import re
import asyncio
import time

from common.lib.local_connection import Local
from common.lib.telnet_connection import connectionManager
from common.lib.ovs import Ovs
from common.utils.ovs_utils import get_connection_object

def add_port_to_dataplane(port_list):
    """
    To add device ports to dataplane database for ptf usage
    """
    local = Local()
    r,_,_ = local.execute_command("ip -j link show")
    result = json.loads(r)
    for name in port_list:
        for iface in result:
            if (iface["ifname"] == name):
                config["port_map"].update({(0,iface["ifindex"]):name})
                continue
    
    return ptf_ports() 

def get_port_name_from_pci_bdf(pci_bdf):
    """
    To return port name from pci_bdf
    """
    local = Local()
    r,_,_ = local.execute_command(f"ls -la /sys/class/net/ |grep -i {pci_bdf}")
    port_name = r.split()[8]
    
    return port_name


def gen_dep_files_p4c_ovs_pipeline_builder(config_data):
    """
    util function to generate p4 artifacts
    :params: config_data --> dict --> dictionary with all config data loaded from json
    :returns: Boolean True/False
    """
    local = Local()
    
    p4file = config_data['p4file']
    conf_file = p4file + ".conf"
    output_dir = os.sep.join(["common", "p4c_artifacts", p4file])
    pb_bin_file = config_data['p4file']+'.pb.bin'
    config_data['pb_bin'] = output_dir + "/" + pb_bin_file
    config_data['p4_info'] = output_dir + "/p4Info.txt"
    p4file = p4file + ".p4"
    cmd = f'''p4c --arch psa --target dpdk --output {output_dir}/pipe --p4runtime-files \
            {output_dir}/p4Info.txt --bf-rt-schema {output_dir}/bf-rt.json --context \
            {output_dir}/pipe/context.json {output_dir}/{p4file}'''

    out, returncode, err = local.execute_command(cmd)
    if returncode:
        print(f"Failed to run p4c: {out} {err}")
        return False

    print(f"PASS: {cmd}")

    cmd = f'''cd {output_dir}; ovs_pipeline_builder --p4c_conf_file={conf_file} \
            --bf_pipeline_config_binary_file={pb_bin_file}'''

    out, returncode, err = local.execute_command(cmd)
    if returncode:
        print(f"Failed to run ovs_pipeline_builder: {out} {err}")
        return False

    cmd = f'''ovs_pipeline_builder --p4c_conf_file={conf_file} \
            --bf_pipeline_config_binary_file={pb_bin_file}'''

    print(f"PASS: {cmd}")

    return True

def gen_dep_files_p4c_dpdk_pna_ovs_pipeline_builder(config_data):
    """
    util function to generate p4 artifacts for dpdk pna architecture
    :params: config_data --> dict --> dictionary with all config data loaded from json
    :returns: Boolean True/False
    """
    local = Local()

    p4file = config_data['p4file']
    conf_file = p4file + ".conf"
    output_dir = os.sep.join(["common", "p4c_artifacts", p4file])
    pb_bin_file = config_data['p4file']+'.pb.bin'
    config_data['pb_bin'] = output_dir + "/" + pb_bin_file
    config_data['p4_info'] = output_dir + "/p4Info.txt"
    spec_file = p4file + ".spec"
    p4file = p4file + ".p4"
    cmd = f'''p4c-dpdk -I p4include -I p4include/dpdk --p4v=16 --p4runtime-files \
            {output_dir}/p4Info.txt -o {output_dir}/pipe/{spec_file} --arch pna --bf-rt-schema {output_dir}/bf-rt.json --context \
            {output_dir}/pipe/context.json {output_dir}/{p4file}'''
   
    out, returncode, err = local.execute_command(cmd)
    if returncode:
        print(f"Failed to run p4c: {out} {err}")
        return False

    print(f"PASS: {cmd}")

    cmd = f'''cd {output_dir}; ovs_pipeline_builder --p4c_conf_file={conf_file} \
            --bf_pipeline_config_binary_file={pb_bin_file}'''

    out, returncode, err = local.execute_command(cmd)
    if returncode:
        print(f"Failed to run ovs_pipeline_builder: {out} {err}")
        return False

    cmd = f'''ovs_pipeline_builder --p4c_conf_file={conf_file} \
            --bf_pipeline_config_binary_file={pb_bin_file}'''

    print(f"PASS: {cmd}")

    return True

def qemu_version(ver="6.1.0"):
    """
    To Add/Del same Hotplug mutiple times need to check qemu version >= 6.1.0.
    Below 6.1.0  version functionality will fail.
    """
    local = Local()
    cmd = f'''qemu-system-x86_64 --version | head -1 | cut -d" " -f4'''
    print(cmd)

    out, returncode, err = local.execute_command(cmd)
    result = out.strip() >= ver
    if result:
        print(f"PASS: {cmd}")
        return out.strip()

    return False 

def vm_create(vm_location_list, memory="512M"):
    """
    To create VMs. Will stop the execution if anyone of VM creation gets failed.
    Usage eg : result, vm_name = vm_create(vm_location_list)

    """
    num_of_vms = len(vm_location_list)
    vm_list = []
    for i in range(num_of_vms):
        vm_name = f"VM{i}"
        vm_list.append(vm_name)

        cmd = f"(qemu-system-x86_64 -enable-kvm -smp 4 -m {memory} \
-boot c -cpu host -enable-kvm -nographic \
-L /root/pc-bios -name VM{i} \
-hda {vm_location_list[i]} \
-object memory-backend-file,id=mem,size={memory},mem-path=/dev/hugepages,share=on \
-mem-prealloc \
-numa node,memdev=mem \
-chardev socket,id=char{i},path=/tmp/vhost-user-{i} \
-netdev type=vhost-user,id=netdev{i},chardev=char{i},vhostforce \
-device virtio-net-pci,netdev=netdev{i} \
-serial telnet::655{i},server,nowait &)"

        time.sleep(5)
        p  = subprocess.Popen([cmd], shell=True, stdin=subprocess.PIPE, stdout=subprocess.PIPE)

        try:
            out, err = p.communicate(timeout=5)
            return False, f"VM{i}"

        except Exception as err:
            print(f"VM creation successful : VM{i}")

    return True,vm_list

def configure_vm(conn, command_list):
    """
    To login to VM instance and configure the VMs.
    """
    for cmd in command_list:
        status = conn.sendCmd(cmd)
        if status:
            print(f"Command: {cmd} executed ")
        else:
            print("Failed to execute command {cmd}")


def vm_to_vm_ping_test(conn, dst_ip, count="4"):
    """
    To test if ping to a destination works without any drop
    """
    cmd = f"ping {dst_ip} -c {count}"
    dummy_ping(conn, cmd)

    conn.sendCmd(cmd)
    result = conn.readResult()
    pkt_loss = 100
    if result:
        match = re.search('(\d*)% packet loss', result)
        if match:
            pkt_loss = int(match.group(1))

    if f"{count} received, 0% packet loss" in result:
        print(f"PASS: Ping successful to destination {dst_ip}")
        return True
    else:
        print(f"FAIL: Ping Failed to destination {dst_ip} with {pkt_loss}% loss")
        return False

def dummy_ping(conn, cmd):
    """
    Dummy traffic to ignore traffic drops dueto ARP learning etc
    """
    conn.sendCmd(cmd)
    conn.readResult()

def vm_to_vm_ping_drop_test(conn, dst_ip, count="4"):
    """
    To test if ping to a destination is getting failed.
    i.e, 100% drop expected.
    """
    cmd = f"ping {dst_ip} -c {count}"
    dummy_ping(conn, cmd)

    conn.sendCmd(cmd)
    result = conn.readResult()
    pkt_loss = "100"
    if result:
        match = re.search('(\d*)% packet loss', result)
        if match:
            pkt_loss = match.group(1)

    if pkt_loss == "100":
        print(f"PASS: 100% packet loss to destination {dst_ip}")
        return True
    else:
        print(f"FAIL: Ping to destination {dst_ip} works with {pkt_loss}% loss")
        return False


def vm_port_flapping(conn, config_data, result):
    """ Read live data from telnet connection
    :param conn: VM1 telnet instance
    :type conn: 'obj' type
    :param config_data: input configuration dictionary
    :type config_data: dict
    :param result: instance of unittest.TestResult() for test result logging
    :type result: 'obj' type
    :return: True or False
    :rtype: boolean
    """
    ping_cmd = f"ping -w 30 {config_data['vm'][0]['remote_ip']}"
    print(ping_cmd)
    res = conn.sendCmd(ping_cmd)
    if res:
        print('Traffic started from VM1 -> VM2')
    else:
        result.addFailure(sys.exc_info())
        print('failed to start ping')
        return False
    try:
        down_cmd = f"ip link set {config_data['port'][1]['interface']} " \
                   f"down"
        up_cmd = f"ip link set {config_data['port'][1]['interface']} up"
        lookup_string1 = f"64 bytes from {config_data['vm'][0]['remote_ip']}"
        while True:
            line = conn.tn.read_until(b"ms", 30).decode('utf-8')
            print(f'{line}')
            if lookup_string1 in line:
                print(f'Traffic is running successfully. '
                      f'Now bring VM2 interface down')
                if not conn.sendCmd(down_cmd):
                    result.addFailure(sys.exc_info())
                    print(f'Failed to bring VM2 interface down')
                lookup_string1 = "None"
                sys.stdout.flush()
                print(f'VM2 interface is down')
            elif down_cmd in line:
                print('No traffic is running, '
                      'bring VM2 interface up')
                if not conn.sendCmd(up_cmd):
                    result.addFailure(sys.exc_info())
                    print(f'Failed to bring VM2 interface up')
                lookup_string1 = f"64 bytes from {config_data['vm'][0]['remote_ip']}"
                down_cmd = "None"
                sys.stdout.flush()
            if lookup_string1 in line:
                print('traffic is resumed as expected, Port flapping is '
                      'successful')
                return True
    except Exception as err:
        print(f"Read CLI output failed with error: {err}")
        return False


def get_port_status(interface_ip_list):
    """Get port status using ethtool utility
    :param interface_ip_list: list of dict; An pair of interface with IP
    :type interface_ip_list: list [{},{}]
    :return: True or False
    :rtype: boolean
    """
    local = local_connection.Local()
    for interface_ipv4_dict in interface_ip_list:
        for interface, ip in interface_ipv4_dict.items():
            cmd = f'ethtool {interface} |grep "Link detected:" |cut -d " ' \
                  f'" -f3 '
            out, _, err = local.execute_command(cmd)
            out = str(out).rstrip('\n')
            if out == "yes":
                print(f'{interface} link detected')
            else:
                print(f'Failed to detect {interface}')
                return False
    return True

def vm_create_with_hotplug(config_data, memory="512M"):
    """
    To create VMs. Will stop the execution if anyone of VM creation gets failed.
    Usage eg : result, vm_name = vm_create(vm_location_list)

    """
    vm_list = []
    for vm in config_data['vm']:
        vm_name = vm['vm_name']
        vm_list.append(vm_name)

        cmd = f"(qemu-system-x86_64 -enable-kvm -smp 4 -m {memory} \
 -boot c -cpu host -enable-kvm -nographic \
 -L /root/pc-bios -name {vm_name} \
 -hda {vm['vm_image_location']} \
 -object memory-backend-file,id=mem,size={memory},mem-path=/dev/hugepages,share=on \
 -mem-prealloc \
 -numa node,memdev=mem \
 -monitor telnet::{vm['hotplug']['qemu-socket-port']},server,nowait \
 -serial telnet::{vm['hotplug']['serial-telnet-port']},server &)"
        
        print(cmd)
        time.sleep(5)

        p  = subprocess.Popen([cmd], shell=True, stdin=subprocess.PIPE, stdout=subprocess.PIPE)

        try:
            out, err = p.communicate(timeout=5)
            return False, f"VM{i}"

        except Exception as err:
            print(f"VM creation successful : {vm_name}")

    return True,vm_list

def sendCmd_and_recvResult(conn, command_list):
    """
    Sends Command and returns output
    """
    result=[]
    for cmd in command_list:
        status = conn.sendCmd(cmd)
        if status:
            print(f"Command: {cmd} executed ")
        else:
            print("Failed to execute command {cmd}")
        result.append(conn.readResult())

    return result

def vm_interface_up(conn, interface_ip_list):
    command_list=[]
    for interface_ipv4_dict in interface_ip_list:
        for interface, ip in interface_ipv4_dict.items():
            command_list.append(f"ip link set {interface} up")
    
    return sendCmd_and_recvResult(conn, command_list)

def vm_interface_configuration(conn, interface_ip_list):
    command_list=[]
    for interface_ipv4_dict in interface_ip_list:
        for interface, ip in interface_ipv4_dict.items():
            command_list.append(f"ip addr add {ip} dev {interface}")

    return sendCmd_and_recvResult(conn, command_list)

def vm_route_configuration(conn, interface, local_ip, remote_ip):
    local_ip = local_ip.split("/")[0] #stripping it of any /24 if any
    remote_ip = remote_ip.split("/")[0]

    cmd = f"ip route add {remote_ip}/24 via {local_ip} dev {interface}"
    return sendCmd_and_recvResult(conn, [cmd])

def vm_ip_neigh_configuration(conn, interface, remote_ip, remote_mac):
    remote_ip = remote_ip.split("/")[0] #stripping it of any /24 if any

    cmd = f"ip neigh add dev {interface} {remote_ip} lladdr {remote_mac}"
    return sendCmd_and_recvResult(conn, [cmd])

def set_telnet_conn_timeout(conn, timeout=10):
    conn.timeout=timeout

def compare_counter(counter2, counter1):
    delta =dict()
    for key in counter2.keys():
        delta[key] = counter2[key] -counter1[key]
    return delta

def ovs_add_ctrl_port_to_bridge(bridge, port_list, p4_device_id):
    """
    ovs-vsctl add-port BRIDGE CONTROL_PORT
    Example:
        ovs-vsctl add-p4-device 1
        ovs-vsctl add-br br1
        ovs-vsctl add-br-p4 br1 1
        ovs-vsctl add-port br1 TAP0
    """
    ovs = Ovs(get_connection_object())
    
    out, returncode, err = ovs.vsctl.add_p4_device(p4_device_id)
    if returncode:
        print(f"Failed to add p4_device {p4_device_id} in bridge {bridge} due to {out} {err}")
        return False
    out, returncode, err = ovs.vsctl.add_br(bridge)
    if returncode:
        print(f"Failed to add bridge {bridge} due to {out} {err}")
        return False
    out, returncode, err = ovs.vsctl.add_br_p4(bridge, 1)
    if returncode:
        print(f"Failed to add bridge {bridge} in p4 device due to {out} {err}")
        return False
    # adding port into ovs bridge
    for port in port_list:
        out, returncode, err = ovs.vsctl.add_port(bridge,port)
        if returncode:
            print(f"Failed to port {port} in bridge {bridge} {out} {err}")
            return False

    return True

def get_ovs_port_dump(bridge, ctrl_port_list):
    """
    ovs-ofctl dump-ports BRIDGE
    Example:
        ovs-ofctl dump-ports br1
    output example
        port LOCAL: rx pkts=22, bytes=1860, drop=0, errs=0, frame=0, over=0, crc=0
                tx pkts=8, bytes=656, drop=0, errs=0, coll=0
        port  1: rx pkts=0, bytes=0, drop=0, errs=0, frame=0, over=0, crc=0
                tx pkts=14, bytes=1204, drop=0, errs=0, coll=0
    """
    counter_dict = dict()
    out, returncode, err = Ovs(get_connection_object()).ofctl.dump_port(bridge)
 
    if not returncode and ("port" in out):
        out = out.split("\n")
        #skip headline, local port rx and local port tx
        out.pop(0); out.pop(0);out.pop(0)
    else:
        print (f"FAIL: unable to ovs-ofctl dump-ports {bridge} due to {err}")
        return False
   
    # Build control port counter dict
    for each in out:
        each = each.strip()
        if each.isspace() or len(each)==0:
            continue
        if "port" in each:
            port_id = int(each.split()[1].replace(":",""))
            rx = each.split()[2]
            ctrl_port_name = ctrl_port_list[port_id-1]
            counter_dict[ctrl_port_name] = dict()
            counter_dict[ctrl_port_name][rx]=dict()
            items=  each.split()[3:]
            #Build remaining counter
            for item in items:
                count_name = item.split("=")[0].strip()
                counter_dict[ctrl_port_name][rx][count_name] = dict()
                counter_dict[ctrl_port_name][rx][count_name] = int(item.split("=")[1].replace(",",""))
        else:
            tx = each.split(", ")[0].split()[0]
            counter_dict[ctrl_port_name][tx] = dict()
            # remmove prefix tx
            items = each.replace("tx", "").split()
            for item in items:
                count_name = item.split("=")[0].strip()
                counter_dict[ctrl_port_name][tx][count_name] = dict()
                counter_dict[ctrl_port_name][tx][count_name] = int(item.split("=")[1].replace(",",""))

    if counter_dict:
        return counter_dict
    else:
        return False

def get_control_port(config_data):
    ctrl_port = []
    for data in config_data['port']:
        if data["control-port"]:
            ctrl_port.append(data["control-port"])
    if ctrl_port:
        return ctrl_port
    else:
        return False

def local_ping(*args):
    local = Local()
    cmd = " ".join(args)
    result,_,_  = local.execute_command(cmd)

    pkt_loss = 100
    if result:
        match = re.search('(\d*)% packet loss', result)
        if match:
            pkt_loss = int(match.group(1))

    if pkt_loss == 100:
        return False
    else:
        return True

def check_and_clear_vhost(directory="/tmp/"):
    """
    :Function to check and clear vhost socket file from /tmp (default) directory
    :returns False if socket file is found and could not be deleted, else True
    """
    _, vhost_list = check_vhost_socket_count(directory)
    if vhost_list:
        for file in vhost_list:
                file_path = directory + file
                print(f"Deleting vhost-user socket file " + file_path)
                local = Local()
                _, returncode, _ = local.execute_command(f"rm -f "  + file_path)
                if returncode:
                    print(f"Cannot delete file " + file_path)
                    return False
                else:
                    return True
    else:            
        print(f"No vhost-user socket file found in " + directory)
        return True

def check_vhost_socket_count(directory="/tmp/"):
    """
    :Function to check vhost socket files count from /tmp (default) directory
    :returns length of vhost_list in integer and vhost_list 
    """
    vhost_list = []
    for file in os.listdir(directory):
        if file.startswith("vhost-user"):
            vhost_list.append(file)
    return len(vhost_list), vhost_list  
