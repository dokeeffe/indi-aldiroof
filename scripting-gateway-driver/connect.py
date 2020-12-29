#!/usr/bin/python
from firmatacontroller import FirmataRoofController
from pyfirmata import Arduino, util
import pyfirmata
import os


def main():
    rc = FirmataRoofController('/dev/ttyACM0')
    rc.disconnect()
    try:
        os.remove('/tmp/roofstate.p')
    except:
        pass
    print('OK')


if __name__ == "__main__":
    main()
