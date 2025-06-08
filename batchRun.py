import subprocess
import re
import csv

command = "make clean"
subprocess.run(
    command, shell=True, capture_output=True, text=True)
command = "make serial"
subprocess.run(
    command, shell=True, capture_output=True, text=True)
command = "make run_serial"


output_file = 'Serial_513_513.csv'
with open(output_file, 'w', newline='') as f:
    writer = csv.writer(f)
    writer.writerow(['Test Case', 'Time (s)'])

    times = []
    time = 0
    for i in range(3):
        result = subprocess.run(
            command, shell=True, capture_output=True, text=True)
        output = result.stdout + result.stderr
        match = re.search(r"Ran in \((\d+\.\d+)\) seconds", output)
        if match:
            time = float(match.group(1))
        else:
            print(f"WARNING: could not find timing in run {i+1}")
            continue

        # appending to times list for calculating average later
        times.append(float(time))

        print(f"Test Case {i}:")
        print("Time:", time)
        writer.writerow([i + 1, time])
    avgTime = sum(times)/len(times)  # calculate average time
    print("Times:", times)
    print("Average Time:", avgTime)
    writer.writerow(['Average', avgTime])
