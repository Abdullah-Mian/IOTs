import socket
import cv2
import numpy as np
import threading
import time
import struct
from collections import defaultdict
from datetime import datetime

class DetailedUDPDiagnosticServer:
    def __init__(self, port=1234):
        self.port = port
        self.running = True
        self.frames_buffer = defaultdict(dict)  # frame_id -> {packet_num: data}
        self.frame_headers = {}  # frame_id -> header info
        
        # Diagnostics
        self.discovery_count = 0
        self.total_packets = 0
        self.frame_count = 0
        self.bytes_received = 0
        
        # Performance tracking
        self.fps_counter = 0
        self.fps_start_time = time.time()
        self.current_fps = 0
        
        # Socket setup
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self.sock.bind(('0.0.0.0', port))
        
        self.esp32_ip = None
        
        print("=" * 70)
        print("DETAILED UDP DIAGNOSTIC SERVER")
        print("=" * 70)
        print(f"Listening on port {port}")
        print("Waiting for ESP32-CAM discovery...")
        print("All packets will be logged with full details")
        print("=" * 70)
        
    def log_with_timestamp(self, message):
        """Log message with precise timestamp"""
        timestamp = datetime.now().strftime("%H:%M:%S.%f")[:-3]
        print(f"[{timestamp}] {message}")
        
    def analyze_packet(self, data, addr):
        """Detailed packet analysis"""
        self.total_packets += 1
        self.bytes_received += len(data)
        
        if len(data) == 0:
            self.log_with_timestamp(f"❌ Empty packet from {addr[0]}")
            return None
            
        packet_type = data[0]
        
        self.log_with_timestamp(f"📦 Packet #{self.total_packets} from {addr[0]}:{addr[1]}")
        self.log_with_timestamp(f"    Size: {len(data)} bytes")
        self.log_with_timestamp(f"    Type: 0x{packet_type:02X}")
        
        # Hex dump of first 32 bytes for debugging
        hex_dump = " ".join([f"{b:02X}" for b in data[:min(32, len(data))]])
        self.log_with_timestamp(f"    Hex:  {hex_dump}")
        
        return packet_type
    
    def handle_discovery(self, data, addr):
        """Handle and log discovery packets in detail"""
        if data == b"ESP32CAM_DISCOVERY":
            self.discovery_count += 1
            self.log_with_timestamp(f"🔍 DISCOVERY #{self.discovery_count} from {addr[0]}")
            self.log_with_timestamp(f"    Content: {data.decode('utf-8', errors='ignore')}")
            self.log_with_timestamp(f"    Length: {len(data)} bytes")
            
            # Set ESP32 IP
            if not self.esp32_ip:
                self.esp32_ip = addr[0]
                self.log_with_timestamp(f"✅ ESP32-CAM registered: {self.esp32_ip}")
            
            # Send response
            response = b"SERVER_FOUND"
            self.sock.sendto(response, addr)
            self.log_with_timestamp(f"↩️ Response sent: {response.decode('utf-8')}")
            self.log_with_timestamp(f"    Response size: {len(response)} bytes")
            
            return True
            
        return False
        
    def handle_frame_header(self, data, addr):
        """Handle frame header packets"""
        if len(data) < 11:  # Minimum header size
            self.log_with_timestamp(f"❌ Invalid header size: {len(data)} bytes")
            return False
            
        try:
            # Unpack header: type(1) + frame_id(4) + total_size(4) + total_packets(2)
            header_data = struct.unpack('<BIIH', data[:11])
            packet_type, frame_id, total_size, total_packets = header_data
            
            self.log_with_timestamp(f"🎬 FRAME HEADER")
            self.log_with_timestamp(f"    Frame ID: {frame_id}")
            self.log_with_timestamp(f"    Total size: {total_size} bytes")
            self.log_with_timestamp(f"    Total packets: {total_packets}")
            self.log_with_timestamp(f"    Expected frame size: {total_size / 1024:.1f} KB")
            
            # Store header info
            self.frame_headers[frame_id] = {
                'total_size': total_size,
                'total_packets': total_packets,
                'timestamp': time.time(),
                'received_packets': 0,
                'start_time': time.time()
            }
            
            return True
            
        except Exception as e:
            self.log_with_timestamp(f"❌ Header parsing error: {e}")
            return False
    
    def handle_frame_data(self, data, addr):
        """Handle frame data packets"""
        if len(data) < 9:  # Minimum data header size
            self.log_with_timestamp(f"❌ Invalid data packet size: {len(data)} bytes")
            return False
            
        try:
            # Unpack data header: type(1) + frame_id(4) + packet_num(2) + data_size(2)
            header_data = struct.unpack('<BIHH', data[:9])
            packet_type, frame_id, packet_num, data_size = header_data
            
            actual_data_size = len(data) - 9
            
            if frame_id not in self.frame_headers:
                self.log_with_timestamp(f"❌ Data packet for unknown frame: {frame_id}")
                return False
                
            self.log_with_timestamp(f"📄 FRAME DATA")
            self.log_with_timestamp(f"    Frame ID: {frame_id}")
            self.log_with_timestamp(f"    Packet: {packet_num + 1}/{self.frame_headers[frame_id]['total_packets']}")
            self.log_with_timestamp(f"    Data size: {actual_data_size} bytes (header says {data_size})")
            
            # Store packet data
            self.frames_buffer[frame_id][packet_num] = data[9:9+actual_data_size]
            self.frame_headers[frame_id]['received_packets'] += 1
            
            # Check if frame is complete
            if self.frame_headers[frame_id]['received_packets'] == self.frame_headers[frame_id]['total_packets']:
                elapsed = time.time() - self.frame_headers[frame_id]['start_time']
                self.log_with_timestamp(f"✅ FRAME COMPLETE ({elapsed:.3f}s)")
                self.process_complete_frame(frame_id)
                
            return True
            
        except Exception as e:
            self.log_with_timestamp(f"❌ Data packet parsing error: {e}")
            return False
    
    def process_complete_frame(self, frame_id):
        """Process and display a complete frame"""
        try:
            # Reconstruct frame data
            frame_data = b''
            total_packets = self.frame_headers[frame_id]['total_packets']
            
            for packet_num in range(total_packets):
                if packet_num in self.frames_buffer[frame_id]:
                    frame_data += self.frames_buffer[frame_id][packet_num]
            
            self.log_with_timestamp(f"🖼️ FRAME RECONSTRUCTION")
            self.log_with_timestamp(f"    Total data: {len(frame_data)} bytes")
            self.log_with_timestamp(f"    Expected: {self.frame_headers[frame_id]['total_size']} bytes")
            
            # Decode JPEG
            nparr = np.frombuffer(frame_data, np.uint8)
            frame = cv2.imdecode(nparr, cv2.IMREAD_COLOR)
            
            if frame is not None:
                self.frame_count += 1
                self.fps_counter += 1
                
                # Calculate FPS
                current_time = time.time()
                if current_time - self.fps_start_time >= 1.0:
                    self.current_fps = self.fps_counter / (current_time - self.fps_start_time)
                    self.fps_start_time = current_time
                    self.fps_counter = 0
                
                height, width = frame.shape[:2]
                
                self.log_with_timestamp(f"✅ FRAME DECODED")
                self.log_with_timestamp(f"    Resolution: {width}x{height}")
                self.log_with_timestamp(f"    Frame #{self.frame_count}")
                self.log_with_timestamp(f"    Current FPS: {self.current_fps:.1f}")
                
                # Display frame
                self.display_frame(frame, frame_id)
                
            else:
                self.log_with_timestamp(f"❌ JPEG decode failed")
                
            # Cleanup
            del self.frames_buffer[frame_id]
            del self.frame_headers[frame_id]
            
        except Exception as e:
            self.log_with_timestamp(f"❌ Frame processing error: {e}")
            
    def display_frame(self, frame, frame_id):
        """Display frame with diagnostic info"""
        try:
            height, width = frame.shape[:2]
            
            # Scale up if frame is small
            if width < 300:
                scale = max(2, 600 // width)
                new_width = width * scale
                new_height = height * scale
                frame = cv2.resize(frame, (new_width, new_height), interpolation=cv2.INTER_NEAREST)
            
            # Add diagnostic overlay
            info_text = [
                f"Frame ID: {frame_id}",
                f"Resolution: {width}x{height}",
                f"FPS: {self.current_fps:.1f}",
                f"Total Frames: {self.frame_count}",
                f"Packets Received: {self.total_packets}",
                f"Data: {self.bytes_received / 1024:.1f} KB"
            ]
            
            y_offset = 30
            for text in info_text:
                cv2.rectangle(frame, (10, y_offset - 20), (400, y_offset + 5), (0, 0, 0), -1)
                cv2.putText(frame, text, (15, y_offset), cv2.FONT_HERSHEY_SIMPLEX, 0.5, (0, 255, 0), 1)
                y_offset += 25
            
            cv2.imshow('ESP32-CAM UDP Stream [DIAGNOSTIC]', frame)
            
            key = cv2.waitKey(1) & 0xFF
            if key == ord('q'):
                self.log_with_timestamp("👋 Quit key pressed")
                self.running = False
                
        except Exception as e:
            self.log_with_timestamp(f"❌ Display error: {e}")
    
    def receive_packets(self):
        """Main packet receiving loop with detailed logging"""
        self.sock.settimeout(1)
        
        while self.running:
            try:
                data, addr = self.sock.recvfrom(65535)  # Max UDP size
                
                # Analyze packet
                packet_type = self.analyze_packet(data, addr)
                if packet_type is None:
                    continue
                
                # Only accept packets from known ESP32 (after discovery)
                if self.esp32_ip and addr[0] != self.esp32_ip:
                    self.log_with_timestamp(f"⚠️ Ignoring packet from unknown source: {addr[0]}")
                    continue
                
                # Handle different packet types
                if packet_type == 0xFF:  # Header packet
                    self.handle_frame_header(data, addr)
                elif packet_type == 0x01:  # Data packet
                    self.handle_frame_data(data, addr)
                else:
                    # Check if it's a discovery packet (string data)
                    if self.handle_discovery(data, addr):
                        continue
                    else:
                        self.log_with_timestamp(f"❓ Unknown packet type: 0x{packet_type:02X}")
                
                self.log_with_timestamp("─" * 50)  # Separator
                
            except socket.timeout:
                continue
            except Exception as e:
                self.log_with_timestamp(f"❌ Receive error: {e}")
                
    def start(self):
        """Start the diagnostic server"""
        print("🚀 Starting detailed UDP diagnostic server...")
        
        # Start packet receiver thread
        receiver_thread = threading.Thread(target=self.receive_packets)
        receiver_thread.daemon = True
        receiver_thread.start()
        
        # Main loop for statistics
        try:
            while self.running:
                time.sleep(5)  # Print stats every 5 seconds
                if self.total_packets > 0:
                    self.log_with_timestamp(f"📊 STATS: {self.discovery_count} discoveries, {self.frame_count} frames, {self.total_packets} packets, {self.bytes_received/1024:.1f} KB")
        except KeyboardInterrupt:
            self.log_with_timestamp("👋 Server stopped by user")
        finally:
            # Cleanup
            self.running = False
            self.sock.close()
            cv2.destroyAllWindows()
            print("\n🏁 Diagnostic server stopped")

def main():
    server = DetailedUDPDiagnosticServer()
    server.start()

if __name__ == "__main__":
    main()