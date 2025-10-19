#!/usr/bin/env python3
"""
Python UDP Server - Message Forwarder
Receives commands from C3 and forwards to WROOM32
"""

import socket
import time
from datetime import datetime

# Server configuration
SERVER_IP = '0.0.0.0'  # Listen on all interfaces
SERVER_PORT = 4210

# Client tracking
c3_address = None
wroom_address = None

# Statistics
packets_received = 0
packets_forwarded = 0

def print_header():
    print("\n" + "="*50)
    print("     UDP SERVER - ESP32 MESSAGE FORWARDER")
    print("="*50)
    print(f"Server IP: {SERVER_IP}")
    print(f"Server Port: {SERVER_PORT}")
    print("="*50 + "\n")

def print_status():
    print("\n" + "-"*50)
    print(f"C3 Address: {c3_address if c3_address else 'Not connected'}")
    print(f"WROOM Address: {wroom_address if wroom_address else 'Not connected'}")
    print(f"Packets Received: {packets_received}")
    print(f"Packets Forwarded: {packets_forwarded}")
    print("-"*50 + "\n")

def main():
    global c3_address, wroom_address, packets_received, packets_forwarded
    
    print_header()
    
    # Create UDP socket
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.bind((SERVER_IP, SERVER_PORT))
    
    print(f"✅ Server started successfully!")
    print(f"🎧 Listening on port {SERVER_PORT}...")
    print("\nWaiting for clients to connect...\n")
    
    last_status_print = time.time()
    
    try:
        while True:
            # Receive data
            data, addr = sock.recvfrom(1024)
            packets_received += 1
            
            try:
                message = data.decode('utf-8').strip()
                timestamp = datetime.now().strftime("%H:%M:%S.%f")[:-3]
                
                # Identify client type based on message
                if message.startswith("C3:"):
                    # Message from C3 (sender)
                    c3_address = addr
                    command = message.split(":", 1)[1] if ":" in message else message
                    
                    print(f"[{timestamp}] 📤 C3 → Server: '{command}' from {addr[0]}:{addr[1]}")
                    
                    # Forward to WROOM32 if connected
                    if wroom_address:
                        sock.sendto(command.encode('utf-8'), wroom_address)
                        packets_forwarded += 1
                        print(f"[{timestamp}] 📥 Server → WROOM: '{command}' to {wroom_address[0]}:{wroom_address[1]}")
                    else:
                        print(f"[{timestamp}] ⚠️  WROOM not connected, message dropped")
                
                elif message.startswith("WROOM:"):
                    # Registration from WROOM32 (receiver)
                    wroom_address = addr
                    print(f"[{timestamp}] 🔗 WROOM32 registered: {addr[0]}:{addr[1]}")
                    
                    # Send acknowledgment
                    sock.sendto(b"ACK", addr)
                
                elif message == "PING_C3":
                    # Ping from C3
                    c3_address = addr
                    sock.sendto(b"PONG", addr)
                
                elif message == "PING_WROOM":
                    # Ping from WROOM
                    wroom_address = addr
                    sock.sendto(b"PONG", addr)
                
                else:
                    print(f"[{timestamp}] ❓ Unknown message: '{message}' from {addr[0]}:{addr[1]}")
            
            except UnicodeDecodeError:
                print(f"[{timestamp}] ⚠️  Received non-UTF8 data from {addr[0]}:{addr[1]}")
            
            # Print status every 10 seconds
            if time.time() - last_status_print > 10:
                print_status()
                last_status_print = time.time()
    
    except KeyboardInterrupt:
        print("\n\n⛔ Server shutting down...")
        print_status()
        sock.close()
        print("✅ Server closed successfully")
    
    except Exception as e:
        print(f"\n❌ Error: {e}")
        sock.close()

if __name__ == "__main__":
    main()