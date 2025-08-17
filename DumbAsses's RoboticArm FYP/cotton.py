import cv2
import numpy as np

# Replace with your ESP32-CAM stream URL
STREAM_URL = 'http://192.168.137.71/stream'

# OpenCV VideoCapture for MJPEG stream
cap = cv2.VideoCapture(STREAM_URL)

while True:
    ret, frame = cap.read()
    if not ret:
        print("Failed to grab frame")
        break

    # Resize for performance (optional)
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
            cotton_coords.append((cx, cy))
            # Draw bounding box
            cv2.rectangle(frame, (x, y), (x + w, y + h), (0, 255, 0), 2)
            # Draw centroid
            cv2.circle(frame, (cx, cy), 4, (0, 0, 255), -1)
            # Put coordinates text
            cv2.putText(frame, f"({cx},{cy})", (x, y - 10), cv2.FONT_HERSHEY_SIMPLEX, 0.5, (255, 0, 0), 1)
    
    # Display coordinates in console for robotic arm use
    if cotton_coords:
        print("Cotton coordinates:", cotton_coords)

    # Show both windows
    cv2.imshow('Raw Stream with Bounding Boxes', frame)
    cv2.imshow('Cotton Mask (Black & White)', mask)

    # Exit on 'q' key
    if cv2.waitKey(1) & 0xFF == ord('q'):
        break

cap.release()
cv2.destroyAllWindows()
