import subprocess
import threading
import signal
import os
import sys

#####################################################
commands = [
    ["./../usbcam/build/usbcam", sys.argv[1], sys.argv[3], "5000", "30", "640", "480"],
    ["./../usbcam/build/usbcam", sys.argv[2], sys.argv[3], "5001", "30", "640", "480"]
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
