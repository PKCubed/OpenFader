<img width="1532" height="750" alt="image" src="https://github.com/user-attachments/assets/91fc9a4c-a72d-4707-a795-fd21c87dde00" />

## Hardware
Each fader has two UART connections. One UART connection is upstream, and the other is downstream. This allows an OpenFader board to modify incomming UART data before its sent to the next fader.

## UART Overview
According to the UART protocol defined [here](https://www.analog.com/en/resources/analog-dialogue/articles/uart-a-hardware-communication-protocol.html), a data packet transmission starts with a start bit. During idle state, the data transmission line is held at a high voltage level. This start bit pulls the line low for one clock cycle. Following this start bit are databits. This project uses 8 data bits. Following the data bits is the parity bit which is calculated from the data bits describing the even/oddness of the data, allowing the receiver to detect data validity or corruption. After the parity bit there are 1 to 2 stop bits. These stop bits are high voltage levels.

## UART Daisy-chained Structure
The OpenFader protocol uses two start bytes. These two bytes make up a 16 bit address. Following the start bytes are the payload data. This can be any number of bytes, but these bytes must not match the end byte. After these data bytes is the end byte `0xFF`.

As for the two start bytes, the controller sends out an address as to which OpenFader board in the chain it wants to talk to. This address, anywhere from `0x0000` to `0xFFFE` is sent to the first fader in the chain. When a board receives data, it saves it up until it sees the end byte. At this point, it checks the data it saved. It looks at the address bytes. If the address is `0x0000`, it knows that the data it received was designated for it. It then processes the data, and does not send anything on to the next fader in the chain. If the address is non-zero, it decrements the address by 1, then sends the entire data transmission to the next fader.

When the controller needs data from a fader (position, touch-state, etc.), it will send a request for data using this transmission method. The fader in question will then process the request, and will send its data back. However, instead of sending its actual address in the chain, it sends an address of zero. Then, each following fader on the upstream path back to the controller will increment the address bytes by 1. By the time the controller receives the data, it will see the actual address of the fader that sent the data.

## Data Structure

| Data Byte 0 <br> (Control) | Description         |
| ----------- | ------------------- |
| `0x00`      | Read Position       |
| `0x01`      | Read Error          |
| `0x02`      | Read Touch          |
| `0x10`      | Write Position      |
| `0x11`      | Write LED Color     |
| `0x12`      | Write Configuration |

## Timing Statistics
Inherent to the method of addressed data transfer used by OpenFader, things are rather inefficient. Each fader in the chain has to buffer the data before it gets to the fader at the end. This causes data transactions to take many uart clock cycles to be completed.

For a 32 fader system assuming a 6 byte transaction <!-- (`Addr0`, `Addr1`, `Control`, `Data1`, `Data2`, `End`) -->running at 115200 baud, a round-trip data transmission to the last fader in the chain would take ___ clock cycles.

A single byte as a UART transaction takes 12 bits. Projecting to a 6 byte transaction, this would take 72 uart clock cycles.

Allowing for a 12 uart cycle processing period, it would take 84*32=2688 uart clock cycles to get the data to the last fader and let the fader process the data. 2688 clock cycles at 115200 Hz gives a transmit time of ~23ms.
