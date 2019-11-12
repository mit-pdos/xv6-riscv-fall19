import socket
import sys

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
addr = ('localhost', int(sys.argv[1]))
print >>sys.stderr, 'listening on %s port %s' % addr
print >>sys.stderr, 'provide this port number as an arg to nettests inside xv6'
sock.bind(addr)

while True:
    buf, raddr = sock.recvfrom(4096)
    print >>sys.stderr, buf
    if buf:
        sent = sock.sendto(buf, raddr)
