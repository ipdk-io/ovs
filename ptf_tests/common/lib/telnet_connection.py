import telnetlib

class connectionManager():
    """
    Class to manage device via telnet.
    This class is used to manage VMs.
    """

    def __init__(self, host, port, username, password="", timeout=10):
        """
        Initiate telnet session and login to device
        """
        self.host = host
        self.port = port
        self.username = username
        self.password = password
        self.timeout = timeout
        self.login_prompt = b"login: "
        self.password_prompt = b"Password: "
        self.vm_root_prompt = b"#"
        self.vm_user_prompt = b"$"

        try :
            self.tn = telnetlib.Telnet(self.host, self.port, self.timeout)
        except Exception as err:
            print(f"Failed connection to {self.host} port {self.port} : {err}")
            raise err


        self.tn.write(b"\n")
        self.tn.read_until(self.login_prompt)
        self.tn.write(self.username.encode('ascii') + b"\n")

        if password:
            self.tn.read_until(self.password_prompt, self.timeout)
            self.tn.write(self.password.encode('ascii') + b"\n")


        n,_,_ = self.tn.expect([b'Login incorrect', self.vm_root_prompt, self.vm_user_prompt], self.timeout)
        if n == 0:
            print(f"FAIL: Login Failed to {self.host} port {self.port}")
            raise AssertionError(f"FAIL: Login Failed to {self.host} port {self.port}")


    def connect(self, username, password=""):
        """
        login/re-login to devices.
        """
        try :
            self.tn = telnetlib.Telnet(self.host, self.port, self.timeout)
        
        except Exception as err:
            print(f"Failed connection {err}")
            raise err

        self.tn.write(b"\n")
        self.tn.read_until(self.login_prompt, self.timeout)
        self.tn.write(username.encode('ascii') + b"\n")

        if password:
            self.tn.read_until(self.password_prompt, self.timeout)
            self.tn.write(password.encode('ascii') + b"\n")

    def sendCmd(self, cmd):
        """
        Send CLI commands through telnet
        """
        try:
            self.tn.write(cmd.encode('ascii') + b"\n")
            return True
        except Exception as err:
            print(f"Send command {cmd} Failed with error: {err}")
            return False

    def readResult(self):
        """
        Read the commad output from CLI.
        """
        try:
            return self.tn.read_until(b"\n*", self.timeout).decode('ascii')
        except Exception as err:
            print(f"Read CLI output failed with error: {err}")
            return False

    def close(self):
        """
        Close telnet sesssion
        """
        self.tn.close()
        return True

