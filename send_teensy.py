import socket          # Import socket module
import time
import sys

addr = "gevcu7.local"
if (len(sys.argv) > 1):
    addr = sys.argv[1]

print("Starting firmware update process with address " + addr)
s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
port = 23                 # telnet
print("About to connect")
s.connect((addr, port))
time.sleep(1)
f = open('GEVCU7.hex','rb')
print('Beginning transfer...')
s.send(bytes.fromhex("A5"))
time.sleep(1)
l = f.readline()
p = 0
lin = 0

while (l):
    s.send(l)
    lin = lin + 1
    if lin > 1000:
        lin = 0
        print('Still transferring...')
    while (s.recv(1) != bytes.fromhex("97")):
        p = p + 1
    l = f.readline()

time.sleep(5)
f.close()
print("Done Sending Firmware")
s.close                     # Close the socket when done
