"""
DPDK MAX TAP PORT 

"""

# in-built module imports
import time
import sys, unittest
# ptf related imports
import ptf
from ptf.base_tests import BaseTest
from ptf.testutils import *
from ptf import config


# framework related imports
from common.utils.config_file_utils import get_gnmi_params_simple, get_config_dict
from common.utils.gnmi_cli_utils import gnmi_cli_set_and_verify, gnmi_set_params


class Max_Tap_Port_Mtu(BaseTest):

    def setUp(self):
        BaseTest.setUp(self)
        self.result = unittest.TestResult()
        test_params = test_params_get()
        config_json = test_params['config_json']
        self.config_data = get_config_dict(config_json)

        self.gnmicli_params = get_gnmi_params_simple(self.config_data)

    def runTest(self):
        max_port_count = int(self.config_data['max_port_count'])
        print(f"Creating tap ports count: {max_port_count}")
        if not gnmi_cli_set_and_verify(self.gnmicli_params):
            self.result.addFailure(self, sys.exc_info())
            self.fail("Failed to configure gnmi cli ports")
        else: 
            print(f"Max TAP port count:  {max_port_count}  successful ") 
  
    def tearDown(self):

        if self.result.wasSuccessful():
            print("Test has PASSED")
        else:
            print("Test has FAILED")
        

 

