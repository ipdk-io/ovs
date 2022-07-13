#!/usr/bin/python

from common.lib.local_connection import Local
from common.lib.exceptions import ExecuteCMDException
import subprocess 

class TcpDumpCap(object):
    def __init__(self):
        """
        Constructor method
        """
        self.TCPDUMP = self._TCPDUMP()

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

    class _TCPDUMP(_Common):
        def __init__(self):
            """
            Constructor method
            """
            self.local = Local()
            cmd = "which tcpdump"
            output, return_code, _ = self.local.execute_command(cmd)
            if return_code: 
                print(f"Error: Tcpdump is not installed")

            self.cmd_prefix = 'tcpdump'


        def tcpdump_start_capture(self, params):
            """
            tcpdump command to start capture in background and supressed stdout
            :param params: list of parameters for tcpdump capture 
            :type params: list
            :return: none, exit code of executing tcpdump with the parameters 
            """
            paramstr = " "
            params = paramstr.join(map(str, params))
            cmd = self.form_cmd(params + " 2> /dev/null &")
            output, return_code, _ = self.local.execute_command(cmd)
            if return_code:
                print(f"Failed to run the tcpdump capture command")
                return False
            else:    
                return output


        def tcpdump_tear_down(self):
            """
            Function
            """
            cmd = "pkill -9 tcpdump"
            output, return_code, _ = self.local.execute_command(cmd)
            if not return_code:
                print("tcpdump process terminated")
            else:
                print("No tcpdump process to terminate")
                    
