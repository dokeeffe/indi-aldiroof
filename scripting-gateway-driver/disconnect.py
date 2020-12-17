#!/usr/bin/python

import os

try:
    os.remove('/tmp/roofstate.p')
except:
    pass
print('ok')
