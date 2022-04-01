from enum import Enum
from ipaddress import AddressValueError
from multiprocessing.sharedctypes import Value
from typing import Any, Optional

DEBUG_NO_HARDWARE = True

class StepDirection(Enum):
    FORWARD = 0
    BACKWARD = 1

increment_map = {
    0: 0,       1: 1,     2: 2,     4: 3,
    8: 4,      16: 5,    32: 6,    64: 7,
    128: 8,   256: 9,   512: 10,   40: 11,
    80: 12,   160: 13,  320: 14,  640: 15
}

tile_dimensions_map = {
    32: 0, 64: 1, 128: 2, 256: 3
}

depth_map = {
    1: 0, 2: 1, 4: 2, 8: 3
}

increment_reverse_map = {v:k for k,v in increment_map.items()}

class Increment(Enum):
    INCR_0   = 0
    INCR_1   = 1
    INCR_2   = 2
    INCR_4   = 3
    INCR_8   = 4
    INCR_16  = 5
    INCR_32  = 6
    INCR_64  = 7
    INCR_128 = 8
    INCR_256 = 9
    INCR_512 = 10
    INCR_40  = 11
    INCR_80  = 12
    INCR_160 = 13
    INCR_320 = 14
    INCR_640 = 15

# For plain old 8 bit registers
def register_property(register, doc):
    def get_reg(self):
        return self.read_register(register)
    def set_reg(self, value):
        return self.write_register(register, value)
    return property(fget=get_reg, fset=set_reg, doc=doc)

# For single-bit registers
def register_bit_property(register, bit, doc):
    def get_reg(self):
        return self.get_register_bit(register, bit)
    def set_reg(self, value):
        return self.set_register_bit(register, bit, value)
    return property(fget=get_reg, fset=set_reg, doc=doc)

# For registers that are larger than a bit but smaller than a byte
def register_field_property(register, msb, lsb, doc):
    def get_reg(self):
        mask = ((1 << msb)-1)
        return (self.read_register(register) & mask) >> lsb
    def set_reg(self, value):
        mask = ((1 << msb)-1) ^ ((1 << lsb)-1)
        self.write_register(register, value << lsb, mask)

# For registers that require DCSEL to be set before accessing them
def dcsel_register_property(register, select, doc):
    def get_reg(self):
        self.DCSEL = select
        return self.read_register(register)
    def set_reg(self, value):
        self.DCSEL = select
        return self.write_register(register, select, value)
    return property(fget=get_reg, fset=set_reg, doc=doc)

# For single-bit registers that require DCSEL to be set
def dcsel_register_bit(register, select, bit, doc):
    def get_reg(self):
        self.DCSEL = select
        return self.get_register_bit(register, bit)
    def set_reg(self, value):
        self.DCSEL = select
        return self.set_register_bit(register, bit, value)
    return property(fget=get_reg, fset=set_reg, doc=doc)

VERA_ADDR_L = 0x00
VERA_ADDR_M = 0x01
VERA_ADDR_H = 0x02
VERA_DATA_0 = 0x03
VERA_DATA_1 = 0x04
VERA_CTRL   = 0x05

VERA_IEN           = 0x06
VERA_ISR           = 0x07
VERA_IRQ_LINE_L    = 0x08

VERA_DC_VIDEO      = 0x09
VERA_DC_HSCALE     = 0x0A
VERA_DC_VSCALE     = 0x0B
VERA_DC_BORDER     = 0x0C

VERA_DC_HSTART     = 0x09
VERA_DC_HSTOP      = 0x0A
VERA_DC_VSTART     = 0x0B
VERA_DC_VSTOP      = 0x0C

VERA_L0_CONFIG     = 0x0D
VERA_L0_MAPBASE    = 0x0E
VERA_L0_TILEBASE   = 0x0F
VERA_L0_HSCROLL_L  = 0x10
VERA_L0_HSCROLL_H  = 0x11
VERA_L0_VSCROLL_L  = 0x12
VERA_L0_VSCROLL_H  = 0x13

VERA_L1_CONFIG     = 0x14
VERA_L1_MAPBASE    = 0x15
VERA_L1_TILEBASE   = 0x16
VERA_L1_HSCROLL_L  = 0x17
VERA_L1_HSCROLL_H  = 0x18
VERA_L1_VSCROLL_L  = 0x19
VERA_L1_VSCROLL_H  = 0x1A

