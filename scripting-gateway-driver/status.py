#!/usr/bin/python

import sys
import pickle
import time
from firmatacontroller import FirmataRoofController
from pyfirmata import Arduino, util
import pyfirmata

OPEN='0 1 0'
CLOSED='1 0 0'
UNKNOWN='2 0 0'
CACHE_EXPIRY=300

def retrieve_cached():
    try:
        now = int(time.time())
        state = pickle.load( open( "/tmp/roofstate.p", "rb" ) )
        if state==None or state['time'] < now -CACHE_EXPIRY:
            print('Cache old')
            return None
        return state['state']
    except:
        return None

def query_firmware():
    rc = FirmataRoofController('/dev/ttyACM0')
    rc.send_arduino_command('QUERY')
    rc.disconnect()
    return rc.state

def cache_state(state):
    pickle.dump( {'state':state, 'time': int(time.time())}, open( "/tmp/roofstate.p", "wb" ) )

def write_to_indi_tempfile(path, state):
    status_file = open(path, 'w')
    status_file.truncate()
    if state=='OPEN':
        status_file.write(OPEN)
    elif state=='CLOSED':
        status_file.write(CLOSED)
    elif state=='UNKNOWN':
        status_file.write(UNKNOWN)
    status_file.close()


def main(path):
    '''
    Queries the roof and writes a 3 digit string to the temp file passed. 3 digits represent park-state, shutter state and azimuth.
    Since this is not a rotating dome, the azimuth is hard coded to 0.
    '''
    state = retrieve_cached()
    if state==None:
        state = query_firmware()
        cache_state(state)
    write_to_indi_tempfile(path, state)


if __name__ == "__main__":
    script, path = sys.argv
    main(path)
