An INDI (http://indilib.org/) driver and arduino firmware to control a roll off roof of an astronomical observatory.

![observatory](https://pbs.twimg.com/media/CFelQpDW0AEEALn.jpg)

The roof is powered by a 550W electric hoist purchased from Aldi.
The arduino controls 4 relays connected to the hoist's hand controller which override the manual switch.
The indi driver comminicates with the arduino using the frimata protocol.

# Hardware Parts List

1. Electric hoist reworked to open and close the roof.
1. Arduino Micro (to install firmware on)
2. 4 X 30A relays http://www.dx.com/p/30a-250v-relay-module-red-blue-216873#.Vgg9-xNViko
3. Project box (to contain arduino and relays)
4. Linux machine running indi server (install the driver on).

# Building / Installing Software

1. Flash the firmware to your arduino using the arduino ide.
2. Build and install the driver on the machine running indi server.

# Building Hardware

1. TODO: Wiring diagram and description

![Arduino and relays](https://pbs.twimg.com/media/CQlkj6qUsAElgoM.jpg:large)

2. TODO: Photos of hoist in roof

![hoist](https://customerservice.aldi.co.uk/warranties/images/products/ThuJan22161226UTC2009.jpg)
