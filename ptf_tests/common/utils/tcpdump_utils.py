from common.lib.tcpdump import TcpDumpCap
from common.lib.local_connection import Local
import os


def tcpdump_start_pcap(interface, src_host, pkt_count=1 ):
    """
    TCPDUMP function to start packet capture in background and dump packet capture to /tmp dir
    e.g  << tcpdump -i TAP1 host 192.168.1.10 -nn -c 1 >> /tmp/TAP1/TAP1.pcap &

    :params interface: to start packet capture 
            src_host: to filter traffic with source ip
            pkt_count: Number of packets to capure with default value as 1 
    """
    tcpd = TcpDumpCap()

    cmdopt = []
    if interface:
        cmdopt.extend(['-i', interface])
    if src_host:
        cmdopt.extend(['src', src_host])
    if pkt_count:
        cmdopt.extend(['-c', pkt_count])
    cmdopt.extend(['-nn'])
    pcapdir = "/tmp/" + interface  
    if not os.path.exists(pcapdir):
       try:
           os.makedirs(pcapdir)
       except OSError as e:
           if e.errno != errno.EEXIST:
                raise
    pcapfile = pcapdir + "/" + interface + ".pcap"
    cmdopt.extend(['>>', pcapfile])
    if os.path.exists(pcapfile): os.remove(pcapfile)
    tcpd.TCPDUMP.tcpdump_start_capture(cmdopt)


def tcpdump_get_pcap(interface): 
    """
    Function to return captured packets in clear text
    param: interface name for file/interface identity
    return: type str, tcdump packet capture output

    """
    pcapdir = "/tmp/" + interface
    pcapfile = pcapdir + "/" + interface + ".pcap"
    if os.path.exists(pcapfile) and os.stat(pcapfile).st_size != 0:
        output = open(pcapfile).read()
        return output
    else: 
        print("Pcap file does not exist or empty")

def tcdump_match_str(superstring, substring):
    """
    Function to match all substring elements with superstring
    params: type str, superstring and substring
    return: value of True or false
    """
    superstringlist = list(superstring.split(" "))
    substringlist = list(substring.split(" "))
    result = True
    for str in substringlist:
        if str not in superstringlist:
            result = False
    return result

def tcpdump_tear_down():
  
    tcpd = TcpDumpCap()
    tcpd.TCPDUMP.tcpdump_tear_down()
