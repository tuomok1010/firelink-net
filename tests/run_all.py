#!/usr/bin/env python3
"""
Run the async TCP test script with several different configurations.
"""

import subprocess
import sys
import time
from pathlib import Path

# Adjust the name if your test script has a different filename
TEST_SCRIPT = Path(__file__).parent / "simple_async_tcp_server_and_client_test.py"


MSG_BYTE   = "0"
MSG_1KB    = "A" * 1024
MSG_4KB    = "B" * 4096
MSG_16KB   = "C" * 16384


def run_one(name: str, server_args: list[str], client_args: list[str]) -> int:
    print("\n" + "=" * 70)
    print(f">>> Running test: {name}")
    print("=" * 70)

    cmd = [sys.executable, str(TEST_SCRIPT)]

    for arg in server_args:
        cmd.append(f"--server-arg={arg}")

    for arg in client_args:
        cmd.append(f"--client-arg={arg}")

    print("Command:", " ".join(cmd))          # optional: helpful for debugging
    result = subprocess.run(cmd)
    return result.returncode


def main() -> int:
    tests = [
        # Test 1 packet  per second
        {
            "name": "IPv4 1 packet (1B) per second",
            "server_args": ["--server", "127.0.0.1:63000"],
            "client_args": [
                "--server", "127.0.0.1:63000",
                "--client", "127.0.0.1:63001",
                "--message", MSG_BYTE,
                "--num-packets", "3",
                "--interval", "1000"
            ],
        },
        {
            "name": "IPv6 1 packet (1B) per second",
            "server_args": ["--server", "[::1]:63002"],
            "client_args": [
                "--server", "[::1]:63002",
                "--client", "[::1]:63003",
                "--message", MSG_BYTE,
                "--num-packets", "3",
                "--interval", "1000"
            ],
        },
        {
            "name": "Dual-stack IPv4 1 packet (1B) per second",
            "server_args": ["--server", "[::]:63004", "--dual-stack"],
            "client_args": [
                "--server", "127.0.0.1:63004",
                "--client", "127.0.0.1:63005",
                "--message", MSG_BYTE,
                "--num-packets", "3",
                "--interval", "1000"
            ],
        },
        {
            "name": "Dual-stack IPv6 1 packet (1B) per second",
            "server_args": ["--server", "[::]:63006", "--dual-stack"],
            "client_args": [
                "--server", "[::1]:63006",
                "--client", "[::1]:63007",
                "--message", MSG_BYTE,
                "--num-packets", "3",
                "--interval", "1000"
            ],
        },

        # Test 5  packets (1KB) per second
        {
            "name": "IPv4 5 packets (1KB) per second",
            "server_args": ["--server", "127.0.0.1:63008"],
            "client_args": [
                "--server", "127.0.0.1:63008",
                "--client", "127.0.0.1:63009",
                "--message", MSG_1KB,
                "--num-packets", "10",
                "--interval", "200"
            ],
        },
        {
            "name": "IPv6 5 packets (1KB) per second",
            "server_args": ["--server", "[::1]:63010"],
            "client_args": [
                "--server", "[::1]:63010",
                "--client", "[::1]:63011",
                "--message", MSG_1KB,
                "--num-packets", "10",
                "--interval", "200"
            ],
        },
        {
            "name": "Dual-stack IPv4 5 packets (1KB) per second",
            "server_args": ["--server", "[::]:63012", "--dual-stack"],
            "client_args": [
                "--server", "127.0.0.1:63012",
                "--client", "127.0.0.1:63013",
                "--message", MSG_1KB,
                "--num-packets", "10",
                "--interval", "200"
            ],
        },
        {
            "name": "Dual-stack IPv6 5 packets (1KB) per second",
            "server_args": ["--server", "[::]:63014", "--dual-stack"],
            "client_args": [
                "--server", "[::1]:63014",
                "--client", "[::1]:63015",
                "--message", MSG_1KB,
                "--num-packets", "10",
                "--interval", "200"
            ],
        },

        # Test 50  packets (4KB) per second
        {
            "name": "IPv4 50 packets (4KB) per second",
            "server_args": ["--server", "127.0.0.1:63016"],
            "client_args": [
                "--server", "127.0.0.1:63016",
                "--client", "127.0.0.1:63017",
                "--message", MSG_4KB,
                "--num-packets", "10",
                "--interval", "20"
            ],
        },
        {
            "name": "IPv6 50 packets (4KB) per second",
            "server_args": ["--server", "[::1]:63018"],
            "client_args": [
                "--server", "[::1]:63018",
                "--client", "[::1]:63019",
                "--message", MSG_4KB,
                "--num-packets", "10",
                "--interval", "20"
            ],
        },
        {
            "name": "Dual-stack IPv4 50 packets (4KB) per second",
            "server_args": ["--server", "[::]:63020", "--dual-stack"],
            "client_args": [
                "--server", "127.0.0.1:63020",
                "--client", "127.0.0.1:63021",
                "--message", MSG_4KB,
                "--num-packets", "10",
                "--interval", "20"
            ],
        },
        {
            "name": "Dual-stack IPv6 50 packets (4KB) per second",
            "server_args": ["--server", "[::]:63022", "--dual-stack"],
            "client_args": [
                "--server", "[::1]:63022",
                "--client", "[::1]:63023",
                "--message", MSG_4KB,
                "--num-packets", "10",
                "--interval", "20"
            ],
        },

        # Test 100  packets (16KB) no delay
        {
            "name": "IPv4 100 packets (16KB) no delay",
            "server_args": ["--server", "127.0.0.1:63024"],
            "client_args": [
                "--server", "127.0.0.1:63024",
                "--client", "127.0.0.1:63025",
                "--message", MSG_16KB,
                "--num-packets", "100",
                "--interval", "1"
            ],
        },
        {
            "name": "IPv6 100 packets (16KB) no delay",
            "server_args": ["--server", "[::1]:63026"],
            "client_args": [
                "--server", "[::1]:63026",
                "--client", "[::1]:63027",
                "--message", MSG_16KB,
                "--num-packets", "100",
                "--interval", "1"
            ],
        },
        {
            "name": "Dual-stack IPv4 100 packets (16KB) no delay",
            "server_args": ["--server", "[::]:63028", "--dual-stack"],
            "client_args": [
                "--server", "127.0.0.1:63028",
                "--client", "127.0.0.1:63029",
                "--message", MSG_16KB,
                "--num-packets", "100",
                "--interval", "1"
            ],
        },
        {
            "name": "Dual-stack IPv6 100 packets (16KB) no delay",
            "server_args": ["--server", "[::]:63030", "--dual-stack"],
            "client_args": [
                "--server", "[::1]:63030",
                "--client", "[::1]:63031",
                "--message", MSG_16KB,
                "--num-packets", "100",
                "--interval", "1"
            ],
        },
    ]

    failed = []

    for test in tests:
        rc = run_one(test["name"], test["server_args"], test["client_args"])
        if rc != 0:
            failed.append(test["name"])
        time.sleep(3)

    print("\n" + "=" * 70)
    print("SUMMARY")
    print("=" * 70)

    if not failed:
        print("ALL TEST CONFIGURATIONS PASSED!")
        return 0
    else:
        print("FAILED configurations:")
        for name in failed:
            print(f"  - {name}")
        return 1


if __name__ == "__main__":
    sys.exit(main())
