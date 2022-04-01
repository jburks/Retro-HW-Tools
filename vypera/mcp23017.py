import smbus
from enum import Enum

class MCP23017(object):
    OUTPUT = 0
    INPUT = 1

    PORT_A = 0
    PORT_B = 1

    IODIR_A     = 0x00          # Controls the direction of the data I/O for port A.
    IODIR_B     = 0x01          # Controls the direction of the data I/O for port B.
    IPOL_A      = 0x02          # Configures the polarity on the corresponding GPIO_ port bits for port A.
    IPOL_B      = 0x03          # Configures the polarity on the corresponding GPIO_ port bits for port B.
    GPINTEN_A   = 0x04          # Controls the interrupt-on-change for each pin of port A.
    GPINTEN_B   = 0x05          # Controls the interrupt-on-change for each pin of port B.
    DEFVAL_A    = 0x06          # Controls the default comparaison value for interrupt-on-change for port A.
    DEFVAL_B    = 0x07          # Controls the default comparaison value for interrupt-on-change for port B.
    INTCON_A    = 0x08          # Controls how the associated pin value is compared for the interrupt-on-change for port A.
    INTCON_B    = 0x09          # Controls how the associated pin value is compared for the interrupt-on-change for port B.
    IOCON       = 0x0A          # Controls the device.
    GPPU_A      = 0x0C          # Controls the pull-up resistors for the port A pins.
    GPPU_B      = 0x0D          # Controls the pull-up resistors for the port B pins.
    INTF_A      = 0x0E          # Reflects the interrupt condition on the port A pins.
    INTF_B      = 0x0F          # Reflects the interrupt condition on the port B pins.
    INTCAP_A    = 0x10          # Captures the port A value at the time the interrupt occured.
    INTCAP_B    = 0x11          # Captures the port B value at the time the interrupt occured.
    GPIO_A      = 0x12          # Reflects the value on the port A.
    GPIO_B      = 0x13          # Reflects the value on the port B.
    OLAT_A      = 0x14          # Provides access to the port A output latches.
    OLAT_B      = 0x15          # Provides access to the port B output latches.

    def __init__(self, address: int, bus: int):
        self.address = address
        self.bus = smbus.SMBus(bus)
        
    def init(self):
        self.write_register(self.IOCON, 0b00100000)
        self.write_register(self.GPPU_A, 0xff)
        self.write_register(self.GPPU_B, 0xff)
    
    def port_mode(self, port: int, directions: int, pullups: int = 0xFF, inverted: int = 0x00):
        self.write_register(self.IODIR_A + port, directions)
        self.write_register(self.GPPU_A + port, directions)
        self.write_register(self.IPOL_A + port, directions)

    def port_dir(self, port: int, direction: int):
        self.write_register(self.IODIR_A + port, direction * 0xff)

    def write_port(self, port: int, value: int):
        self.write_register(self.GPIO_A + port, value)

    def read_port(self, port: int) -> int:
        return self.read_register(self.GPIO_A + port)

    def write_register(self, register: int, value: int):
        self.bus.write_byte_data(self.address, register, value)
    
    def read_register(self, register: int) -> int:
        return self.bus.read_byte_data(self.address, register)
