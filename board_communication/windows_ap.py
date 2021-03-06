import subprocess
import re
from util.util import run_cmd
from concurrent.futures import ThreadPoolExecutor


class WindowsSoftAP:
    def __init__(self, ssid="test_ap_1", key="1234567890"):
        """
        Class to create and start the netsh "hostednetwork" soft AP Windows feature

        :param ssid: (str) ssid for AP
        :param key: (str) password for AP
        """

        self.ssid = ssid
        self.key = key
        self.ipv4 = None

    def start_ap(self):
        """
        Starts the AP and sets self.ipv4 to the IPv4 of the AP
        """

        self.ipv4 = self._get_ipv4()

        if not self.ipv4:
            print("\n*** CONFIGURING WINDOWS HOSTEDNETWORK ***\n")
            run_cmd(f"netsh wlan set hostednetwork mode=allow ssid={self.ssid} key={self.key}")

            print("\n*** STARTING WINDOWS HOSTEDNETWORK ***\n")
            run_cmd("netsh wlan start hostednetwork")

            print("\n*** RETRIEVING IP OF WINDOWS HOSTEDNETWORK ***\n")
            self.ipv4 = self._get_ipv4()

        if not self.ipv4:
            print("*** ERROR: UNABLE TO RETRIEVE WINDOWS HOSTNETWORK IP, CHECK IF RUNNING ***")
            return

        print(f"*** WINDOWS HOSTEDNETWORK STARTED, IP: {self.ipv4} ***")

    def get_ipv4(self):
        """
        Returns the IPv4 address of the AP

        :return: (str) IPv4 address of the AP in the form: "xxx.xxx.xxx.xxx"
        """

        if not self.ipv4:
            self.ipv4 = self._get_ipv4()
            if not self.ipv4:
                print('this WindowsSoftAP instance has no associated IP, must run WindowsSoftAP.start_ap() first')
            else:
                return self.ipv4
        else:
            return self.ipv4

    def stop_ap(self):
        """
        Stops the AP
        """

        print("\n*** STOPPING WINDOWS HOSTEDNETWORK ***\n")
        run_cmd("netsh wlan stop hostednetwork")
        print("\n*** WINDOWS HOSTEDNETWORK STOPPED ***")

    def _get_ipv4(self):
        """
        Retrieves and returns the IPv4 address of the hostednetwork AP from an "ipconfig /all" call

        :return: (str) IPv4 address of the AP in the form: "xxx.xxx.xxx.xxx"
        """
        stdout = run_cmd("ipconfig /all", show=False).lower().split(sep='\n')
        record_next_ip = False
        ip = None
        for line in stdout:
            if not record_next_ip:
                # this will be read in the "description" line of an adapter
                if "microsoft hosted network virtual adapter" in line \
                        or "microsoft wi-fi direct virtual adapter" in line:
                    record_next_ip = True
            else:
                if "ipv4 address" in line:
                    match = re.search(r'(\d+)\.(\d+)\.(\d+)\.(\d+)', line)
                    ip = match.group(0)
                    break
                # if another "description" is read before an "ipv4 address" there is no associated IPv4 address with
                # the "microsoft hosted network virtual adapter" adapter, most likely because the hostednetwork
                # is not running
                elif "description" in line and not ("microsoft hosted network virtual adapter" in line
                                                    or "microsoft wi-fi direct virtual adapter" in line):
                    record_next_ip = False
        return ip


if __name__ == "__main__":
    ap = WindowsSoftAP()
    ap.start_ap()
    ap.stop_ap()
