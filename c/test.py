import socket
import struct
data = """The server can optionally work in SSL mode when given the correct keyword arguments. 
(That is, the presence of any keyword arguments will trigger SSL mode.) On Python 2.7.9 and later 
(any Python version that supports the ssl.SSLContext), this can be done with a configured SSLContext. 
On any Python version, it can be done by passing the appropriate arguments for ssl.wrap_socket().
"""
sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

# prepare CONNECT request
buffer = struct.pack("=BQIHH", 3, 0, 0, 1, 0)
print len(buffer)
sock.sendto(buffer, ('54.165.59.155', 13555))

# recv client id
data1, serv = sock.recvfrom(1000)
print len(data1)
type, id, request_id, pack_count, subpack = struct.unpack("=BQIHH", str(data1))

# send ok
buffer = struct.pack("=BQIHH", 0, id, 0, 1, 0)
sock.sendto(buffer, ('54.165.59.155', 13555))