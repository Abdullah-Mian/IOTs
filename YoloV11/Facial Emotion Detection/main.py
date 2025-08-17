from ultralytics import YOLO
import cv2
import threading
import asyncio
import websockets
import json
import socket

# WebSocket server configuration
WS_PORT = 8765
SERVER_IP = '0.0.0.0'  # Bind to all interfaces

# Initialize YOLO model
model = YOLO("best2.pt")

# Global variables
latest_prediction = {"character": "None", "confidence": 0.0}
connected_clients = set()

# Print server IP addresses to help with debugging
def print_server_ips():
    hostname = socket.gethostname()
    print(f"Hostname: {hostname}")
    print("Available IP addresses:")
    try:
        # Get all IP addresses
        addresses = socket.getaddrinfo(hostname, None)
        
        # Print IPv4 addresses
        for addr in addresses:
            if addr[0] == socket.AF_INET:  # IPv4
                print(f"  IPv4: {addr[4][0]}")
    except:
        print("  Could not get IP addresses")
    
    print(f"WebSocket server will listen on all interfaces (0.0.0.0) port {WS_PORT}")

# Fixed WebSocket handler - compatible with both older and newer websockets library versions
async def websocket_handler(websocket):
    """Handle WebSocket connections and send predictions to clients"""
    client_ip = websocket.remote_address[0] if hasattr(websocket, 'remote_address') else "unknown"
    print(f"New WebSocket client connected: {client_ip}")
    connected_clients.add(websocket)
    try:
        # Send the current prediction immediately upon connection
        await websocket.send(json.dumps(latest_prediction))
        print(f"Sent initial prediction to {client_ip}: {latest_prediction}")
        
        # Keep connection open and listen for any messages
        async for message in websocket:
            print(f"Received message from {client_ip}: {message}")
            if message.strip().lower() == "get_prediction":
                print(f"Sending prediction to {client_ip}: {latest_prediction}")
                await websocket.send(json.dumps(latest_prediction))
    except websockets.exceptions.ConnectionClosed:
        print(f"Client disconnected: {client_ip}")
    except Exception as e:
        print(f"Error handling client {client_ip}: {e}")
    finally:
        if websocket in connected_clients:
            connected_clients.remove(websocket)

async def broadcast_predictions():
    """Broadcast predictions to all connected clients"""
    while True:
        if connected_clients:  # Only send if clients are connected
            print(f"Broadcasting to {len(connected_clients)} clients: {latest_prediction}")
            for websocket in connected_clients.copy():
                try:
                    await websocket.send(json.dumps(latest_prediction))
                except websockets.exceptions.ConnectionClosed:
                    if websocket in connected_clients:
                        connected_clients.remove(websocket)
                except Exception as e:
                    print(f"Error broadcasting: {e}")
                    if websocket in connected_clients:
                        connected_clients.remove(websocket)
        await asyncio.sleep(1)  # Send predictions once per second

def run_prediction():
    """Run YOLO predictions on webcam feed"""
    global latest_prediction
    cap = cv2.VideoCapture(0)
    
    if not cap.isOpened():
        print("Error: Could not open camera.")
        return
        
    print("Starting character recognition...")
    
    try:
        for result in model.predict(source=0, show=True, stream=True):
            if len(result.boxes) > 0:
                # Get the highest confidence detection
                conf = float(result.boxes.conf[0])
                cls_id = int(result.boxes.cls[0])
                character = model.names[cls_id]
                
                # Update latest prediction with character and confidence
                latest_prediction = {
                    "character": character,
                    "confidence": round(conf, 2)
                }
                
                print(f"Detected: {character} (Confidence: {conf:.2f})")
            
            # Press 'q' to quit
            if cv2.waitKey(1) & 0xFF == ord('q'):
                break
    finally:
        cap.release()
        cv2.destroyAllWindows()

async def main():
    """Main async function to start everything"""
    # Print IP addresses
    print_server_ips()
    
    # Start YOLO prediction in a separate thread
    prediction_thread = threading.Thread(target=run_prediction, daemon=True)
    prediction_thread.start()
    
    # Start WebSocket server
    print("Starting WebSocket server...")
    async with websockets.serve(websocket_handler, SERVER_IP, WS_PORT):
        # Start broadcaster task
        broadcast_task = asyncio.create_task(broadcast_predictions())
        
        # Keep running forever
        await asyncio.Future()  # Run forever

if __name__ == "__main__":
    # Run the main async function
    asyncio.run(main())