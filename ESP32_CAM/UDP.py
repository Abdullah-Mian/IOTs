import socket
import cv2
import numpy as np
import threading
import time
from collections import defaultdict
import struct

class UltraFastUDPReceiver:
    def __init__(self, port=1234):
        self.port = port
        self.running = True
        self.frames_buffer = defaultdict(dict)  # frame_id -> {packet_num: data}
        self.frame_headers = {}  # frame_id -> header info
        
        # Performance tracking
        self.fps_counter = 0
        self.fps_start_time = time.time()
        self.current_fps = 0
        
        # Socket setup
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self.sock.bind(('0.0.0.0', port))
        
        print(f"UDP server listening on port {port}")
        print("Waiting for ESP32-CAM discovery...")
        
    def handle_discovery(self, data, addr):
        """Handle ESP32-CAM discovery broadcast"""
        if data == b"ESP32CAM_DISCOVERY":
            print(f"ESP32-CAM discovered at {addr[0]}")
            # Send response back
            response = b"SERVER_FOUND"
            self.sock.sendto(response, addr)
            return addr[0]
        return None
        
    def receive_packets(self):
        """Receive UDP packets in dedicated thread"""
        self.sock.settimeout(1)  # 1 second timeout
        esp32_ip = None
        
        while self.running:
            try:
                data, addr = self.sock.recvfrom(65535)  # Max UDP size
                
                # Handle discovery first
                if not esp32_ip:
                    discovered_ip = self.handle_discovery(data, addr)
                    if discovered_ip:
                        esp32_ip = discovered_ip
                        continue
                
                # Only accept packets from known ESP32
                if esp32_ip and addr[0] != esp32_ip:
                    continue
                    
                if len(data) < 1:
                    continue
                    
                packet_type = data[0]
                
                if packet_type == 0xFF:  # Header packet
                    # Unpack header
                    header = struct.unpack('<BIII H I', data[:17])
                    frame_id = header[1]
                    total_size = header[2]
                    total_packets = header[3]
                    timestamp = header[4]
                    
                    self.frame_headers[frame_id] = {
                        'total_size': total_size,
                        'total_packets': total_packets,
                        'timestamp': timestamp,
                        'received_packets': 0
                    }
                    
                elif packet_type == 0x01:  # Data packet
                    # Unpack data header
                    header = struct.unpack('<BI H H', data[:9])
                    frame_id = header[1]
                    packet_num = header[2]
                    data_size = header[3]
                    
                    # Store packet data
                    if frame_id in self.frame_headers:
                        self.frames_buffer[frame_id][packet_num] = data[9:9+data_size]
                        self.frame_headers[frame_id]['received_packets'] += 1
                        
                        # Check if frame is complete
                        if self.frame_headers[frame_id]['received_packets'] == self.frame_headers[frame_id]['total_packets']:
                            self.process_complete_frame(frame_id)
                            
            except socket.timeout:
                continue
            except Exception as e:
                print(f"Receive error: {e}")
                
    def process_complete_frame(self, frame_id):
        """Process a complete frame"""
        try:
            # Reconstruct frame data
            frame_data = b''
            total_packets = self.frame_headers[frame_id]['total_packets']
            
            for packet_num in range(total_packets):
                if packet_num in self.frames_buffer[frame_id]:
                    frame_data += self.frames_buffer[frame_id][packet_num]
            
            # Decode JPEG
            nparr = np.frombuffer(frame_data, np.uint8)
            frame = cv2.imdecode(nparr, cv2.IMREAD_COLOR)
            
            if frame is not None:
                self.display_frame(frame)
                self.fps_counter += 1
                
                # Calculate FPS
                current_time = time.time()
                if current_time - self.fps_start_time >= 1.0:
                    self.current_fps = self.fps_counter / (current_time - self.fps_start_time)
                    self.fps_start_time = current_time
                    self.fps_counter = 0
                    
            # Cleanup
            del self.frames_buffer[frame_id]
            del self.frame_headers[frame_id]
            
        except Exception as e:
            print(f"Frame processing error: {e}")
            
    def display_frame(self, frame):
        """Display frame with minimal processing"""
        try:
            # Resize small frames for better visibility
            height, width = frame.shape[:2]
            if width < 200:  # If frame is too small (like 96x96)
                # Scale up by 4x for visibility
                frame = cv2.resize(frame, (width*4, height*4), interpolation=cv2.INTER_NEAREST)
            
            # Add performance info
            fps_text = f"UDP FPS: {self.current_fps:.1f} | Resolution: {width}x{height}"
            cv2.rectangle(frame, (5, 5), (400, 30), (0, 0, 0), -1)
            cv2.putText(frame, fps_text, (10, 22), cv2.FONT_HERSHEY_SIMPLEX, 0.5, (0, 255, 0), 1)
            
            cv2.imshow('ESP32-CAM UDP Stream [ULTRA FAST]', frame)
            
            key = cv2.waitKey(1) & 0xFF
            if key == ord('q'):
                print("Quit key pressed")
                self.running = False
        except Exception as e:
            print(f"Display error: {e}")
            
    def start(self):
        """Start the UDP receiver"""
        print("Starting ultra-fast UDP receiver...")
        
        # Start packet receiver thread
        receiver_thread = threading.Thread(target=self.receive_packets)
        receiver_thread.daemon = True
        receiver_thread.start()
        
        # Main loop for OpenCV
        while self.running:
            try:
                time.sleep(0.01)  # Very short sleep
            except KeyboardInterrupt:
                break
                
        # Cleanup
        self.sock.close()
        cv2.destroyAllWindows()
        print("UDP receiver stopped")

def main():
    print("=" * 60)
    print("UDP ESP32-CAM Receiver Starting...")
    print("=" * 60)
    print("Starting UDP receiver and OpenCV display...")
    
    receiver = UltraFastUDPReceiver()
    try:
        receiver.start()
    except KeyboardInterrupt:
        print("\nStopping UDP receiver...")
        receiver.running = False
    except Exception as e:
        print(f"Error: {e}")
    finally:
        cv2.destroyAllWindows()
        print("UDP receiver stopped")

if __name__ == "__main__":
    main()