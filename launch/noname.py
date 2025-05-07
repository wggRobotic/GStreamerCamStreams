import subprocess
import threading
import signal
import os
import sys

#####################################################
commands = [
    ["./../realsense/build/realsense", sys.argv[1], "7001", "7002", "60", "640", "480", "30", "640", "480", "2000.0"],
    ["./../oak/build/oak", sys.argv[1], "7000"]
]
#####################################################

processes = []

def run_program(command):
    """Runs a program with arguments and adds it to the process list."""
    process = subprocess.Popen(command)
    processes.append(process)
    process.wait()

threads = []
for command in commands:
    thread = threading.Thread(target=run_program, args=(command,))
    thread.start()
    threads.append(thread)

input("Press Enter to stop ...\n")

for process in processes:
    os.kill(process.pid, signal.SIGTERM)

for thread in threads:
    thread.join()

print("Stopped.")
