# -*- coding: utf-8 -*-
from gevent import monkey
from gevent import socket

monkey.patch_all()

import socket
import sys
import struct

import time
from server import T_OK, T_ERR, T_DATA

import sys, traceback

current_time = lambda: int(round(time.time() * 1000))

SEND_OK            = 0
SEND_ERROR         = 1
SEND_ERROR_TIMEOUT = 2

MAX_PACKET_SIZE = 8000

class DatagramSender(object):
  
    sock = None
    
    timeout = 5
    max_packet_size = 8000
    
    packet_delivering_time = 0.05
    
    start_sending_time = 0
    sending_time = 0
    request = 0
    
    
    def __init__(self, server_address, sock = None):
        self.server_address = server_address
        if sock:
            self.sock = sock
            self.closeSocketOnDelete = False
        else:
            self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            self.sock.settimeout(self.packet_delivering_time)
            self.closeSocketOnDelete = True
    
    
    def send(self, id, data):
        self.prepareData(data)
        
        self.id = id
        self.start_sending_time = current_time()
        self.sendPackets()
        
        return self.checkDelivering()
    
    
    def sendPackets(self, type = None):
        packNumber = 0
        
        if type:
            packetType = type
        else:
            packetType = T_DATA
          
        for data in self.packets:
            if not self.sended[packNumber]:
                header = struct.pack("=BQIHH", packetType, self.id, self.request, len(self.packets), packNumber)
                self.sock.sendto(buffer(header) + data, self.server_address)
                #self.checkResponse()
          
          packNumber += 1
    
    
    def prepareData(self, data):
        self.packets = []
        
        rest = data
        while len(rest) > self.max_packet_size:
            pack = buffer(rest, 0, self.max_packet_size)
            self.packets.append(pack)
            rest = buffer(rest, self.max_packet_size, len(rest))
          
        self.packets.append(rest)
        self.sended = [False] * len(self.packets)
    
    
    def checkDelivering(self):
        delta = current_time() - self.start_sending_time
        while int(self.timeout * 1000) > delta:
            try:
                data, server = self.sock.recvfrom(MAX_PACKET_SIZE)
            except:
                self.sendPackets()
                delta = current_time() - self.start_sending_time
                continue
          
          if server[0] == self.server_address[0] and server[1] == self.server_address[1]:
              type, id, request, count, num = struct.unpack("=BQIHH", data)
              if id == self.id:
                  if type == T_OK:
                      self.sended[num] = True
                  elif type == T_ERROR:
                      return SEND_ERROR, request
                
          if not False in self.sended:
              self.sending_time = current_time() - self.start_sending_time
              return SEND_OK, None
          
        return SEND_ERROR_TIMEOUT, None
    
    
    def __del__(self):
        if self.sock and self.closeSocketOnDelete:
            self.sock.close()
      
sender = DatagramSender(('127.0.0.1', 521))
sender.max_packet_size = 100
print sender.send(10, 'Hello!!!')
