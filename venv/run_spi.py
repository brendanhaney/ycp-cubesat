#!/usr/bin/env python3
"""Launch Renode's GUI and C SPI adapter, or run a bounded headless simulation."""
import argparse
import pathlib
import socket
import subprocess
import time

ROOT = pathlib.Path(__file__).resolve().parents[1]


def run_simulation(build, module="all", test=False, headless=False):
    headless = headless or test
    build = pathlib.Path(build).resolve()
    build.mkdir(parents=True, exist_ok=True)
    log_path = build / ("spi-integration.log" if test else f"spi-{module}.log")
    # The external API is local co-simulation plumbing, not a firmware transport.
    with socket.socket() as reservation:
        reservation.bind(("127.0.0.1", 0))
        port = reservation.getsockname()[1]
    modules = ("avionics", "radio", "payload") if module == "all" else (module,)
    commands = ["include @venv/common.resc", f"$spi_port={port}"]
    for name in modules:
        commands.append(f"$global.{name}_elf=@{build / (name + '.elf')}")
        commands.append(f"include @venv/{name}.resc")
    commands.append("include @venv/connect.resc")
    options = ["--disable-gui", "--console", "--plain"] if headless else []
    adapter = None
    adapter_log_path = build / f"spi-{module}-adapter.log"
    with log_path.open("w") as log:
        renode = subprocess.Popen(
            ["renode", *options, "-e", "; ".join(commands)],
            cwd=ROOT, stdin=subprocess.PIPE if headless else subprocess.DEVNULL,
            stdout=log, stderr=subprocess.STDOUT,
            text=True,
        )
        try:
            deadline = time.monotonic() + 60
            while True:
                startup_log = log_path.read_text()
                if ("SPI_API_READY" in startup_log and
                        f"Listening for connections on port: {port}" in startup_log):
                    break
                if (renode.poll() is not None or time.monotonic() >= deadline or
                        "There was an error" in startup_log or
                        "Could not tokenize" in startup_log):
                    raise RuntimeError(f"Renode failed to start; see {log_path}")
                time.sleep(0.05)
            executable = build / ("spi-integration" if test else "spi-renode")
            if not headless:
                print("Renode GUI is open. The demo runs once, then pauses for inspection.", flush=True)
                print("Use the Renode monitor to resume; quit Renode to end the session.", flush=True)
                print(f"Logs: {log_path} and {adapter_log_path}", flush=True)
                with adapter_log_path.open("w") as adapter_log:
                    adapter = subprocess.Popen(
                        [str(executable), str(port), module, "--stay-connected"],
                        cwd=ROOT, stdin=subprocess.PIPE, stdout=adapter_log,
                        stderr=subprocess.STDOUT, text=True,
                    )
                    while renode.poll() is None:
                        if adapter.poll() is not None:
                            # A GUI quit also disconnects the adapter. Allow
                            # Renode's disposal to finish before reporting failure.
                            try:
                                renode.wait(timeout=5)
                            except subprocess.TimeoutExpired:
                                raise RuntimeError(f"SPI adapter exited; see {adapter_log_path}")
                            break
                        time.sleep(0.1)
                return log_path
            result = subprocess.run(
                [str(executable), str(port), module], cwd=ROOT,
                stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, timeout=120,
            )
            # Append after Renode exits, avoiding concurrent writes to one file.
            renode.communicate("quit\n", timeout=15)
            log.write(result.stdout)
            log.flush()
            if result.returncode:
                raise RuntimeError(f"C SPI adapter failed: {result.stdout}\nSee {log_path}")
        finally:
            if adapter is not None and adapter.poll() is None:
                adapter.terminate()
                try:
                    adapter.wait(timeout=10)
                except subprocess.TimeoutExpired:
                    adapter.kill()
                    adapter.wait()
            if renode.poll() is None:
                renode.terminate()
                try:
                    renode.wait(timeout=10)
                except subprocess.TimeoutExpired:
                    renode.kill()
                    renode.wait()
    return log_path


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build", default=ROOT / "build", type=pathlib.Path)
    parser.add_argument("--module", choices=("all", "avionics", "radio", "payload"), default="all")
    parser.add_argument("--headless", action="store_true", help="run without windows and exit after the demo")
    args = parser.parse_args()
    path = run_simulation(args.build, args.module, headless=args.headless)
    if args.headless:
        print(path.read_text())
    print(f"Renode log: {path}")
