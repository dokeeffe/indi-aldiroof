from pyfirmata import Arduino, util
import pyfirmata
import time

class FirmataRoofController():

    NAME = "FirmataRoofController"

    def __init__(self, port):
        print('connecting')
        self.board = Arduino(port)
        self.it = util.Iterator(self.board)
        self.it.start()
        self.board.add_cmd_handler(
            pyfirmata.pyfirmata.STRING_DATA, self._messageHandler)
        print('connected')
        self.state = 'UNKNOWN'

    def disconnect(self):
        self.board.exit()

    def _messageHandler(self, *args, **kwargs):
        print('processing message')
        message_string = util.two_byte_iter_to_str(args)
        print('Got message {} '.format(message_string))
        self.state=message_string

    def update(self):
        super(Collector, self).update()

    def send_arduino_command(self, cmd=[]):
        print('sending command {}'.format(cmd))
        data = util.str_to_two_byte_iter(cmd + "\0")
        self.board.send_sysex(pyfirmata.pyfirmata.STRING_DATA, data)
        time.sleep(0.2)
        print('sent command')

    def dispose(self):
        super(Collector, self).dispose()
        try:
            self.board.exit()
        except AttributeError:
            print('exit() raised an AttributeError unexpectedly!')

    def abort(self):
        self.send_arduino_command('ABORT')

    def move(self, direction_cmd, until):
        self.abort()
        self.send_arduino_command('QUERY')
        time.sleep(0.1)
        if self.state == until:
            print('already {}'.format(until))
            return
        self.send_arduino_command(direction_cmd)
        i = 1
        while i < 40:
            self.send_arduino_command('QUERY')
            time.sleep(0.5)
            if self.state == until:
                self.abort()
                return
            i+=1
        self.abort()
