import asyncio
import websockets
import socket
import threading
import json
from datetime import datetime

class RobotControlServer:
    def __init__(self):
        self.running = True
        self.client_connected = False
        self.last_command = 'S'
        self.command_count = 0
        
    def start_udp_discovery(self, udp_port=8888, ws_port=8765):
        """UDP discovery service - responds to ESP32 discovery packets"""
        udp_socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        udp_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        udp_socket.bind(('0.0.0.0', udp_port))
        udp_socket.settimeout(1.0)
        
        print(f"🔍 UDP Discovery service running on port {udp_port}")
        
        while self.running:
            try:
                data, addr = udp_socket.recvfrom(1024)
                message = data.decode('utf-8').strip()
                
                if message == "DISCOVER_SERVER":
                    print(f"📡 Discovery request from {addr[0]}:{addr[1]}")
                    
                    # Send server info back
                    response = json.dumps({
                        "type": "SERVER_INFO",
                        "ws_port": ws_port,
                        "ip": self.get_local_ip()
                    })
                    
                    udp_socket.sendto(response.encode('utf-8'), addr)
                    print(f"✅ Sent server info to {addr[0]}")
                    
            except socket.timeout:
                continue
            except Exception as e:
                if self.running:
                    print(f"❌ UDP Discovery error: {e}")
        
        udp_socket.close()
        print("🔍 UDP Discovery service stopped")
    
    def get_local_ip(self):
        """Get local IP address"""
        try:
            s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            s.connect(("8.8.8.8", 80))
            ip = s.getsockname()[0]
            s.close()
            return ip
        except:
            return "192.168.137.1"
    
    async def handle_client(self, websocket):
        """Handle WebSocket connection from ESP32"""
        client_ip = websocket.remote_address[0] if websocket.remote_address else "unknown"
        print(f"\n🤖 ESP32 connected from {client_ip}")
        print(f"⏰ Connected at: {datetime.now().strftime('%H:%M:%S')}")
        self.client_connected = True
        
        # Send initial STOP command
        try:
            await websocket.send('S')
            print("📤 Sent initial STOP command")
        except:
            pass
        
        try:
            # Keep connection alive and handle any responses from ESP32
            async for message in websocket:
                print(f"📥 Received from ESP32: {message}")
                
        except websockets.exceptions.ConnectionClosed:
            print(f"\n🔌 ESP32 {client_ip} disconnected")
            self.client_connected = False
        except Exception as e:
            print(f"❌ Error with ESP32 {client_ip}: {e}")
            self.client_connected = False
    
    async def send_command(self, websocket, command):
        """Send command to ESP32"""
        try:
            await websocket.send(command.upper())
            self.command_count += 1
            self.last_command = command.upper()
            timestamp = datetime.now().strftime('%H:%M:%S.%f')[:-3]
            print(f"📤 [{timestamp}] Sent: '{command.upper()}' (#{self.command_count})")
            return True
        except Exception as e:
            print(f"❌ Failed to send command: {e}")
            return False
    
    async def command_input_handler(self, connected_clients):
        """Handle terminal input and send commands to ESP32"""
        print("\n" + "="*60)
        print("🎮 ROBOT CONTROL INTERFACE")
        print("="*60)
        print("Commands:")
        print("  W - Forward")
        print("  S - Stop")
        print("  A - Turn Left")
        print("  D - Turn Right")
        print("  X - Backward")
        print("  Q - Quit server")
        print("="*60)
        print("💡 Type command and press Enter\n")
        
        loop = asyncio.get_event_loop()
        
        while self.running:
            try:
                # Read input asynchronously
                command = await loop.run_in_executor(None, input, "Enter command: ")
                command = command.strip().upper()
                
                if command == 'Q':
                    print("\n🛑 Shutting down server...")
                    self.running = False
                    break
                
                if command in ['W', 'S', 'A', 'D', 'X']:
                    if connected_clients:
                        # Send to all connected clients
                        for client in connected_clients:
                            await self.send_command(client, command)
                    else:
                        print("⚠️  No ESP32 connected!")
                else:
                    print(f"❌ Invalid command: '{command}'. Use W/A/S/D/X/Q")
                    
            except EOFError:
                break
            except Exception as e:
                print(f"❌ Input error: {e}")
    
    async def start_server(self, ws_port=8765, udp_port=8888):
        """Start WebSocket server and UDP discovery"""
        server_ip = "0.0.0.0"
        local_ip = self.get_local_ip()
        
        print("\n" + "="*60)
        print("🚀 ESP32 ROBOT CONTROL SERVER")
        print("="*60)
        print(f"🌐 Local IP: {local_ip}")
        print(f"🔌 WebSocket Port: {ws_port}")
        print(f"🔍 UDP Discovery Port: {udp_port}")
        print(f"📡 ESP32 will auto-discover this server")
        print("="*60 + "\n")
        
        # Start UDP discovery in separate thread
        udp_thread = threading.Thread(
            target=self.start_udp_discovery,
            args=(udp_port, ws_port),
            daemon=True
        )
        udp_thread.start()
        
        connected_clients = set()
        
        async def handle_client_wrapper(websocket):
            connected_clients.add(websocket)
            try:
                await self.handle_client(websocket)
            finally:
                connected_clients.remove(websocket)
        
        try:
            async with websockets.serve(
                handle_client_wrapper,
                server_ip,
                ws_port,
                max_size=1024 * 10,
                compression=None,
                ping_interval=20,
                ping_timeout=10
            ):
                print(f"✅ WebSocket server running on port {ws_port}")
                print("⏳ Waiting for ESP32 connection...\n")
                
                # Start command input handler
                await self.command_input_handler(connected_clients)
                
        except Exception as e:
            print(f"❌ Server error: {e}")
        finally:
            self.running = False
            print("\n🔚 Server shutdown complete")

def main():
    """Main function"""
    server = RobotControlServer()
    
    try:
        asyncio.run(server.start_server())
    except KeyboardInterrupt:
        print("\n\n🛑 Server stopped by user")
    except Exception as e:
        print(f"❌ Error: {e}")
    finally:
        print("👋 Goodbye!")

if __name__ == "__main__":
    main()