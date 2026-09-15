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
            "name": "IPv4 1 packet per second",
            "server_args": ["--server", "127.0.0.1:63000"],
            "client_args": [
                "--server", "127.0.0.1:63000",
                "--client", "127.0.0.1:63001",
                "--message", "test",
                "--num-packets", "3",
                "--interval", "1000"
            ],
        },
        {
            "name": "IPv6 1 packet per second",
            "server_args": ["--server", "[::1]:63002"],
            "client_args": [
                "--server", "[::1]:63002",
                "--client", "[::1]:63003",
                "--message", "test",
                "--num-packets", "3",
                "--interval", "1000"
            ],
        },
        {
            "name": "Dual-stack 1 packet per second",
            "server_args": ["--server", "[::]:63004", "--dual-stack"],
            "client_args": [
                "--server", "127.0.0.1:63004",
                "--client", "127.0.0.1:63005",
                "--message", "test",
                "--num-packets", "3",
                "--interval", "1000"
            ],
        },

        # Test 5  packets per second
        {
            "name": "IPv4 5 packets per second",
            "server_args": ["--server", "127.0.0.1:63006"],
            "client_args": [
                "--server", "127.0.0.1:63006",
                "--client", "127.0.0.1:63007",
                "--message", "test",
                "--num-packets", "10",
                "--interval", "200"
            ],
        },
        {
            "name": "IPv6 5 packets per second",
            "server_args": ["--server", "[::1]:63008"],
            "client_args": [
                "--server", "[::1]:63008",
                "--client", "[::1]:63009",
                "--message", "test",
                "--num-packets", "10",
                "--interval", "200"
            ],
        },
        {
            "name": "Dual-stack 5 packets per second",
            "server_args": ["--server", "[::]:63010", "--dual-stack"],
            "client_args": [
                "--server", "127.0.0.1:63010",
                "--client", "127.0.0.1:63011",
                "--message", "test",
                "--num-packets", "10",
                "--interval", "200"
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
