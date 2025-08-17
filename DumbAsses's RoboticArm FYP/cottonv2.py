import cv2
import numpy as np
import asyncio
import websockets
import json
import threading
import time
from datetime import datetime
import io
from PIL import Image

# WebSocket server configuration
WEBSOCKET_HOST = "0.0.0.0"  # Listen on all interfaces
WEBSOCKET_PORT_COMMANDS = 8765  # Port for ESP32 robotic arm commands
WEBSOCKET_PORT_CAMERA = 8766    # Port for ESP32-CAM stream

class CottonDetector:
    def __init__(self):
        self.current_frame = None
        self.frame_lock = threading.Lock()
        self.connected_arm_clients = set()
        self.connected_camera_clients = set()
        self.latest_coordinates = []
        self.websocket_loop = None
        self.server_running = False
        self.camera_server_running = False
        self.last_frame_time = 0
        
    def detect_cotton(self, frame):
        """Detect cotton in the frame and return coordinates"""
        try:
            # Resize for performance
            frame = cv2.resize(frame, (320, 240))
            
            # Convert to HSV for robust white detection
            hsv = cv2.cvtColor(frame, cv2.COLOR_BGR2HSV)
            
            # Define range for white color in HSV
            lower_white = np.array([0, 0, 200])
            upper_white = np.array([180, 40, 255])
            mask = cv2.inRange(hsv, lower_white, upper_white)
            
            # Morphological operations to remove noise
            kernel = np.ones((5,5), np.uint8)
            mask = cv2.morphologyEx(mask, cv2.MORPH_OPEN, kernel)
            mask = cv2.morphologyEx(mask, cv2.MORPH_DILATE, kernel)
            
            # Find contours (cotton blobs)
            contours, _ = cv2.findContours(mask, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)
            cotton_coords = []
            
            # Draw bounding boxes and centroids on the raw frame
            for cnt in contours:
                area = cv2.contourArea(cnt)
                if area > 100:  # Minimum area threshold to filter noise
                    x, y, w, h = cv2.boundingRect(cnt)
                    cx = x + w // 2
                    cy = y + h // 2
                    cotton_coords.append({"x": cx, "y": cy, "area": int(area)})
                    
                    # Draw bounding box
                    cv2.rectangle(frame, (x, y), (x + w, y + h), (0, 255, 0), 2)
                    # Draw centroid
                    cv2.circle(frame, (cx, cy), 4, (0, 0, 255), -1)
                    # Put coordinates text
                    cv2.putText(frame, f"({cx},{cy})", (x, y - 10), 
                               cv2.FONT_HERSHEY_SIMPLEX, 0.5, (255, 0, 0), 1)
            
            return cotton_coords, frame, mask
            
        except Exception as e:
            print(f"✗ Error in cotton detection: {e}")
            return [], frame, None
    
    async def handle_arm_client(self, websocket):
        """Handle WebSocket connections from robotic arm ESP32"""
        print(f"✓ Robotic arm connected: {websocket.remote_address}")
        self.connected_arm_clients.add(websocket)
        
        try:
            async for message in websocket:
                print(f"📥 Received from arm ESP32: {message}")
        except websockets.exceptions.ConnectionClosed:
            pass
        except Exception as e:
            print(f"✗ Arm client handling error: {e}")
        finally:
            self.connected_arm_clients.discard(websocket)
            print(f"✗ Robotic arm disconnected: {websocket.remote_address}")
    
    async def handle_camera_client(self, websocket):
        """Handle WebSocket connections from camera ESP32"""
        print(f"✓ ESP32-CAM connected: {websocket.remote_address}")
        self.connected_camera_clients.add(websocket)
        
        try:
            async for message in websocket:
                if isinstance(message, bytes):
                    # Binary data - camera frame
                    await self.process_camera_frame(message)
                else:
                    # Text data - control messages
                    try:
                        data = json.loads(message)
                        if data.get("type") == "camera_connected":
                            print(f"📷 Camera info: {data}")
                    except json.JSONDecodeError:
                        print(f"📥 Camera message: {message}")
                        
        except websockets.exceptions.ConnectionClosed:
            pass
        except Exception as e:
            print(f"✗ Camera client handling error: {e}")
        finally:
            self.connected_camera_clients.discard(websocket)
            print(f"✗ ESP32-CAM disconnected: {websocket.remote_address}")
    
    async def process_camera_frame(self, frame_data):
        """Process received camera frame from ESP32-CAM"""
        try:
            # Convert bytes to numpy array
            nparr = np.frombuffer(frame_data, np.uint8)
            
            # Decode JPEG
            frame = cv2.imdecode(nparr, cv2.IMREAD_COLOR)
            
            if frame is not None:
                # Update current frame thread-safely
                with self.frame_lock:
                    self.current_frame = frame.copy()
                    self.last_frame_time = time.time()
                
                # Process for cotton detection
                cotton_coords, processed_frame, mask = self.detect_cotton(frame)
                
                # Update latest coordinates
                self.latest_coordinates = cotton_coords
                
                # Send coordinates to robotic arm
                if cotton_coords:
                    await self.broadcast_coordinates(cotton_coords)
                    coord_str = ", ".join([f"({c['x']},{c['y']})" for c in cotton_coords])
                    print(f"🎯 Cotton coordinates: [{coord_str}]")
                
                # Update display frame
                with self.frame_lock:
                    self.current_frame = processed_frame
            
        except Exception as e:
            print(f"✗ Error processing camera frame: {e}")
    
    async def broadcast_coordinates(self, coordinates):
        """Send coordinates to all connected robotic arm clients"""
        if self.connected_arm_clients and coordinates:
            message = {
                "type": "cotton_coordinates",
                "timestamp": datetime.now().isoformat(),
                "coordinates": coordinates,
                "count": len(coordinates)
            }
            
            # Send to all connected arm clients
            disconnected_clients = set()
            for client in self.connected_arm_clients:
                try:
                    await client.send(json.dumps(message))
                except websockets.exceptions.ConnectionClosed:
                    disconnected_clients.add(client)
                except Exception as e:
                    print(f"✗ Error sending to arm client: {e}")
                    disconnected_clients.add(client)
            
            # Remove disconnected clients
            self.connected_arm_clients -= disconnected_clients
    
    def display_loop(self):
        """Display camera feed in separate thread"""
        print("📺 Starting display loop...")
        
        while True:
            try:
                with self.frame_lock:
                    if self.current_frame is not None:
                        display_frame = self.current_frame.copy()
                    else:
                        display_frame = None
                
                if display_frame is not None:
                    cv2.imshow('Cotton Detection - WebSocket Stream', display_frame)
                else:
                    # Show waiting message
                    waiting_frame = np.zeros((240, 320, 3), dtype=np.uint8)
                    cv2.putText(waiting_frame, "Waiting for camera...", (50, 120), 
                               cv2.FONT_HERSHEY_SIMPLEX, 0.7, (255, 255, 255), 2)
                    cv2.imshow('Cotton Detection - WebSocket Stream', waiting_frame)
                
                # Check for frame timeout
                if time.time() - self.last_frame_time > 5 and self.last_frame_time > 0:
                    print("⚠️ No camera frames received for 5 seconds")
                    with self.frame_lock:
                        self.current_frame = None
                
                # Exit on 'q' key
                if cv2.waitKey(30) & 0xFF == ord('q'):
                    print("🛑 Stopping system...")
                    break
                    
            except KeyboardInterrupt:
                print("🛑 Interrupted by user")
                break
            except Exception as e:
                print(f"✗ Error in display loop: {e}")
                time.sleep(1)
        
        cv2.destroyAllWindows()
    
    def start_websocket_servers(self):
        """Start WebSocket servers for both arm and camera"""
        print(f"🚀 Starting WebSocket servers:")
        print(f"   📡 Arm commands on {WEBSOCKET_HOST}:{WEBSOCKET_PORT_COMMANDS}")
        print(f"   📷 Camera stream on {WEBSOCKET_HOST}:{WEBSOCKET_PORT_CAMERA}")
        
        # Create new event loop for this thread
        self.websocket_loop = asyncio.new_event_loop()
        asyncio.set_event_loop(self.websocket_loop)
        
        try:
            async def server_main():
                # Start both servers
                arm_server = await websockets.serve(
                    self.handle_arm_client, 
                    WEBSOCKET_HOST, 
                    WEBSOCKET_PORT_COMMANDS
                )
                
                camera_server = await websockets.serve(
                    self.handle_camera_client,
                    WEBSOCKET_HOST,
                    WEBSOCKET_PORT_CAMERA
                )
                
                self.server_running = True
                self.camera_server_running = True
                print("✓ Both WebSocket servers started!")
                print("✓ Waiting for ESP32 connections...")
                
                # Run forever
                await asyncio.Future()
            
            self.websocket_loop.run_until_complete(server_main())
            
        except Exception as e:
            print(f"✗ WebSocket server error: {e}")
            self.server_running = False
            self.camera_server_running = False
    
    def run(self):
        """Main entry point - start servers and display"""
        print("🎯 Starting Cotton Detection System with WebSocket Camera...")
        print("=" * 60)
        
        # Start WebSocket servers in separate thread
        print("🌐 Initializing WebSocket servers...")
        websocket_thread = threading.Thread(target=self.start_websocket_servers)
        websocket_thread.daemon = True
        websocket_thread.start()
        
        # Give servers time to start
        time.sleep(3)
        
        if not self.server_running:
            print("⚠️ WebSocket servers starting (may take a moment)...")
            time.sleep(2)
        
        # Start display in separate thread
        print("📺 Starting display system...")
        display_thread = threading.Thread(target=self.display_loop)
        display_thread.daemon = True
        display_thread.start()
        
        # Keep main thread alive
        try:
            while True:
                time.sleep(1)
                # Show status periodically
                if time.time() % 30 < 1:  # Every 30 seconds
                    print(f"📊 Status - Arm clients: {len(self.connected_arm_clients)}, "
                          f"Camera clients: {len(self.connected_camera_clients)}")
        except KeyboardInterrupt:
            print("\n🛑 Shutting down...")

if __name__ == "__main__":
    try:
        detector = CottonDetector()
        detector.run()
    except KeyboardInterrupt:
        print("\n🛑 System stopped by user")
    except Exception as e:
        print(f"✗ Fatal error: {e}")
        import traceback
        traceback.print_exc()