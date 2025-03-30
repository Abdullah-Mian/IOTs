from ultralytics import YOLO
import socket
import threading
import cv2

TCP_PORT = 5000
BUFFER_SIZE = 1024
SERVER_IP = '0.0.0.0'  # Bind to all interfaces
model = YOLO("best.pt")
latest_prediction = "No prediction yet"

def prediction_thread():
    global latest_prediction
    for result in model.predict(source="0", show=True, stream=True):
        labels = [model.names[int(cls)] for cls in result.boxes.cls]
        latest_prediction = ','.join(labels)
        if cv2.waitKey(1) & 0xFF == ord('q'):
            break

def handle_tcp_client(conn, addr):
    print(f"New TCP Connection from {addr}")
    with conn:
        while True:
            data = conn.recv(BUFFER_SIZE).decode()
            if not data or data.lower().strip() == 'exit':
                break
            if data.strip().lower() == "get_prediction":
                response = latest_prediction + '\n'  # Add newline
                conn.send(response.encode())
    conn.close()

# Start prediction thread
threading.Thread(target=prediction_thread, daemon=True).start()

# TCP server
with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
    s.bind((SERVER_IP, TCP_PORT))
    s.listen()
    print(f"TCP Server on {SERVER_IP}:{TCP_PORT}")
    while True:
        conn, addr = s.accept()
        threading.Thread(target=handle_tcp_client, args=(conn, addr)).start()