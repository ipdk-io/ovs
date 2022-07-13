#!/usr/bin/python

from common.lib.local_connection import Local
from common.lib.exceptions import ExecuteCMDException
from common.lib.ssh import Ssh


class PortConfig(object):
    def __init__(self, remote=False, hostname="", username="", passwd=""):
        """
        Constructor method
        """
        self.GNMICLI = self._GNMICLI()
        self.Ip = self._IpCMD(remote = remote, hostname = hostname , username
        = username, passwd = passwd)

    class _Common(object):
        cmd_prefix = None

        def form_cmd(self, cmd):
            """Combine command prefix with command
            :param cmd: command to combine with prefix
            :type cmd: str
            :return: command combined with prefix
            :rtype: str
            """
            return " ".join([self.cmd_prefix, cmd])

    class _GNMICLI(_Common):
        def __init__(self):
            """
            Constructor method
            """
            # self.ssh = SSH(host, username, password)
            self.local = Local()
            self.cmd_prefix = 'gnmi-cli'


        def gnmi_cli_set(self, params):
            """
            gnmi-cli set command
            :param params: all parameters required for gnmi-cli set
            :type params: str
            :return: stdout of the gnmi-cli set command
            :rtype: str
            """
            cmd = self.form_cmd(f"set \"{params}\"")
            output, return_code, _ = self.local.execute_command(cmd)
            if 'Set request, successful' not in output:
                print(f"FAIL: {cmd}")
                raise ExecuteCMDException(f'Failed to execute command {cmd}')
            print(f"PASS: {cmd}")
            return output

        def gnmi_cli_get(self, mandatory_params, key):
            """
            gnmi-cli get command
            :param mandatory_params: "device:virtual-device,name:net_vhost0"
            :param key: "mtu" or "pipeline-name" etc.
            :return: stdout of the gnmi-cli get command
            :rtype: str
            """
            cmd = self.form_cmd(f"get \"{mandatory_params},{key}\" |egrep \"*_val\" | cut -d \":\"  -f 2")
            output, return_code, _ = self.local.execute_command(cmd)
            if return_code:
                print(f"FAIL: {cmd}")
                raise ExecuteCMDException(f'Failed to execute command "{cmd}"')
            return output

        def gnmi_cli_get_counter(self, mandatory_params, key="counters"):
            """
            gnmi-cli get command
            :param mandatory_params: "device:virtual-device,name:TAP2,counters"
            :param key: "counter".
            :return: stdout of the gnmi-cli get command
            :rtype: str
            """
            cmd = self.form_cmd(f"get \"{mandatory_params},{key}\" |grep \"name\\|uint_val\"|grep -v \"interface\\|key\\|config\\|counters\"")
            output, return_code, _ = self.local.execute_command(cmd)
            if return_code:
                print(f"FAIL: {cmd}")
                raise ExecuteCMDException(f'Failed to execute command "{cmd}"')
            return output

        def tear_down(self):
            """
            TBD
            """
            pass

    class _IpCMD(_Common):
        """
        This class intended to have methods related to ip command only
        """
        def __init__(self, remote=False, hostname="", username="", passwd=""):
            """
            Constructor method
            :param remote: set value to True enables remote host cmd execution
            :type remote: boolean e.g. remote=True
            :param hostname: remote host IP address, not required for DUT host
            :type hostname: string e.g. 10.233.132.110
            :param username: remote host username, not required for DUT
            :type username: string e.g. root
            :param passwd: remote host password, not required for DUT
            :type passwd: string e.g. cloudsw
            """
            self.cmd_prefix = 'ip'
            if remote:
                self.connection = Ssh(hostname=hostname, username=username,
                                      passwrd=passwd)
                self.connection.setup_ssh_connection()
            else:
                self.connection = Local()

        def iplink_enable_disable_link(self, interface, status_to_change='up'):
            """
            Brings <interface> up
            :param: interface: network interface name --> str e.g. "TAP1"
            :param: status_to_change: state of the interface to be changed to --> str --> accepted values 'up' or 'down'
            :return: True/False --> boolean
            """
            assert status_to_change == 'up' or status_to_change == 'down'

            cmd = self.form_cmd(f" link set {interface} {status_to_change}")
            output, return_code, _ = self.connection.execute_command(cmd)
            if return_code:
                print(f"FAIL: {cmd}")
                raise ExecuteCMDException(f'Failed to execute command "{cmd}"')

            print(f"PASS: {cmd}")
            return True

        def ipaddr_ipv4_set(self, interface, ip):
            """
            Assigns IP address 'ip' to 'interface'
            :param interface: network interface name --> str e.g. "TAP0"
            Assigns IP address 'ip' to 'interface'
            :param interface: network interface name --> str e.g. "TAP0"
            :param ip: ipv4 address --> str e.g. "1.1.1.1/24"
            :return: True/False --> boolean
            """
            cmd = self.form_cmd(f" addr add {ip} dev {interface}")
            output, return_code, _ = self.connection.execute_command(cmd)
            if return_code:
                print(f"FAIL: {cmd}")
                raise ExecuteCMDException(f'Failed to execute command "{cmd}"')

            print(f"PASS: {cmd}")
            return True
           

        def iplink_add_vlan_port(self, id, name, netdev_port):
            """ Method to add vlan port to given netdev port

            :param id: vlan id
            :type id: integer e.g. 1
            :param name: name of vlan port to add
            :type name: string e.g. vlan1
            :param netdev_port: name of netdev where vlan ports to be added
            :type netdev_port: string e.g. TAP0
            :return: exit status
            :rtype: boolean e.g. True on success, script exits on failure
            """
            cmd = self.form_cmd(f" link add link {netdev_port} name {name} "
                                f"type vlan id {id}")
            output, return_code, err = self.connection.execute_command(cmd)
            if return_code:
                raise ExecuteCMDException(f'FAIL:command "{cmd}" failed with '
                                          f'an error {err}')
            print(f"PASS: {cmd}")
            return True

        def iplink_create_veth_interface(self, veth_iface, veth_peer_iface):
            """ This method creates virtual network interface

            :param veth_iface: netns virtual VM interface name
            :type veth_iface: string e.g. veth_vm0
            :param veth_peer_iface: peer name for veth interface
            :type veth_peer_iface: string e.g. veth_host_vm0
            :return: exit status
            :rtype: boolean e.g. True on success, script exits on failure
            """
            cmd = self.form_cmd(f" link add {veth_iface} type veth peer name "
                                f"{veth_peer_iface}")
            output, return_code, err = self.connection.execute_command(cmd)
            if return_code:
                raise ExecuteCMDException(f'FAIL:command "{cmd}" failed with '
                                          f'an error {err}')
            print(f"PASS: {cmd}")
            return True

        def iplink_add_veth_to_netns(self, namespace_name, veth_iface):
            """ This method adds virtual network interface to created namespace

            :param namespace_name: name of VM namespace
            :type namespace_name: string e.g. VM0
            :param veth_iface: netns virtual VM interface name
            :type veth_iface: string e.g. veth_vm0
            :return: exit status
            :rtype: boolean e.g. True on success, script exits on failure
            """
            cmd = self.form_cmd(f" link set {veth_iface} netns "
                                f"{namespace_name}")
            output, return_code, err = self.connection.execute_command(cmd)
            if return_code:
                raise ExecuteCMDException(f'FAIL:command "{cmd}" failed with '
                                          f'an error {err}')
            print(f"PASS: {cmd}")
            return True

        def iplink_del_port(self, port_to_delete):
            """ This method is used to delete any port including vlan type

            :param port_to_delete: name of port to delete
            :type port_to_delete: string e.g. vlan1
            :return: exit status
            :rtype: True on success, script exits on failure
            """
            cmd = self.form_cmd(f" link del {port_to_delete}")
            output, return_code, err = self.connection.execute_command(cmd)
            if return_code:
                raise ExecuteCMDException(f'FAIL:command "{cmd}" failed with '
                                          f'an error {err}')
            print(f"PASS: {cmd}")
            return True

        def ipnetns_create_namespace(self, vm_name):
            """ This method is used to create netns type VM

            :param vm_name: name of vm to add
            :type vm_name: string e.g. VM0
            :return: exit status
            :rtype: True on success, script exits on failure
            """
            cmd = self.form_cmd(f" netns add {vm_name}")
            output, return_code, err = self.connection.execute_command(cmd)
            if return_code:
                raise ExecuteCMDException(f'FAIL:command "{cmd}" failed with '
                                          f'an error {err}')
            print(f"PASS: {cmd}")
            return True

        def ipnetns_execute_command(self, namespace, command):
            """ This method executes command with network namespace created

            :param namespace: VM namespace name
            :type namespace: string e.g. VM0
            :param command: command to execute
            :type command: string e.g. command = "ip addr"
            :return: exit status
            :rtype: boolean e.g. True of success, script exits on failure
            """
            cmd = self.form_cmd(f" netns exec {namespace} {command}")
            output, return_code, err = self.connection.execute_command(cmd)
            if return_code:
                raise ExecuteCMDException(f'FAIL:command "{cmd}" failed with '
                                          f'an error {err}')
            print(f"PASS: {cmd}")
            return True

        def ipnetns_delete_namespace(self, namespace_name):
            """ This method is used to delete VM namespace

            :param namespace_name: namespace name to delete
            :type namespace_name: string e.g. VM0
            :return: exit status
            :rtype: True on success, script exits on failure
            """
            cmd = self.form_cmd(f" netns delete {namespace_name}")
            output, return_code, err = self.connection.execute_command(cmd)
            if return_code:
                raise ExecuteCMDException(f'FAIL:command "{cmd}" failed with '
                                          f'an error {err}')
            print(f"PASS: {cmd}")
            return True

        def tear_down(self):
            """ Close any open connections after use of class

            :return: None
            :rtype: None
            """
            self.connection.tear_down()
