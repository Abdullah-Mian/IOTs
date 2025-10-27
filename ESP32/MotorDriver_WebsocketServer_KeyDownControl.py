import asyncio
import websockets
import socket
import threading
import json
from datetime import datetime
from pynput import keyboard

class RobotControlServer:
    def __init__(self):
        self.running = True
        self.client_connected = False
        self.last_command = 'S'
        self.command_count = 0
        self.connected_clients = set()
        self.pressed_keys = set()  # Track currently pressed keys
        
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
    
    async def send_command(self, command):
        """Send command to all connected ESP32 clients"""
        command = command.upper()
        
        if not self.connected_clients:
            print(f"⚠️  No ESP32 connected! Command '{command}' not sent.")
            return False
            
        if command == self.last_command:
            return True  # Skip duplicate commands
            
        try:
            # Send to all connected clients
            for websocket in self.connected_clients:
                await websocket.send(command)
            
            self.command_count += 1
            self.last_command = command
            timestamp = datetime.now().strftime('%H:%M:%S.%f')[:-3]
            
            # Print what command means
            command_names = {
                'W': 'FORWARD',
                'A': 'LEFT',
                'S': 'STOP',
                'D': 'RIGHT',
                'X': 'BACKWARD'
            }
            command_name = command_names.get(command, command)
            print(f"📤 [{timestamp}] Sent: '{command}' ({command_name}) [#{self.command_count}]")
            return True
        except Exception as e:
            print(f"❌ Failed to send command: {e}")
            return False
    
    def on_press(self, key):
        """Handle key press events"""
        try:
            # Get the character key
            if hasattr(key, 'char') and key.char:
                char = key.char.upper()
                
                # Only process W, A, S, D, X keys
                if char in ['W', 'A', 'S', 'D', 'X']:
                    if char not in self.pressed_keys:
                        self.pressed_keys.add(char)
                        # Schedule command sending in the asyncio loop
                        asyncio.run_coroutine_threadsafe(
                            self.send_command(char),
                            self.loop
                        )
        except AttributeError:
            # Special keys (like Esc, Ctrl, etc.)
            if key == keyboard.Key.esc:
                print("\n🛑 ESC pressed - Shutting down server...")
                self.running = False
                return False  # Stop listener
    
    def on_release(self, key):
        """Handle key release events"""
        try:
            if hasattr(key, 'char') and key.char:
                char = key.char.upper()
                
                # When any movement key is released, send STOP
                if char in ['W', 'A', 'S', 'D', 'X']:
                    if char in self.pressed_keys:
                        self.pressed_keys.remove(char)
                        # Send stop command
                        asyncio.run_coroutine_threadsafe(
                            self.send_command('S'),
                            self.loop
                        )
        except AttributeError:
            pass
    
    def start_keyboard_listener(self):
        """Start keyboard listener in a separate thread"""
        listener = keyboard.Listener(
            on_press=self.on_press,
            on_release=self.on_release
        )
        listener.start()
        print("⌨️  Keyboard listener started")
        return listener
    
    async def keep_alive(self):
        """Keep the server running and monitor status"""
        while self.running:
            await asyncio.sleep(1)
    
    async def start_server(self, ws_port=8765, udp_port=8888):
        """Start WebSocket server and UDP discovery"""
        server_ip = "0.0.0.0"
        local_ip = self.get_local_ip()
        
        print("\n" + "="*60)
        print("🚀 ESP32 ROBOT CONTROL SERVER (KEYBOARD CONTROL)")
        print("="*60)
        print(f"🌐 Local IP: {local_ip}")
        print(f"🔌 WebSocket Port: {ws_port}")
        print(f"🔍 UDP Discovery Port: {udp_port}")
        print(f"📡 ESP32 will auto-discover this server")
        print("="*60)
        print("\n⌨️  KEYBOARD CONTROLS:")
        print("  W - Forward")
        print("  A - Turn Left")
        print("  S - Stop (or release any key)")
        print("  D - Turn Right")
        print("  X - Backward")
        print("  ESC - Quit server")
        print("="*60 + "\n")
        
        # Store event loop for keyboard callbacks
        self.loop = asyncio.get_event_loop()
        
        # Start UDP discovery in separate thread
        udp_thread = threading.Thread(
            target=self.start_udp_discovery,
            args=(udp_port, ws_port),
            daemon=True
        )
        udp_thread.start()
        
        # Start keyboard listener
        kb_listener = self.start_keyboard_listener()
        
        async def handle_client_wrapper(websocket):
            self.connected_clients.add(websocket)
            try:
                await self.handle_client(websocket)
            finally:
                self.connected_clients.discard(websocket)
        
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
                print("⏳ Waiting for ESP32 connection...")
                print("💡 Press W/A/S/D/X to control robot, ESC to quit\n")
                
                # Keep server running
                await self.keep_alive()
                
        except Exception as e:
            print(f"❌ Server error: {e}")
        finally:
            self.running = False
            kb_listener.stop()
            print("\n🔚 Server shutdown complete")

def main():
    """Main function"""
    server = RobotControlServer()
    
    try:
        asyncio.run(server.start_server())
    except KeyboardInterrupt:
        print("\n\n🛑 Server stopped by user (Ctrl+C)")
    except Exception as e:
        print(f"❌ Error: {e}")
    finally:
        print("👋 Goodbye!")

if __name__ == "__main__":
    main()