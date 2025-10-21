import asyncio
import websockets
import cv2
import numpy as np
from datetime import datetime
import threading
import queue
import time

class HighPerformanceESP32Server:
    def __init__(self):
        self.frame_count = 0
        self.latest_frame = None
        self.frame_queue = queue.Queue(maxsize=2)  # Reduced buffer for lower latency
        self.running = True
        self.client_connected = False
        
        # FPS calculation variables
        self.fps_start_time = time.time()
        self.fps_frame_count = 0
        self.current_fps = 0
        self.fps_update_interval = 1.0  # Update FPS every second
        
    async def handle_camera(self, websocket):
        """Handle incoming ESP32-CAM WebSocket connection with optimizations"""
        client_ip = websocket.remote_address[0] if websocket.remote_address else "unknown"
        print(f"📷 ESP32-CAM connected from {client_ip}")
        self.client_connected = True
        
        try:
            async for message in websocket:
                if isinstance(message, bytes):
                    # Drop old frames immediately to reduce latency
                    while not self.frame_queue.empty():
                        try:
                            self.frame_queue.get_nowait()
                        except queue.Empty:
                            break
                    
                    # Add new frame
                    try:
                        self.frame_queue.put_nowait(message)
                    except queue.Full:
                        pass  # Skip if queue is full
                        
        except websockets.exceptions.ConnectionClosed:
            print(f"📷 ESP32-CAM {client_ip} disconnected")
            self.client_connected = False
        except Exception as e:
            print(f"❌ Error with ESP32-CAM {client_ip}: {e}")
            self.client_connected = False

    def calculate_fps(self):
        """Calculate and update FPS"""
        current_time = time.time()
        self.fps_frame_count += 1
        
        if current_time - self.fps_start_time >= self.fps_update_interval:
            elapsed_time = current_time - self.fps_start_time
            self.current_fps = self.fps_frame_count / elapsed_time
            
            # Reset counters
            self.fps_start_time = current_time
            self.fps_frame_count = 0

    def display_frames(self):
        """High-performance frame display with minimal processing"""
        print("🖥️ High-performance display thread started...")
        
        # OpenCV optimizations
        cv2.setNumThreads(4)  # Use multiple threads
        
        while self.running:
            try:
                if not self.client_connected:
                    # Minimal waiting screen
                    waiting_frame = np.zeros((240, 320, 3), dtype=np.uint8)
                    cv2.putText(waiting_frame, "Waiting for ESP32-CAM...", (50, 120), 
                              cv2.FONT_HERSHEY_SIMPLEX, 0.6, (0, 255, 255), 2)
                    
                    cv2.imshow('ESP32-CAM Stream [High Performance]', waiting_frame)
                    
                    key = cv2.waitKey(100) & 0xFF  # Shorter wait
                    if key == ord('q'):
                        print("👋 Quit key pressed")
                        self.running = False
                        break
                    continue
                
                # Get frame with minimal timeout
                try:
                    frame_data = self.frame_queue.get(timeout=0.1)
                except queue.Empty:
                    continue
                
                # Fast JPEG decode
                nparr = np.frombuffer(frame_data, np.uint8)
                frame = cv2.imdecode(nparr, cv2.IMREAD_COLOR)
                
                if frame is not None:
                    self.frame_count += 1
                    self.calculate_fps()
                    
                    # Minimal overlay for performance
                    timestamp = datetime.now().strftime("%H:%M:%S.%f")[:-3]  # Include milliseconds
                    
                    # Performance-optimized text overlay
                    overlay_text = f"Frame: {self.frame_count} | FPS: {self.current_fps:.1f} | {timestamp}"
                    
                    # Single rectangle and text call
                    cv2.rectangle(frame, (5, 5), (500, 30), (0, 0, 0), -1)
                    cv2.putText(frame, overlay_text, (10, 22), 
                              cv2.FONT_HERSHEY_SIMPLEX, 0.4, (0, 255, 0), 1)
                    
                    # Direct display without copy
                    cv2.imshow('ESP32-CAM Stream [High Performance]', frame)
                    
                    # Minimal key check
                    key = cv2.waitKey(1) & 0xFF
                    if key == ord('q'):
                        print("👋 Quit key pressed")
                        self.running = False
                        break
                        
                    # Print FPS periodically
                    if self.frame_count % 30 == 0:
                        print(f"📊 Performance: {self.current_fps:.1f} FPS | Frame {self.frame_count}")
                        
            except Exception as e:
                print(f"❌ Display error: {e}")
                time.sleep(0.01)  # Very short wait
        
        cv2.destroyAllWindows()
        print("🖥️ Display window closed")

    async def keep_server_alive(self):
        """Keep server running with minimal overhead"""
        try:
            while self.running:
                await asyncio.sleep(0.1)  # Shorter sleep for responsiveness
        except KeyboardInterrupt:
            print("\n🛑 Server stopped by user")
            self.running = False

    async def start_server(self):
        """Start optimized WebSocket server"""
        server_ip = "0.0.0.0"
        server_port = 8765
        
        print("=" * 60)
        print("🚀 HIGH-PERFORMANCE ESP32-CAM SERVER")
        print(f"📡 Server: ws://{server_ip}:{server_port}")
        print(f"🔗 ESP32-CAM connect to: ws://192.168.137.1:{server_port}")
        print("=" * 60)
        print("⚡ Optimizations: Low latency, FPS counter, minimal buffering")
        print("⌨️ Press 'q' in camera window to quit")
        
        # Start high-performance display thread
        display_thread = threading.Thread(target=self.display_frames)
        display_thread.daemon = True
        display_thread.start()
        
        try:
            # Optimized WebSocket server configuration
            async with websockets.serve(
                self.handle_camera,
                server_ip,
                server_port,
                max_size=1024 * 1024,      # 1MB max message
                max_queue=2,               # Minimal queue
                compression=None,          # Disable compression for speed
                ping_interval=None,        # Disable ping for performance
                ping_timeout=None          # Disable timeout checks
            ):
                print(f"✅ High-performance server running on port {server_port}")
                print("⏳ Waiting for ESP32-CAM...")
                
                await self.keep_server_alive()
                
        except Exception as e:
            print(f"❌ Server error: {e}")
        finally:
            self.running = False

def main():
    """Main function with error handling"""
    server = HighPerformanceESP32Server()
    
    try:
        asyncio.run(server.start_server())
    except KeyboardInterrupt:
        print("\n🛑 Server stopped")
    except Exception as e:
        print(f"❌ Error: {e}")
    finally:
        cv2.destroyAllWindows()
        print("📚 Cleanup complete")

if __name__ == "__main__":
    main()