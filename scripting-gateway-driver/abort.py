#!/usr/bin/python
from firmatacontroller import FirmataRoofController
from pyfirmata import Arduino, util
import pyfirmata

def main():
    rc = FirmataRoofController('/dev/ttyACM0')
    rc.abort()
    rc.disconnect()
    print('OK')


if __name__ == "__main__":
    main()
