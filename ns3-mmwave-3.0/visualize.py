import matplotlib.pyplot as plt
from matplotlib.ticker import MaxNLocator
import numpy as np
import os

DASH_DIR = "dash-log-files"

while True:
  algorithms = os.listdir(DASH_DIR)
  print(algorithms)

  algo = input("Choose an algorithm:")
  if (algo not in algorithms):
    print("That algorithm doesn't exist")
    continue
  break

numClients = os.listdir(DASH_DIR+"/"+algo)

while True:
  print("Number of clients:")
  print(numClients)

  clientNum = input("Plot for which number of clients?")
  if (clientNum not in numClients):
    print("That number of clients doesn't exist")
    continue
  break

#TODO: check sim id?

directory = DASH_DIR+"/"+algo+"/"+clientNum+"/"
# Read adaptation log
adaptationLog = open(directory+"sim1_cl0_adaptationLog.txt", "r")    
adaptationX = []
adaptationY = []

# skip first line
line = adaptationLog.readline()
line = adaptationLog.readline()
while line:
  segIndex, rep, time, case, delayCase = line.split()
  adaptationX.append(int(segIndex))
  adaptationY.append(int(rep))
  line = adaptationLog.readline()

adaptationLog.close()

# read buffer log
bufferLog = open(directory+"sim1_cl0_bufferLog.txt", "r")
bufferX = []
bufferY = []
# skip first line
line = bufferLog.readline()
line = bufferLog.readline()
while line:
  timeNow, bufferLevel = line.split()
  bufferX.append(float(timeNow))
  bufferY.append(float(bufferLevel))
  line = bufferLog.readline()
bufferLog.close()

# read throughput log
throughputLog = open(directory+"sim1_cl0_throughputLog.txt", "r")
throughputX = []
throughputY = []
# skip first line
line = throughputLog.readline()
line = throughputLog.readline()
while line:
  timeNow, bandwidth = line.split()
  timeNow = float(timeNow)
  bandwidth = int(bandwidth)

  if (timeNow not in throughputX):
    throughputX.append(timeNow)
    throughputY.append(bandwidth)
  line = throughputLog.readline()
throughputLog.close()

fig, axis = plt.subplots(3, 1)
plt.tight_layout()
axis[0].plot(adaptationX, adaptationY)
axis[0].set_title("Requested Quality vs. Segment Index")
axis[0].set(xlabel="Segment Index", ylabel="Requested Quality")
axis[0].xaxis.set_major_locator(MaxNLocator(integer=True))

axis[1].plot(bufferX, bufferY)
axis[1].set_title("Buffer Level over Time")
axis[1].set(xlabel="Time (s)", ylabel="Buffer Level (s)")
axis[1].xaxis.set_major_locator(MaxNLocator(15))
axis[1].yaxis.set_major_locator(MaxNLocator(15))

axis[2].plot(throughputX, throughputY)
axis[2].set_title("Throughput over Time")
axis[2].set(xlabel="Time (s)", ylabel="Bandwidth (bytes)")
axis[2].xaxis.set_major_locator(MaxNLocator(15))
axis[2].yaxis.set_major_locator(MaxNLocator(15))


plt.show()

