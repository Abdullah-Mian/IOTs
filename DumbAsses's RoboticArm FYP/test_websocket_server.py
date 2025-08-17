import asyncio
import websockets
import json
import time
from datetime import datetime
import random

# WebSocket server configuration
WEBSOCKET_HOST = "0.0.0.0"  # Listen on all interfaces
WEBSOCKET_PORT = 8765

class TestWebSocketServer:
    def __init__(self):
        self.connected_clients = set()
        self.running = False
        
    async def handle_client(self, websocket):
        """Handle WebSocket client connections"""
        print(f"✓ ESP32 connected: {websocket.remote_address}")
        self.connected_clients.add(websocket)
        
        try:
            # Send welcome message
            welcome_msg = {
                "type": "welcome",
                "message": "Connected to test WebSocket server",
                "timestamp": datetime.now().isoformat()
            }
            await websocket.send(json.dumps(welcome_msg))
            print(f"📨 Sent welcome message to {websocket.remote_address}")
            
            # Keep connection alive and wait for disconnection
            async for message in websocket:
                print(f"📥 Received from ESP32: {message}")
                
        except websockets.exceptions.ConnectionClosed:
            print(f"✗ ESP32 disconnected normally: {websocket.remote_address}")
        except Exception as e:
            print(f"✗ Client handling error: {e}")
        finally:
            self.connected_clients.discard(websocket)
            print(f"✗ ESP32 removed from client list: {websocket.remote_address}")
    
    async def send_dummy_coordinates(self):
        """Send dummy cotton coordinates to all connected clients"""
        while self.running:
            if self.connected_clients:
                # Generate random cotton coordinates
                dummy_coordinates = []
                num_cotton = random.randint(1, 3)  # 1-3 cotton objects
                
                for i in range(num_cotton):
                    cotton = {
                        "x": random.randint(50, 270),    # Random X between 50-270
                        "y": random.randint(50, 190),    # Random Y between 50-190
                        "area": random.randint(150, 500) # Random area
                    }
                    dummy_coordinates.append(cotton)
                
                # Create message
                message = {
                    "type": "cotton_coordinates",
                    "timestamp": datetime.now().isoformat(),
                    "coordinates": dummy_coordinates,
                    "count": len(dummy_coordinates)
                }
                
                # Send to all connected clients
                disconnected_clients = set()
                for client in self.connected_clients.copy():
                    try:
                        await client.send(json.dumps(message))
                        coord_str = ", ".join([f"({c['x']},{c['y']})" for c in dummy_coordinates])
                        print(f"📨 Sent coordinates to {client.remote_address}: [{coord_str}]")
                    except websockets.exceptions.ConnectionClosed:
                        disconnected_clients.add(client)
                        print(f"✗ Client disconnected during send: {client.remote_address}")
                    except Exception as e:
                        print(f"✗ Error sending to client {client.remote_address}: {e}")
                        disconnected_clients.add(client)
                
                # Remove disconnected clients
                self.connected_clients -= disconnected_clients
                
                # Wait before sending next coordinates
                await asyncio.sleep(5)  # Send every 5 seconds
            else:
                print("⏳ No clients connected. Waiting...")
                await asyncio.sleep(2)
    
    async def start_server(self):
        """Start the WebSocket server"""
        print(f"🚀 Starting Test WebSocket server on {WEBSOCKET_HOST}:{WEBSOCKET_PORT}")
        print("=" * 60)
        
        try:
            # Start WebSocket server
            async with websockets.serve(
                self.handle_client,
                WEBSOCKET_HOST,
                WEBSOCKET_PORT
            ):
                self.running = True
                print("✓ WebSocket server started!")
                print("✓ Waiting for ESP32 connections...")
                print("📡 Server listening on all network interfaces")
                print(f"🔗 ESP32 should connect to: ws://192.168.137.1:{WEBSOCKET_PORT}")
                print("=" * 60)
                
                # Start sending dummy data in background
                await self.send_dummy_coordinates()
            
        except Exception as e:
            print(f"✗ Server error: {e}")
            self.running = False
    
    def run(self):
        """Main entry point"""
        print("🧪 Test WebSocket Server for ESP32")
        print("This server will send dummy cotton coordinates to test ESP32 connection")
        print()
        
        try:
            asyncio.run(self.start_server())
        except KeyboardInterrupt:
            print("\n🛑 Server stopped by user")
            self.running = False
        except Exception as e:
            print(f"✗ Fatal error: {e}")

def show_network_info():
    """Display network connection information"""
    print("📶 Network Information:")
    print("- Make sure your ESP32 is connected to your laptop's hotspot")
    print("- Your laptop should have IP: 192.168.137.1 (hotspot gateway)")
    print("- ESP32 should have IP: 192.168.137.xxx")
    print("- ESP32 should connect to: ws://192.168.137.1:8765")
    print()

if __name__ == "__main__":
    show_network_info()
    
    server = TestWebSocketServer()
    server.run()
    
    server = TestWebSocketServer()
    server.run()
