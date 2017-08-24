import socket
import sys

def send_msg(msg):
    s = socket.socket()
    s.connect(('127.0.0.1', 3000))
    s.send(msg.encode())

if __name__ == '__main__':
    msg = sys.argv[1]
    send_msg(msg)
