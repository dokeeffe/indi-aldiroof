#!/usr/bin/python
from firmatacontroller import FirmataRoofController
from pyfirmata import Arduino, util
import pyfirmata

def main():
    rc = FirmataRoofController('/dev/ttyACM0')
    rc.move('CLOSE', until='CLOSED')
    rc.disconnect()
    try:
        os.remove('/tmp/roofstate.p')
    except:
        pass
    print('OK')


if __name__ == "__main__":
    main()