VERA_AUDIO_CTRL    = 0x1B
VERA_AUDIO_RATE    = 0x1C
VERA_AUDIO_DATA    = 0x1D

VERA_SPI_DATA      = 0x1E
VERA_SPI_CTRL      = 0x1F

class VERA(object):

    DEBUG_REGISTER_ACCESS = True

    def __init__(self, bus):
        self.bus = bus
        self._increment: Increment = Increment.INCR_0
        self._direction: StepDirection = StepDirection.FORWARD

    def write_register(self, register: int, value: int, mask: int = 0) -> None:
        '''
        Write to a register on VERA.

        Args:
            register (int): VERA register index [0..31]
            value (int): byte value to write to the register
            mask (int): bit mask of values to update, default=0
        
        Raises:
            IndexError: `register` is not a legal register index
            ValueError: `value` is not a legal byte value
        '''
        if self.DEBUG_REGISTER_ACCESS:
            print(f"  {register:02x} <- {value:02x} & {mask:02x}")
        if register < 0 or register > 0x1f:
            raise IndexError(register)
        if value < 0 or value > 0xff:
            raise ValueError(value)
        if mask != 0:
            register_value = self.bus.read_register(register)
            value = (register_value & ~mask) | (value & mask)
        self.bus.write_register(register, value)
    
    def read_register(self, register: int) -> int:
        '''
        Read a register from VERA.

        Args:
            register (int): VERA register index [0..31]

        Returns:
            int: Register byte value
        
        Raises:
            IndexError: `register` is not a legal register index
        '''
        if register < 0 or register > 0x1f:
            raise ValueError(register)
        value = self.bus.read_register(register)
        if self.DEBUG_REGISTER_ACCESS:
            print(f"  {register:02x} == {value:02x}")
        return value
    
    def update_register_bits(self, register: int,
                            bit7:Optional[int] = None, bit6:Optional[int] = None,
                            bit5:Optional[int] = None, bit4:Optional[int] = None,
                            bit3:Optional[int] = None, bit2:Optional[int] = None,
                            bit1:Optional[int] = None, bit0:Optional[int] = None) -> None:
        '''
        Update individual bits of a register. All bits are optional, unspecified bits will not be modified.

        Args:
            register (int): Register index to update.
            bit7 (int, optional): Value to set in register bit 7 (MSb)
            bit6 (int, optional): Value to set in register bit 6
            bit5 (int, optional): Value to set in register bit 5
            bit4 (int, optional): Value to set in register bit 4
            bit3 (int, optional): Value to set in register bit 3
            bit2 (int, optional): Value to set in register bit 2
            bit1 (int, optional): Value to set in register bit 1
            bit0 (int, optional): Value to set in register bit 0 (LSb)
        '''
        mask = 0
        flags = 0
        bits = [bit0, bit1, bit2, bit3, bit4, bit5, bit6, bit7]
        for i, value in enumerate(bits):
            if value is not None:
                mask |= 1 << i
                flags |= (value&1) << i
        self.write_register(register, flags, mask)
    
    def get_register_bit(self, register: int, bit_index: int) -> int:
        '''
        Read and return the value of a single register bit.
        
        Raises:
            IndexError if the bit_index is not in the range [0..7]
        '''
        if bit_index < 0 or bit_index > 7:
            raise IndexError
        value = self.read_register(register)
        return (value >> bit_index) & 0x1

    def set_register_bit(self, register: int, bit_index: int, value: int) -> None:
        '''
        Set an individual bit of a register.
        
        Raises:
            IndexError if the bit_index is not in the range [0..7]
        '''
        if bit_index < 0 or bit_index > 7:
            raise IndexError
        self.write_register(register, value << bit_index, 1 << bit_index )

    def write_data(self, port: int, value: int) -> None:
        '''
        Write to a VERA data port. Writing to port 0 is the same as writing to the property
        DATA0, and port 1 the same as DATA1.

        Args:
            port (int): Data port to write to (0 or 1)
            value (int): Byte value to write
        
        Raises:
            IndexError: `port` is not 0 or 1.
            ValueError: `value` is not a legal byte value.
        '''
        if port == 0:
            self.write_register(VERA_DATA_0, value)
        else:
            self.write_register(VERA_DATA_1, value)
    
    @property
    def address(self) -> int:
        '''The 17-bit VRAM address pointer.'''
        addr = (self.read_register(VERA_ADDR_H) & 1) << 16
        addr |= self.read_register(VERA_ADDR_M) << 8
        addr |= self.read_register(VERA_ADDR_L)
        return addr
    
    @address.setter
    def address(self, address: int):
        if address < 0 or address >= (1 << 17):
            raise AddressValueError
        self.write_register(VERA_ADDR_L, address & 0xff)
        self.write_register(VERA_ADDR_M, (address >> 8) & 0xff)
        self.write_register(VERA_ADDR_H, address >> 16 & 0x1, mask=0b00000001)
    
    @property
    def address_0(self) -> int:
        '''The 17-bit value for VRAM address pointer 0. Accessing this property will update ADDRSEL.'''
        self.ADDRSEL = 0
        return self.address
    
    @address_0.setter
    def address_0(self, address: int):
        self.ADDRSEL = 0
        self.address = address
    
    @property
    def address_1(self) -> int:
        '''The 17-bit value for VRAM address pointer 1. Accessing this property will update ADDRSEL.'''
        self.ADDRSEL = 1
        return self.address
    
    @address_1.setter
    def address_1(self, address: int):
        self.ADDRSEL = 1
        self.address = address
    
    @property
    def IRQ_LINE(self) -> int:
        '''Line number for interrupt trigger.'''
        line = (self.read_register(VERA_IEN) & 0x80) << 1
        line |= self.read_register(VERA_IRQ_LINE_L)
        return line
    
    @IRQ_LINE.setter
    def IRQ_LINE(self, line: int) -> None:
        if line < 0 or line >= 512:
            raise ValueError
        self.set_register_bit(VERA_IEN, 7, line >> 8)
        self.write_register(VERA_IRQ_LINE_L, line & 0xff)
    
    @property
    def INCR(self) -> int:
        '''Address increment value, encoded'''
        return self.read_register(VERA_ADDR_H) >> 4
    
    @INCR.setter
    def INCR(self, inc: int) -> None:
        if inc < 0 or inc > 15:
            raise ValueError
        self.write_register(VERA_ADDR_H, inc << 4, 0b11110000)

    @property
    def INCR0(self) -> int:
        '''Address increment value for pointer 0, encoded'''
        self.ADDRSEL = 0
        return self.INCR
    
    @INCR0.setter
    def INCR0(self, inc: int) -> None:
        self.ADDRSEL = 0
        self.INCR = inc

    @property
    def INCR1(self) -> int:
        '''Address increment value for pointer 1, encoded. Accessing this property will update ADDRSEL.'''
        self.ADDRSEL = 1
        return self.INCR
    
    @INCR1.setter
    def INCR1(self, inc: int) -> None:
        self.ADDRSEL = 1
        self.INCR = inc

    @property
    def DECR0(self) -> int:
        '''Address decrement select. Accessing this property will update ADDRSEL.'''
        self.ADDRSEL = 0
        return self.DECR
    
    @DECR0.setter
    def DECR0(self, value: int) -> None:
        self.ADDRSEL = 0
        self.DECR = value

    @property
    def DECR1(self) -> int:
        self.ADDRSEL = 1
        return self.DECR
    
    @DECR1.setter
    def DECR1(self, value: int) -> None:
        self.ADDRSEL = 1
        self.DECR = value

    @property
    def increment(self) -> int:
        '''Increment value, in number of bytes'''
        inc = increment_reverse_map[self.INCR]
        inc = -inc if self.DECR == 1 else inc
        return inc
    
    @increment.setter
    def increment(self, count: int) -> None:
        if count < 0:
            decr = 1
            count = -count
        else:
            decr = 0
        if count not in increment_map:
            raise ValueError
        incr = increment_map[count]
        self.write_register(VERA_ADDR_H, (incr << 4) | (decr << 3), mask=0b11111000)
    
    @property
    def output_mode(self) -> int:
        '''Display output mode'''
        return self.DC_VIDEO & 0x3
    
    @output_mode.setter
    def output_mode(self, value) -> None:
        self.write_register(VERA_DC_VIDEO, value, 0x3)
        
    ADDR_L      = register_property(        VERA_ADDR_L,        'VRAM address pointer bits 0-7')
    ADDR_M      = register_property(        VERA_ADDR_M,        'VRAM address pointer bits 8-15')
    ADDR_H      = register_property(        VERA_ADDR_H,        'VRAM address pointer bit 16 and address increment value')
    DECR        = register_bit_property(    VERA_ADDR_H,    3,  'Decrement select')
    
    DATA0       = register_property(        VERA_DATA_0,        'Data for VRAM pointer 0')
    DATA1       = register_property(        VERA_DATA_1,        'Data for VRAM pointer 1')
    
    CTRL        = register_property(        VERA_CTRL,          'Control register')
    ADDRSEL     = register_bit_property(    VERA_CTRL,      0,  'Address select')
    DCSEL       = register_bit_property(    VERA_CTRL,      1,  'Display configuration select')
    RESET       = register_bit_property(    VERA_CTRL,      7,  'Reset VERA')

    AFLOW_IEN   = register_bit_property(    VERA_IEN,       3,  'Audio fifo low interrupt enable')
    SPRCOL_IEN  = register_bit_property(    VERA_IEN,       2,  'Sprite collision interrupt enable')
    LINE_IEN    = register_bit_property(    VERA_IEN,       1,  'Line interrupt enable')
    VSYNC_IEN   = register_bit_property(    VERA_IEN,       0,  'Vertical sync pulse interrupt')
    
    DC_VIDEO    = dcsel_register_property(  VERA_DC_VIDEO,  0,      'Video display configuration register')
    DC_CURFIELD = dcsel_register_bit(       VERA_DC_VIDEO,  0,  7,  'Display current field')
    SPRITE_EN   = dcsel_register_bit(       VERA_DC_VIDEO,  0,  6, 'Sprite rendering enable bit')
    L1_EN       = dcsel_register_bit(       VERA_DC_VIDEO,  0,  5,  'Display layer 1 enable')
    L0_EN       = dcsel_register_bit(       VERA_DC_VIDEO,  0,  4,  'Display layer 0 enable')
    CHROMA_DIS  = dcsel_register_bit(       VERA_DC_VIDEO,  0,  2, 'Disable color output (composite/S-Video)')
    # TODO: Output mode

    DC_HSCALE   = dcsel_register_property(  VERA_DC_HSCALE, 0,  'Display horizontal scale register')
    DC_VSCALE   = dcsel_register_property(  VERA_DC_VSCALE, 0,  'Display vertical scale register')
    DC_BORDER   = dcsel_register_property(  VERA_DC_BORDER, 0,  'Display border color')
    
    DC_HSTART   = dcsel_register_property(  VERA_DC_HSTART, 1,  'Display horizontal start pixel, bits 2-9')
    DC_HSTOP    = dcsel_register_property(  VERA_DC_HSTOP,  1,  'Display horizontal stop pixel, bits 2-9')
    DC_VSTART   = dcsel_register_property(  VERA_DC_VSTART, 1,  'Display vertical start scanline, bits 8-1')
    DC_VSTOP    = dcsel_register_property(  VERA_DC_VSTOP,  1,  'Display vertical stop scanline, bits 8-1')

    L0_CONFIG   = register_property(        VERA_L0_CONFIG,     'Layer 0 configuration register')
    L0_MAPBASE  = register_property(        VERA_L0_MAPBASE,    'Layer 0 tilemap base address, bits 9-16')
    L0_TILEBASE = register_property(        VERA_L0_TILEBASE,   'Layer 0 tile bitmaps base address, bits 11-16')
    L0_HSCROLL_L= register_property(        VERA_L0_HSCROLL_L,  'Layer 0 horizontal scroll, bits 0-7')
    L0_HSCROLL_H= register_property(        VERA_L0_HSCROLL_H,  'Layer 0 horizontal scroll, bits 8-11')
    L0_VSCROLL_L= register_property(        VERA_L0_VSCROLL_L,  'Layer 0 vertical scroll, bits 0-7')
    L0_VSCROLL_H= register_property(        VERA_L0_VSCROLL_H,  'Layer 0 vertical scroll, bits 8-11')

    L1_CONFIG   = register_property(        VERA_L1_CONFIG,     'Layer 1 configuration register')
    L1_MAPBASE  = register_property(        VERA_L1_MAPBASE,    'Layer 1 tilemap base address, bits 9-16')
    L1_TILEBASE = register_property(        VERA_L1_TILEBASE,   'Layer 1 tile bitmaps base address, bits 11-16')
    L1_HSCROLL_L= register_property(        VERA_L1_HSCROLL_L,  'Layer 1 horizontal scroll, bits 0-7')
    L1_HSCROLL_H= register_property(        VERA_L1_HSCROLL_H,  'Layer 1 horizontal scroll, bits 8-11')
    L1_VSCROLL_L= register_property(        VERA_L1_VSCROLL_L,  'Layer 1 vertical scroll, bits 0-7')
    L1_VSCROLL_H= register_property(        VERA_L1_VSCROLL_H,  'Layer 1 vertical scroll, bits 8-11')

    def set_increment(self, increment: int, select: Optional[int] = None):
        if select is not None:
            self.ADDRSEL = select
        self.increment = increment

    def set_address_and_increment(self, address, increment: int = 0, select: Optional[int] = None):
        if select is not None:
            self.ADDRSEL = select
        self.ADDR_L = address & 0xff
        self.ADDR_M = (address >> 8) & 0xff
        direction = 0 if increment >= 0 else 1
        encoded_increment = increment_map[abs(increment)]
        addr_h = (encoded_increment << 4) | (direction.value << 3) | ((address >> 16) & 0x1)
        self.write_register(VERA.ADDR_H, addr_h & 0xff)
    
    def set_interrupt_enables(self, aflow = None, sprcol = None, line = None, vsync = None):
        self.update_register_bits(VERA.IEN, bit3=aflow, bit2=sprcol, bit1=line, bit0=vsync)

    def set_ctrl(self, addrsel=None, dcsel=None, reset=None):
        self.update_register_bits(self, bit7=reset, bit1=dcsel, bit0=addrsel)
    
    def configure_display(self, hscale: int = 1, vscale: int = 1, bordercol: int = 0, hstart: int = 0, hstop: int = 640, vstart: int = 0, vstop: int = 480):
        self.DCSEL = 0
        self.DC_HSCALE = round(128 / hscale)
        self.DC_VSCALE = round(128 / vscale)
        self.DC_BORDER = bordercol
        self.DC_HSTART = hstart >> 2
        self.DC_HSTOP = hstop >> 2
        self.DC_VSTART = vstart >> 1
        self.DC_VSTOP = vstop >> 1

    def configure_layer(self, layer_index: int, map_height: int, map_width: int,
                        t256c: bool, bitmap_mode: bool, bpp: int, map_base: int,
                        tile_base: int, tile_height: int, tile_width: int):
        h = tile_dimensions_map[map_height]
        w = tile_dimensions_map[map_width]
        t = 1 if t256c else 0
        b = 1 if bitmap_mode else 0
        d = depth_map[bpp]
        layer_config = (h << 6) | (w << 4) | (t << 3) | (b << 2) | d
        t_map_base = map_base >> 9
        t_base = tile_base >> 11
        t_height = (tile_height / 8) - 1
        t_width = (tile_width / 8) - 1

        if layer_index == 0:
            self.L0_CONFIG = layer_config
            self.L0_MAPBASE = t_map_base
            self.L0_TILEBASE = (t_base << 2) | t_height | t_width
            self.L0_HSCROLL_H = 0
            self.L0_HSCROLL_L = 0
            self.L0_VSCROLL_H = 0
            self.L0_VSCROLL_L = 0
        else:
            self.L1_CONFIG = layer_config
            self.L1_MAPBASE = t_map_base
            self.L1_TILEBASE = (t_base << 2) | t_height | t_width
            self.L1_HSCROLL_H = 0
            self.L1_HSCROLL_L = 0
            self.L1_VSCROLL_H = 0
            self.L1_VSCROLL_L = 0

    def start_display(self, layer0_en: bool = False, layer1_en: bool = False, sprite_en: bool = False):
        self.SPRITE_EN = 1 if sprite_en else 0
        self.L1_EN = 1 if layer1_en else 0
        self.L0_EN = 1 if layer0_en else 1
        self.output_mode = 1 # VGA
    
    def stop_display(self):
        self.output_mode = 0
