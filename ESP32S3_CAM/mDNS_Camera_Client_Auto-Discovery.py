#!/usr/bin/env python3
"""
ESP32-S3 CAM Auto-Discovery MJPEG Client
Automatically discovers ESP32-S3 cameras on the network using mDNS
Displays high-quality video stream with performance metrics

Installation:
pip install opencv-python numpy zeroconf requests

Usage:
python mdns_camera_client.py
"""

import cv2
import numpy as np
import requests
from zeroconf import ServiceBrowser, Zeroconf
import socket
import threading
import time
from datetime import datetime
import json

class ESP32CameraDiscovery:
    def __init__(self):
        self.cameras = {}  # hostname -> {ip, port, info}
        self.discovery_complete = False
        
    def on_service_state_change(self, zeroconf, service_type, name, state_change):
        """Callback when mDNS services are discovered"""
        if state_change.name == 'Added':
            info = zeroconf.get_service_info(service_type, name)
            if info:
                # Extract camera information
                hostname = name.split('.')[0]
                ip_address = socket.inet_ntoa(info.addresses[0])
                port = info.properties.get(b'port', 80)
                
                camera_info = {
                    'ip': ip_address,
                    'port': port,
                    'model': info.properties.get(b'model', b'Unknown').decode('utf-8'),
                    'resolution': info.properties.get(b'resolution', b'Unknown').decode('utf-8'),
                    'stream': info.properties.get(b'stream', b'/stream').decode('utf-8')
                }
                
                self.cameras[hostname] = camera_info
                
                print(f"\n{'='*60}")
                print(f"📷 ESP32 Camera Discovered!")
                print(f"{'='*60}")
                print(f"Hostname: {hostname}")
                print(f"IP Address: {ip_address}")
                print(f"Port: {port}")
                print(f"Model: {camera_info['model']}")
                print(f"Resolution: {camera_info['resolution']}")
                print(f"Stream Path: {camera_info['stream']}")
                print(f"{'='*60}\n")

class HighQualityMJPEGClient:
    def __init__(self, camera_url):
        self.camera_url = camera_url
        self.running = True
        self.frame_count = 0
        self.fps_counter = 0
        self.fps_start_time = time.time()
        self.current_fps = 0.0
        self.latency_ms = 0
        
    def get_camera_status(self, base_url):
        """Fetch camera status information"""
        try:
            response = requests.get(f"{base_url}/status", timeout=2)
            if response.status_code == 200:
                return response.json()
        except:
            pass
        return None
    
    def calculate_fps(self):
        """Calculate FPS"""
        self.fps_counter += 1
        current_time = time.time()
        
        if current_time - self.fps_start_time >= 1.0:
            self.current_fps = self.fps_counter / (current_time - self.fps_start_time)
            self.fps_start_time = current_time
            self.fps_counter = 0
    
    def stream(self):
        """Main streaming function"""
        print(f"Connecting to camera stream: {self.camera_url}")
        print("Press 'q' to quit, 's' to save snapshot, 'i' for info")
        
        # Get base URL for status checks
        base_url = self.camera_url.rsplit('/', 1)[0]
        
        # Performance monitoring thread
        def monitor_performance():
            while self.running:
                time.sleep(5)
                status = self.get_camera_status(base_url)
                if status:
                    print(f"\n📊 Camera Status:")
                    print(f"    Server FPS: {status.get('fps', 'N/A')}")
                    print(f"    Free Heap: {status.get('heap', 'N/A')} bytes")
                    print(f"    Free PSRAM: {status.get('psram', 'N/A')} bytes")
                    print(f"    Quality: {status.get('quality', 'N/A')}")
        
        monitor_thread = threading.Thread(target=monitor_performance, daemon=True)
        monitor_thread.start()
        
        # Connect to MJPEG stream
        try:
            stream = requests.get(self.camera_url, stream=True, timeout=5)
            
            if stream.status_code != 200:
                print(f"Failed to connect: HTTP {stream.status_code}")
                return
            
            print("✅ Connected to camera stream!")
            
            bytes_buffer = bytes()
            
            for chunk in stream.iter_content(chunk_size=4096):
                if not self.running:
                    break
                
                bytes_buffer += chunk
                
                # Look for JPEG boundaries
                a = bytes_buffer.find(b'\xff\xd8')  # JPEG start
                b = bytes_buffer.find(b'\xff\xd9')  # JPEG end
                
                if a != -1 and b != -1:
                    jpg = bytes_buffer[a:b+2]
                    bytes_buffer = bytes_buffer[b+2:]
                    
                    # Measure latency
                    frame_start = time.time()
                    
                    # Decode JPEG
                    nparr = np.frombuffer(jpg, dtype=np.uint8)
                    frame = cv2.imdecode(nparr, cv2.IMREAD_COLOR)
                    
                    if frame is not None:
                        self.frame_count += 1
                        self.calculate_fps()
                        
                        # Calculate latency
                        self.latency_ms = (time.time() - frame_start) * 1000
                        
                        # Add performance overlay
                        height, width = frame.shape[:2]
                        
                        # Create semi-transparent overlay
                        overlay = frame.copy()
                        cv2.rectangle(overlay, (10, 10), (400, 120), (0, 0, 0), -1)
                        cv2.addWeighted(overlay, 0.7, frame, 0.3, 0, frame)
                        
                        # Add text information
                        info_lines = [
                            f"Frame: {self.frame_count}",
                            f"Client FPS: {self.current_fps:.1f}",
                            f"Latency: {self.latency_ms:.1f} ms",
                            f"Resolution: {width}x{height}",
                            f"Time: {datetime.now().strftime('%H:%M:%S')}"
                        ]
                        
                        y_pos = 30
                        for line in info_lines:
                            cv2.putText(frame, line, (15, y_pos), 
                                      cv2.FONT_HERSHEY_SIMPLEX, 0.5, (0, 255, 0), 1)
                            y_pos += 20
                        
                        # Display frame
                        cv2.imshow('ESP32-S3 CAM High-Quality Stream', frame)
                        
                        # Handle keyboard input
                        key = cv2.waitKey(1) & 0xFF
                        
                        if key == ord('q'):
                            print("Quit key pressed")
                            self.running = False
                            break
                        elif key == ord('s'):
                            # Save snapshot
                            filename = f"snapshot_{int(time.time())}.jpg"
                            cv2.imwrite(filename, frame)
                            print(f"💾 Snapshot saved: {filename}")
                        elif key == ord('i'):
                            # Print detailed info
                            print(f"\n📊 Detailed Information:")
                            print(f"    Total Frames: {self.frame_count}")
                            print(f"    Current FPS: {self.current_fps:.1f}")
                            print(f"    Average Latency: {self.latency_ms:.1f} ms")
                            print(f"    Resolution: {width}x{height}")
                            print(f"    Stream URL: {self.camera_url}")
                
        except requests.exceptions.RequestException as e:
            print(f"❌ Connection error: {e}")
        except KeyboardInterrupt:
            print("\n👋 Interrupted by user")
        finally:
            self.running = False
            cv2.destroyAllWindows()
            print("Stream closed")

