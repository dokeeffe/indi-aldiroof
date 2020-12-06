#!/usr/bin/python

import sys

from firmatacontroller import FirmataRoofController
from pyfirmata import Arduino, util
import pyfirmata

OPEN='0 1 0'
CLOSED='1 0 0'
UNKNOWN='2 0 0'

def main(path):
    '''
    Queries the roof and writes a 3 digit string to the temp file passed. 3 digits represent park-state, shutter state and azimuth.
    Since this is not a rotating dome, the azimuth is hard coded to 0.
    '''
    rc = FirmataRoofController('/dev/ttyACM0')
    rc.send_arduino_command('QUERY')
    status_file = open(path, 'w')
    status_file.truncate()
    if rc.state=='OPEN':
        status_file.write(OPEN)
    elif rc.state=='CLOSED':
        status_file.write(CLOSED)
    elif rc.state=='UNKNOWN':
        status_file.write(UNKNOWN)
    status_file.close()
    rc.disconnect()


if __name__ == "__main__":
    script, path = sys.argv
    main(path)
