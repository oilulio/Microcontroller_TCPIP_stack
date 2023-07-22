#!/usr/bin/python3

# Quick MAC address adjustment script
# Takes a Net.hex file in Intel format
# Looks for a MAC address (strictly 6 consecutive bytes) of a known pattern

# Changes that MAC address as required and outputs a new Net.hex file of
# "NetXXXXXXXXXXXX.hex" where XX..XX is the new MAC in hex digits

# Avoids recompile to allows rapid production of many devices

# Fails if the 6-byte sequence is not unique in the file, or not in the file at all.

import sys
import re

NEW_MAC="AABBCCDDEEFF" # Hex digits (6 bytes)
OLD_MAC="324050607080"

with open("Net.hex") as f:
  lines=f.readlines()

lineStarts={}
code=""
newFile=[]
for n,line in enumerate(lines):
  newFile.append(line.rstrip())
  if (line[0]!=":"): continue
  recLen=int(line[1:3],16)
  if (line[7:9]!="00"): continue

  if (len(line.rstrip())!=(9+2*(recLen+1))): continue

  lineStarts[len(code)]=n
  code+=line[9:9+recLen*2]

allMACs=re.findall(OLD_MAC,code)

print (allMACs)

if (len(allMACs)>1):
  print ("MAC address not unique in file.  Exiting")
  sys.exit(0)

location=code.find(OLD_MAC)

if (location<0):
  print ("Old MAC address not detected.  Exiting")
  sys.exit(0)

print (location)
offset=0
while (not (location-offset) in lineStarts):
  offset+=2

sourceLine=location-offset
source=newFile[lineStarts[sourceLine]]
CS=int(source[-2:],16)
replacement=source[0:9+offset]
copyOffset=0

for i in range(6):
  if (9+offset+2*i+2>=len(source)): # move onto next line
    newFile[lineStarts[sourceLine]]=replacement+"{:02X}".format(CS&0xFF)
    print ("Replaced",lines[lineStarts[sourceLine]],"with   ",newFile[lineStarts[sourceLine]])
    sourceLine+=int(lines[lineStarts[location-offset]][1:3],16)*2
    source=newFile[lineStarts[sourceLine]]
    CS=int(source[-2:],16)
    offset=0
    copyOffset=i
    replacement=source[0:9+offset]
  replacement+=NEW_MAC[i*2:i*2+2]
  delta=int(OLD_MAC[i*2:i*2+2],16)-int(NEW_MAC[i*2:i*2+2],16)
  
  CS+=delta
  if (i==5):
    replacement+=source[9+offset+2*(i-copyOffset)+2:-2]
    newFile[lineStarts[sourceLine]]=replacement+"{:02X}".format(CS&0xFF)
    print ("Replaced",lines[lineStarts[sourceLine]],"with   ",newFile[lineStarts[sourceLine]])
    
f=open("Net"+NEW_MAC+".hex","w")
for line in newFile:
  f.write(line+"\n")
f.close()
print ("New file created ","Net"+NEW_MAC+".hex")

  