def discover_cameras(timeout=10):
    """Discover ESP32 cameras on the network"""
    print("=" * 60)
    print("🔍 Discovering ESP32 Cameras on Network...")
    print("=" * 60)
    print(f"Searching for {timeout} seconds...\n")
    
    zeroconf = Zeroconf()
    discovery = ESP32CameraDiscovery()
    
    # Browse for HTTP and camera services
    browsers = [
        ServiceBrowser(zeroconf, "_http._tcp.local.", 
                      handlers=[discovery.on_service_state_change]),
        ServiceBrowser(zeroconf, "_camera._tcp.local.", 
                      handlers=[discovery.on_service_state_change])
    ]
    
    try:
        time.sleep(timeout)
    finally:
        zeroconf.close()
    
    return discovery.cameras

def main():
    """Main function"""
    # Discover cameras
    cameras = discover_cameras(timeout=10)
    
    if not cameras:
        print("\n❌ No ESP32 cameras found on the network")
        print("\nTroubleshooting:")
        print("1. Ensure ESP32 camera is powered on and connected to WiFi")
        print("2. Check that mDNS is enabled on the ESP32")
        print("3. Verify both devices are on the same network")
        print("4. Try increasing discovery timeout")
        return
    
    print(f"\n✅ Found {len(cameras)} camera(s)")
    
    # Let user select camera
    if len(cameras) == 1:
        hostname = list(cameras.keys())[0]
        selected_camera = cameras[hostname]
        print(f"\nAuto-selecting: {hostname}")
    else:
        print("\nAvailable cameras:")
        camera_list = list(cameras.items())
        for idx, (hostname, info) in enumerate(camera_list, 1):
            print(f"{idx}. {hostname} ({info['ip']})")
        
        while True:
            try:
                choice = int(input("\nSelect camera number: ")) - 1
                if 0 <= choice < len(camera_list):
                    hostname, selected_camera = camera_list[choice]
                    break
                else:
                    print("Invalid selection")
            except ValueError:
                print("Please enter a number")
    
    # Build stream URL
    stream_url = f"http://{selected_camera['ip']}:{selected_camera['port']}{selected_camera['stream']}"
    
    # Start streaming
    client = HighQualityMJPEGClient(stream_url)
    client.stream()

if __name__ == "__main__":
    main()