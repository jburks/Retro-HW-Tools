from mcp23017 import MCP23017
from vera_intf import VeraIntf
from vypera import VERA

mcp = MCP23017(0x20, 1)     # The MCP23017 is on i2c bus 1, address 0x20
mcp.init()                  # Initialize the GPIO extender
vera_bus = VeraIntf(mcp)    # Create the register bus interface needed by the VERA class

vera = VERA(vera_bus)

vera.RESET = 1  # Reset VERA

while(True):    # Wait for VERA to start responding
    vera.ADDR_L = 42
    if vera.ADDR_L == 42:
        print("VERA is responding.")
        break
    else:
        print("Waiting for VERA to respond.")
