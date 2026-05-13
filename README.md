# OpenFader
A project to modularize motorized faders with serial daisy chaining.

This board gets soldered onto the back of a motorized fader, and handles the control system aspect of the motorized faders. This board will handle PID control, with different modes and configuration options.
Each board has two UART connections (TX and RX). One UART connection is the upstream connection and connects to the previous fader or to the controller. The other UART connection is the downstream connection and connects to the following fader. When data flows from the controller to the first fader, the controller sends an integer fader address followed by data for that specific fader address. If this integer is not zero, the current fader modifies this address by subtracting 1, then sends the data on to the next fader. This process continuous until the address is zero. At this point, the receiving fader knows that the data is for itself, and it processes the data. Returning data from faders follows the same process but is reversed. Data requested from the fader comes after an address of zero. Each following fader in the path of the data increments this address on its way to the cotnroller. When this address and data gets to the controller, the address will have been incremented such that the controller knows exactly which fader sent this data.

For more protocol details, see the [protocol.md](protocol.md) file.

https://github.com/user-attachments/assets/ef170d1e-9a4b-43a3-9d26-493bcd82ac08

![PXL_20251116_050939562 RAW-01 MP COVER](https://github.com/user-attachments/assets/bc069402-9d71-43b5-a8d1-c1ee2bd488f5)

I've currently only designed a version for the Behringer MF60T and MF100T, which are some of the most economical motorized faders on the market. For other faders with different pin locations, the circuit is identical, but the locations of the pads will have to be redesigned. For testing, you can always use wires to attach the appropriate PCB pads to the fader.

A PCB is on the way, and when it gets to me I can start developing firmware.
