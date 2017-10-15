ft60x-authenticate
------------------

This is a quick hack to switch the FT60x family of USB3 data pipes into DFU mode, which allows dumping and reflashing its firmware.

The FT60x contains an FT32 core and (at least) 64k of internal flash. Alas, the
low-level documentation for the FT32 (including the execution environment and
opcode semantics) is only available under NDA, so I didn't investigate this
further. Putting it up here in case anybody wants to have a look.

Usage
-----

- Attach the FT60x device (it should enumerate with the VID/PID pair 0403:601f, but this might be different on the FT600)
- Run `authenticate`
- The FT60x should reenumerate with VID/PID 0403:ffff, exposing a DFU interface
- Use the standard `dfu-util` to interact with the chip
