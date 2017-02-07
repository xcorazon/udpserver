# -*- coding: utf-8 -*-
#import socket
#import sys
import struct
import time

from gevent.server import DatagramServer

# datagram types
T_OK       = 0
T_ERR      = 1
T_DATA     = 2
T_RESPONSE = 3

# error codes
E_OK             = 0x0
E_REQUEST_NUMBER = 0x10     # request number bigger then current
E_PACKET_NUMBER  = 0x11     # packet number bigger then posible


current_time = lambda: int(round(time.time() * 1000))

class Client(object):

    def __init__(self, id):
        self.id = id
        self.datagrams = []
        self.currentRequest = 0
        
        # request time in miliseconds
        self.requestTime = 0
        
        # max request time in miliseconds
        self.maxRequestTime = 10000
    
    
    def __eq__(self, other):
        return self.id == other
    
    
    def processDatagram(self, requestNum, count, packetNum, data):
        if requestNum < self.currentRequest and requestNum != 0:
            return E_OK
        
        if requestNum > self.currentRequest:
            self.datagrams = []
            return E_REQUEST_NUMBER
        
        # request = 0 is new connection
        if requestNum == 0:
            self.currentRequest = 0
        
        if len(self.datagrams) == 0:
            self.time = current_time()
            self.datagrams = [None] * count
        
        if packetNum < count:
            self.datagrams[packetNum] = data
            return E_OK
        
        return E_PACKET_NUMBER
      
      
    def requestData(self):
        if len(self.datagrams) == 0 or None in self.datagrams:
            return None
          
        res = buffer('')
        for data in self.datagrams:
            res = buffer(res) + data
        
        self.currentRequest += 1
        self.datagrams = []
        self.requestTime = current_time() - self.time
        
        if self.requestTime > self.maxRequestTime:
            return None
        
        return res




class UDPServer(DatagramServer):
  
    min_packet_size = 17
    clients = []
    
    
    def handle(self, datagram, address): # pylint:disable=method-hidden
        if len(datagram) < self.min_packet_size:
            return
          
        struc = struct.Struct("=BQIHH")
        type, id, request, count, num = struc.unpack(datagram[:17])
        
        if type != T_DATA:
            return
        if not (id in self.clients):
            self.clients.append(Client(id))
        i = self.clients.index(id)
        client = self.clients[i]
        
        data = buffer(datagram, self.min_packet_size, len(datagram))
        error = client.processDatagram(request, count, num, data)
        
        if error:
            self.sendError(address, error, id, request, count, num)
            return
        
        self.sendOk(address, id, request, count, num)
        
        data = client.requestData()
        #print data
      
        if data:
            id = struct.unpack("=B", data[:1])
            data = buffer(data, 1, len(data))
            #serverReceive.process_data(id, request, address, data)
      
    
    
    def sendOk(self, address, id, request, count, num):
        print "send ok"
        data = struct.pack("=BQIHH", T_OK, id, request, count, num)
        self.socket.sendto(data, address)
    
    
    
    def sendError(self, address, error, id, request, count, num):
        data = struct.pack("=BQIHHI", T_ERR, id, request, count, num, error)
        self.socket.sendto(data, address)
    
    
    
    def sendResponse(self, address, id, data):
        from datagramSender import DatagramSender
        sender = DatagramSender(address, self.socket)
        res, pack_id = sender.send(id, data)
        return res


if __name__ == '__main__':
    print('Receiving datagrams on :521')
    UDPServer(':521').serve_forever()