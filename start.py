#!/usr/bin/python3

import argparse
import sys
import struct
from socket import *
from typing import List

ICMP_ECHO = 8
ICMP_CODE = 0
ICMP_IDENT = 12345

HEADER_LENGTH = 8
TARGET_LENGTH = 1200


def calculate_checksum(msg: bytes) -> int:
    """
    Simplified checksum calculation for even-length packets
    """
    s = 0
    for n, i in enumerate(msg):
        s += i << ((n % 2) * 8)
        s = (s & 0xffff) + (s >> 16)

    return htons(~s & 0xffff)


#  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
#  |     Type      |     Code      |          Checksum             |
#  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
#  |           Identifier          |        Sequence Number        |
#  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
#
def make_packet(csum: int, seq: int, data: bytes) -> bytes:
    return struct.pack(">BBHHH", ICMP_ECHO, ICMP_CODE, csum, ICMP_IDENT, seq) + data


def send_initial_ping(data: bytes, destination: str) -> None:
    """
    Create and send the initial seed ping
    """
    ping_socket = socket(AF_INET, SOCK_RAW, getprotobyname("icmp"))

    # Checksum field of 0 goes into calculating checksum
    packet = make_packet(csum=0, seq=0, data=data)
    checksum = calculate_checksum(packet)

    # ... again, but this time with proper checksum
    packet = make_packet(csum=checksum, seq=0, data=data)

    ping_socket.sendto(packet, (destination, 0))
    ping_socket.close()


def load_program(filename: str) -> bytes:
    result = bytes()
    memory_values = open(filename).read().splitlines()

    for number in memory_values:
        number = int(number[::-1], 2)
        result += number.to_bytes(4, byteorder='little')

    return result


if __name__ == "__main__":
    parser = argparse.ArgumentParser()

    parser.add_argument("-t", "--target", type=str, required=True,
                        help="Target IP to send ICMP packet to")
    parser.add_argument("-p", "--program", type=str, required=True,
                        help="Program file to execute")

    args = parser.parse_args()

    data = load_program(args.program)

    # Padding to the correct packet size
    data += bytes(TARGET_LENGTH - len(data) - HEADER_LENGTH)

    send_initial_ping(data, args.target)
    print("Started!")
