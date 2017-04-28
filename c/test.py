import socket
data = """The server can optionally work in SSL mode when given the correct keyword arguments. 
(That is, the presence of any keyword arguments will trigger SSL mode.) On Python 2.7.9 and later 
(any Python version that supports the ssl.SSLContext), this can be done with a configured SSLContext. 
On any Python version, it can be done by passing the appropriate arguments for ssl.wrap_socket().
"""
sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.sendto(data, ('54.165.59.155', 13555))