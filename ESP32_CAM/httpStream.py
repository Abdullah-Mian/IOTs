import cv2
import requests
import numpy as np
import time

# Replace with your ESP32-CAM IP
ESP32_CAM_URL = 'http://192.168.1.100:81/stream'  # stream usually runs on port 81

def connect_to_stream(url, max_retries=5):
    for attempt in range(max_retries):
        print(f"Attempt {attempt + 1}: Connecting to {url}")
        cap = cv2.VideoCapture(url)
        ret, frame = cap.read()
        if ret:
            print("✓ Connected to ESP32-CAM stream")
            return cap
        cap.release()
        print("✗ Failed, retrying...")
        time.sleep(2)
    return None

def main():
    cap = connect_to_stream(ESP32_CAM_URL)
    if cap is None:
        print("Could not connect to ESP32-CAM")
        return

    frame_count = 0
    while True:
        ret, frame = cap.read()
        if not ret:
            print("Stream lost. Reconnecting...")
            cap = connect_to_stream(ESP32_CAM_URL, 3)
            if cap is None:
                break
            continue

        frame_count += 1
        cv2.putText(frame, f"Frame: {frame_count}", (10, 30),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.7, (0, 255, 0), 2)

        cv2.imshow("ESP32-CAM Stream", frame)
        if cv2.waitKey(1) & 0xFF == ord('q'):
            break

    cap.release()
    cv2.destroyAllWindows()

if __name__ == "__main__":
    main()
