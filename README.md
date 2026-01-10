# firelink-net
Async network library

## License
This work is released under the AGPL-3.0 license (LICENSE file for more details) except for forgescript related files. These files are:

async_echo_server.fbs.debug.bat

async_echo_client.fbs.debug.bat

async_echo_server.fbs.debug.bat

firelink.fbs.debug.bat

... and anything in the forgecript/ folder.

For more information about the build system used, view: https://github.com/tuomok1010/forgescript-build-system

## IMPORTANT
- Under heavy development. Expect bugs and massive changes.
- "examples" folder contains crude examples on how to use the async sockets.

## Features
- Synchronous and asynchronous socket operations
- User defined handlers for socket operation completions
- Platform independent design (Currently Windows only)
- Dual threadpool design (user handlers and socket IO are separated)
  
## How to build and run
### firelink
Run firelink.fbs.debug.bat

### examples:
Run echo_client.fbs.debug.bat and/or echo_server.fbs.debug.bat.

Copy firelink.dll from firelink build dir into the example build folder.

Run async_echo_server.exe

Run async_echo_client.exe

Server will listen on port 63000. Client connects and sends a test string to the server. Server prints the message and sends a reply. Client prints the reply. Connections are closed. NOTE: The test programs are built around a while(true) loop. This will be replaced later with a proper system.

## Future plans
- IOCore class which will handle threadpools and events.
- Linux implementation (io_uring or similar)
