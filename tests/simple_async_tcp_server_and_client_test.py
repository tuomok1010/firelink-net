
'''
#!/usr/bin/env python3
import argparse
import os
import subprocess
import sys
import time
from pathlib import Path


def get_executables():
    if os.name != "nt":
        server_exe = Path(
            "../examples/simple_async_tcp_echo_server/build/"
            "simple_async_tcp_echo_server/debug/simple_async_tcp_echo_server"
        )
        client_exe = Path(
            "../examples/simple_async_tcp_echo_client/build/"
            "simple_async_tcp_echo_client/debug/simple_async_tcp_echo_client"
        )
    else:
        server_exe = Path(
            "../examples/simple_async_tcp_echo_server/build/"
            "simple_async_tcp_echo_server/debug/simple_async_tcp_echo_server.exe"
        )
        client_exe = Path(
            "../examples/simple_async_tcp_echo_client/build/"
            "simple_async_tcp_echo_client/debug/simple_async_tcp_echo_client.exe"
        )
    return server_exe, client_exe


def run_test(server_args: list[str], client_args: list[str]) -> int:
    server_exe, client_exe = get_executables()

    print("=== Testing Simple Async TCP Server and Client ===\n")
    print(f"Server args: {server_args}")
    print(f"Client args: {client_args}\n")

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
        bufsize=1,
    )

    time.sleep(1.5)  # Wait for server to start listening

    # === Start Client ===
    print("Starting client...\n")
    client_proc = subprocess.Popen(
        [str(client_exe)] + client_args,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
        bufsize=1,
    )

    # === Real-time Client Output ===
    print("--- Client Output ---")
    for line in client_proc.stdout:
        print(line, end="")

    client_exit = client_proc.wait()
    print("\n--- End Client Output ---\n")

    # === Capture remaining Server Output ===
    print("--- Server Output ---")
    for line in server_proc.stdout:
        print(line, end="")

    time.sleep(0.5)

    if server_proc.poll() is None:
        # First try gentle terminate
        server_proc.terminate()
        try:
            server_proc.wait(timeout=2)
        except subprocess.TimeoutExpired:
            # Force kill if it didn't die
            server_proc.kill()
            try:
                server_proc.wait(timeout=2)
            except subprocess.TimeoutExpired:
                pass  # best effort

    server_exit = server_proc.returncode
    if server_exit is None:
        server_exit = -1   # killed

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


def main():
    parser = argparse.ArgumentParser(
        description="Test simple async TCP echo server/client"
    )
    parser.add_argument(
        "--server-arg",
        action="append",
        default=[],
        help="Argument to pass to the server (can be used multiple times)",
    )
    parser.add_argument(
        "--client-arg",
        action="append",
        default=[],
        help="Argument to pass to the client (can be used multiple times)",
    )

    args = parser.parse_args()
    sys.exit(run_test(args.server_arg, args.client_arg))


if __name__ == "__main__":
    main()
'''

#!/usr/bin/env python3
import argparse
import os
import subprocess
import sys
import threading
import time
from pathlib import Path


def get_executables():
    base = Path(__file__).resolve().parent

    if os.name != "nt":
        server_exe = (
            base
            / "../examples/simple_async_tcp_echo_server/build/"
            / "simple_async_tcp_echo_server/debug/simple_async_tcp_echo_server"
        )
        client_exe = (
            base
            / "../examples/simple_async_tcp_echo_client/build/"
            / "simple_async_tcp_echo_client/debug/simple_async_tcp_echo_client"
        )
    else:
        server_exe = (
            base
            / "../examples/simple_async_tcp_echo_server/build/"
            / "simple_async_tcp_echo_server/debug/simple_async_tcp_echo_server.exe"
        )
        client_exe = (
            base
            / "../examples/simple_async_tcp_echo_client/build/"
            / "simple_async_tcp_echo_client/debug/simple_async_tcp_echo_client.exe"
        )

    return server_exe.resolve(), client_exe.resolve()


def drain_output(pipe, prefix: str = ""):
    """Continuously read and print lines from a pipe."""
    try:
        for line in pipe:
            print(f"{prefix}{line}", end="", flush=True)
    except Exception:
        pass  # pipe closed or process died


def run_test(server_args: list[str], client_args: list[str]) -> int:
    server_exe, client_exe = get_executables()

    print("=== Testing Simple Async TCP Server and Client ===\n")
    print(f"Server args: {server_args}")
    print(f"Client args: {client_args}\n")

    if not server_exe.exists() or not client_exe.exists():
        print("Error: Could not find executables")
        print(f"Server: {server_exe}")
        print(f"Client: {client_exe}")
        return 1

    server_proc = None
    client_proc = None

    try:
        # === Start Server ===
        print("Starting server...")
        server_proc = subprocess.Popen(
            [str(server_exe)] + server_args,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            bufsize=1,
        )

        server_thread = threading.Thread(
            target=drain_output,
            args=(server_proc.stdout, "[SERVER] "),
            daemon=True,
        )
        server_thread.start()

        time.sleep(1.5)

        # === Start Client ===
        print("Starting client...\n")
        client_proc = subprocess.Popen(
            [str(client_exe)] + client_args,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            bufsize=1,
        )

        client_thread = threading.Thread(
            target=drain_output,
            args=(client_proc.stdout, "[CLIENT] "),
            daemon=True,
        )
        client_thread.start()

        # Wait for client
        try:
            client_exit = client_proc.wait(timeout=30)
        except subprocess.TimeoutExpired:
            print("\nClient timed out after 30 seconds!")
            client_proc.kill()
            client_exit = -1

        print("\n--- Client finished ---\n")

        # Give server a short moment to exit cleanly if it wants to
        time.sleep(0.5)

        server_exit = server_proc.poll()

        # If server is still running, terminate it
        if server_exit is None:
            print("Server still running  terminating...")
            server_proc.terminate()
            try:
                server_exit = server_proc.wait(timeout=2)
            except subprocess.TimeoutExpired:
                print("Server did not terminate, killing...")
                server_proc.kill()
                try:
                    server_exit = server_proc.wait(timeout=2)
                except subprocess.TimeoutExpired:
                    server_exit = -1

        time.sleep(0.2)  # let drain threads finish

        # === Results ===
        print("\n=== Test Results ===")
        print(f"Client exit code : {client_exit}  {'OK' if client_exit == 0 else 'FAIL'}")
        print(f"Server exit code : {server_exit}  {'OK' if server_exit == 0 else 'FAIL'}")

        # Success = client succeeded AND server did not crash (non-zero)
        if client_exit == 0 and server_exit == 0:
            print("\nALL TESTS PASSED!")
            return 0
        else:
            print("\nTEST FAILED!")
            return 1

    finally:
        for proc in (client_proc, server_proc):
            if proc is not None and proc.poll() is None:
                proc.kill()
                try:
                    proc.wait(timeout=1)
                except subprocess.TimeoutExpired:
                    pass


def main():
    parser = argparse.ArgumentParser(
        description="Test simple async TCP echo server/client"
    )
    parser.add_argument(
        "--server-arg",
        action="append",
        default=[],
        help="Argument to pass to the server (can be used multiple times)",
    )
    parser.add_argument(
        "--client-arg",
        action="append",
        default=[],
        help="Argument to pass to the client (can be used multiple times)",
    )

    args = parser.parse_args()
    sys.exit(run_test(args.server_arg, args.client_arg))


if __name__ == "__main__":
    main()
