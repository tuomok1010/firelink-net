#!/usr/bin/env python3
import subprocess
import time
import sys
from pathlib import Path
import os

def run_test():
    print("=== Testing Simple Async TCP Server and Client (IPv4) ===\n")

    # Default paths
    if os.name != "nt":
        server_exe = Path("../examples/simple_async_tcp_echo_server/build/simple_async_tcp_echo_server/debug/simple_async_tcp_echo_server")
        client_exe = Path("../examples/simple_async_tcp_echo_client/build/simple_async_tcp_echo_client/debug/simple_async_tcp_echo_client")
    else:
        server_exe = Path("../examples/simple_async_tcp_echo_server/build/simple_async_tcp_echo_server/debug/simple_async_tcp_echo_server.exe")
        client_exe = Path("../examples/simple_async_tcp_echo_client/build/simple_async_tcp_echo_client/debug/simple_async_tcp_echo_client.exe")

    # Server args
    server_args = [
        "--server", "127.0.0.1:63000"
    ]

    # Client args
    client_args = [
        "--server", "127.0.0.1:63000",
        "--client", "127.0.0.1:63001"
    ]

    if not server_exe.exists() or not client_exe.exists():
        print("Error: Could not find executables")
        print(f"Server: {server_exe}")
        print(f"Client: {client_exe}")
        return 1

    # === Start Server ===
    print("Starting server...")
    server_proc = subprocess.Popen(
        [str(server_exe)] + server_args,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
        bufsize=1
    )

    time.sleep(1.5)  # Wait for server to start listening

    # === Start Client ===
    print("Starting client...\n")

    client_proc = subprocess.Popen(
        [str(client_exe)] + client_args,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
        bufsize=1
    )

    # === Real-time Client Output ===
    print("--- Client Output ---")
    client_output = []
    for line in client_proc.stdout:
        print(line, end='')
        client_output.append(line)

    client_exit = client_proc.wait()
    print("\n--- End Client Output ---\n")

    # === Capture remaining Server Output ===
    print("--- Server Output ---")
    server_output = []
    # Read any remaining output from server
    for line in server_proc.stdout:
        print(line, end='')
        server_output.append(line)

    # Wait a bit more and force terminate if still running
    time.sleep(0.8)
    if server_proc.poll() is None:
        server_proc.terminate()
        try:
            server_proc.wait(3)
        except subprocess.TimeoutExpired:
            server_proc.kill()

    server_exit = server_proc.returncode
    print("\n--- End Server Output ---\n")

    # === Test Results ===
    print("=== Test Results ===")
    print(f"Client exit code : {client_exit} {'OK' if client_exit == 0 else 'FAIL'}")
    print(f"Server exit code : {server_exit} {'OK' if server_exit == 0 else 'FAIL'}")

    if client_exit == 0 and server_exit == 0:
        print("\nALL TESTS PASSED!")
        return 0
    else:
        print("\nTEST FAILED!")
        return 1


if __name__ == "__main__":
    sys.exit(run_test())
