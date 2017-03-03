An INDI (http://indilib.org/) driver and arduino firmware to control a roll off roof of an astronomical observatory.

![observatory](https://raw.githubusercontent.com/dokeeffe/indi-aldiroof/master/docs/allsky-25.gif)

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

# Wiring relays to the hoist switch

Relays were wired into the hoist's hand controller switch to allow both manual operation via the switch and operation from the arduino.
WARNING: Never operate the switch using both the relays and manual switch at the same time as it may result in a 220v short circuit.

The photo below shows the hoist's hand controller switch with 6 numbered pins. 
The centre 2 pins (3 and 4) provide supply to either 1 and 2 or 5 and 6 depending on what way the switch is pressed.
4 relays were wired into this switch to override it.

![Arduino and relays](https://raw.githubusercontent.com/dokeeffe/indi-aldiroof/master/docs/hand-control.jpg)

The wiring diagram below shows the same switch on the right hand side. The centre shows the 4 relays and the left shows the arduino micro.
The 2 limit switches are also shown attached to pins 8 and 9. See the arduino firmware sketch code for more details.

![Arduino and relays](https://raw.githubusercontent.com/dokeeffe/indi-aldiroof/master/docs/wiring-diagram.jpg)

The photo below shows the finished enclosure containing the arduino and 4 relays.

![Arduino and relays](https://pbs.twimg.com/media/CQlkj6qUsAElgoM.jpg:large)

2. TODO: Photos of hoist in roof

![hoist](https://pbs.twimg.com/media/Cf_WMwnUMAAqjbK.jpg:large)
