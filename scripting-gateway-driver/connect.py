#!/usr/bin/python
from firmatacontroller import FirmataRoofController
from pyfirmata import Arduino, util
import pyfirmata


def main():
    rc = FirmataRoofController('/dev/ttyACM0')
    rc.disconnect()
    print('PARKED')


if __name__ == "__main__":
    main()
