#!/usr/bin/python

import sys

from firmatacontroller import FirmataRoofController
from pyfirmata import Arduino, util
import pyfirmata

def main(path):
    rc = FirmataRoofController('/dev/ttyACM0')
    rc.send_arduino_command('QUERY')
    status_file = open(path, 'w')
    status_file.truncate()
    if rc.state=='OPEN':
        status_file.write('0 1 0')
    elif rc.state=='CLOSED':
        status_file.write('1 0 0')
    elif rc.state=='UNKNOWN':
        status_file.write('2 0 0')
    status_file.close()
    rc.disconnect()


if __name__ == "__main__":
    script, path = sys.argv
    main(path)
