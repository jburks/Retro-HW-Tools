
class VeraIntf:
    CS_N = (1 << 7)
    WE_N = (1 << 6)
    RE_N = (1 << 5)
    def __init__(self, mcp):
        self.mcp = mcp
        mcp.port_mode(mcp.PORT_A, mcp.OUTPUT)
        mcp.port_mode(mcp.PORT_B, mcp.OUTPUT)
        self.ctrl = mcp.GPIO_B
        self.data = mcp.GPIO_A
    
    def write_register(self, index, value):
        # assert CS/WR and put address on bus
        self.mcp.write_register(self.ctrl, self.RE_N | (index & 0x1f))
        # Data appears on bus after CS/WR assertion
        self.mcp.write_register(self.data, value & 0xff)
        
        # Data is sampled on rising edge of (CS# | WR#)
        self.mcp.write_register(self.ctrl, self.CS_N | self.WE_N | self.RE_N | (index & 0x1f))
    
    def read_register(self, index):
        self.mcp.write_register(self.mcp.IODIR_A, 0b11111111)
        
        self.mcp.write_register(self.ctrl, self.CS_N | self.WE_N | self.RE_N | (index & 0x1f)) # Address precedes CS#
        self.mcp.write_register(self.ctrl, 0    | self.WE_N | 0    | (index & 0x1f)) # Assert CS#/RD#
        
        value = self.mcp.read_register(self.data)
        
        self.mcp.write_register(self.ctrl, self.CS_N | self.WE_N | self.RE_N | (index & 0x1f)) # Address precedes CS#
        
        self.mcp.write_register(self.mcp.IODIR_A, 0b00000000)

        return value
