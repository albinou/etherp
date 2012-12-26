Ethernet Ping (Etherp)
======================

This project allows you to generate and send Ethernet datagrams between two
network interfaces. It is actually composed of two programs:

* _etherp-send_ -- in charge of sending Ethernet frames
* _etherp-recv_ -- in charge of receiving them

These two tools, used together, can help you to test an Ethernet link or a
network driver by detecting loss of frames at different speeds. This is not
actually a ping protocol as frames are transfered only on one way (there is no
pong).

Let's consider the following test bench with two computers with their Network
Interfaces (NIC) directly connected:

    __________                __________
    |        |                |        |
    |  PC1 __|_____       ____|__ PC2  |
    |______| NIC1 |-------| NIC2 |_____|
           '------'       '------'

_etherp-send_ usage
-------------------

_etherp-send_ needs to be run as root (or at least with the _CAP_NET_RAW_
capability) on the machine sending Ethernet frames (let's say PC1 with its MAC
address being `00:24:e8:00:00:01`):

    etherp-send -I eth0 00:24:e8:00:00:01

For more help on usage, run the command `etherp-send --help`.

_etherp-recv_ usage
-------------------

_etherp-recv_ also needs to be run as root (or at least with the _CAP_NET_RAW_
capability) on the machine receiving Ethernet frames (PC2):

    etherp-recv -I eth0

For more help on usage, run the command `etherp-recv --help`.

Note that _etherp-recv_ receives all frames with _etherp_ protocol. This means
that it would receives frames from several _etherp-send_ instances. However,
using several _etherp-send_ instances is not supported by the protocol (cf. next
section).

Protocol details
----------------

_etherp_ is an Ethernet protocol using the Ethertype 0x4242. Frames contains the
following fields:

* id (4 bytes): identifier (increasing as long as frames are sent)
* crc32 (4 bytes): Checksum of data
* stop (1 byte): boolean indicating if this is the last frame sent by the sender
* data: generated data whose size is the rest of the Ethernet frame

All fields are transmitted using network byte order.
