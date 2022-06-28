from common.lib.port_config import PortConfig
from common.lib.local_connection import Local


def gnmi_cli_set_and_verify(params):
    """
    Util function to gnmi-set and verify using gnmi-get
    :param params: list of params
                --> ["device:virtual-device,name:net_vhost0,host:host1,device-type:VIRTIO_NET,queues:1,socket-path:/tmp/vhost-user-0,port-type:LINK",
                "device:virtual-device,name:net_vhost1,host:host2,device-type:VIRTIO_NET,queues:1,socket-path:/tmp/vhost-user-1,port-type:LINK",
                ...]
    :return: Boolean True/False
    """
    gnmi_set_params(params)
    return gnmi_get_params_verify(params)

def gnmi_set_params(params):
    port_config = PortConfig()
    for param in params:
        output = port_config.GNMICLI.gnmi_cli_set(param)
    port_config.GNMICLI.tear_down()

    return output

def gnmi_get_params_verify(params):
    port_config = PortConfig()
    results=[]
    for param in params:
        mandatory_param = ",".join(param.split(',')[:2])

        passed=True
        
        for entry in param.split(',')[2:]:
            if port_config.GNMICLI.gnmi_cli_get(mandatory_param, entry.split(':')[0]) != entry.split(':')[1]:
                passed=False
        results.append(passed)
    port_config.GNMICLI.tear_down()
    
    if [x for x in results if not x]:
        for param in params:
            print(f"PASS: gnmi-cli get verified for {param}")
        return True

    return False

def gnmi_get_params_elemt_value(params, elemt):
    port_config = PortConfig()
    elemt_value_list=[]
    results=[]
    for param in params:
        mandatory_param = ",".join(param.split(',')[:2])

        passed=True

        value = port_config.GNMICLI.gnmi_cli_get(mandatory_param, elemt).strip()
        if value :
           elemt_value_list.append(value)
        else:
            passed=False
            
        results.append(passed)
    port_config.GNMICLI.tear_down()
 
    if [x for x in results if not x]:
        return False
    
    return elemt_value_list

def gnmi_get_params_counter(param):
    port_config = PortConfig()
    results=[]
    port_counter= dict()
    mandatory_param = ",".join(param.split(',')[:2])

    value = port_config.GNMICLI.gnmi_cli_get_counter(mandatory_param,"counters").strip()
    for va in value.split("\n"):
            _, counter =va.split(":")
            results.append(counter.strip().replace('"', ''))   
            
    iter_rslt = iter(results)
    for each in iter_rslt:
         port_counter[each] = int(next(iter_rslt))

    if port_counter:
           return port_counter
    else:
        return False

def ip_set_ipv4(interface_ip_list):
    port_config = PortConfig()
    for interface_ipv4_dict in interface_ip_list:
        for interface, ip in interface_ipv4_dict.items():
            port_config.Ip.iplink_enable_disable_link(interface, status_to_change='up')
            port_config.Ip.ipaddr_ipv4_set(interface, ip)

    return

def gnmi_get_element_value(param, element):
    """
    : Get value of an element from output of gnmi cli query and verify 
    : return: value in integer / string or Boolean False

    """
    port_config = PortConfig()
    result = port_config.GNMICLI.gnmi_cli_get(param, element)
    if [x for x in result if not x]:
        return False
    else:
        return result
    port_config.GNMICLI.tear_down()

def get_port_mtu_linuxcli(port):
    """
    : Get MTU value from linux cli for a port / interface
    : return: value in integer or Boolean False

    """
    local = Local()
    mtu_value, returncode, err = local.execute_command(f"cat /sys/class/net/" + port + "/mtu")
    if returncode:
        print(f"Failed to get MTU for " + port + " port")
        return False
    else:
        return mtu_value




