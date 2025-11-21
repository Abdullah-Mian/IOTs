import socket
import numpy as np
import matplotlib.pyplot as plt
from collections import deque
import time

# --- Configuration ---
UDP_IP = "0.0.0.0"  # Listen on all available network interfaces
UDP_PORT = 12345    # Must match the port in the ESP32 code
CSI_WINDOW_SIZE = 200 # Number of CSI packets to hold for analysis
MOTION_THRESHOLD = 25 # An empirical threshold. You WILL need to adjust this!

# --- Global Variables ---
csi_data_deque = deque(maxlen=CSI_WINDOW_SIZE)
last_motion_time = 0

# --- Matplotlib Setup for Real-time Plotting ---
plt.ion()
fig, ax = plt.subplots()
line, = ax.plot([], [], alpha=0.8)
ax.set_ylim(-128, 127) # Raw CSI values are signed 8-bit integers
ax.set_xlim(0, 64)      # ESP32 provides 64 subcarriers of data for LLTF
ax.set_title("Real-time CSI Amplitude (First 64 Subcarriers)")
ax.set_xlabel("Subcarrier Index")
ax.set_ylabel("Amplitude (I value)")
fig.canvas.draw()
plt.show(block=False)


def process_csi_data(data: bytes):
    """
    Processes a raw CSI byte packet from the ESP32.
    """
    try:
        # The ESP32 sends raw bytes. We interpret them as signed 8-bit integers.
        # CSI data consists of I and Q values for each subcarrier.
        csi_raw = np.frombuffer(data, dtype=np.int8)

        # The data represents complex numbers (I + jQ).
        # For this visualization and simple motion detection, we'll just use the
        # 'I' values from the first 64 subcarriers (LLTF format).
        # This corresponds to the first 64 bytes of the payload.
        if len(csi_raw) >= 64:
            return csi_raw[:64]
        else:
            return None

    except Exception as e:
        print(f"Error processing CSI data: {e}")
        return None

def detect_motion():
    """
    Analyzes the variance in the CSI data deque to detect motion.
    """
    global last_motion_time
    # Wait until the buffer is full to have a stable baseline
    if len(csi_data_deque) < CSI_WINDOW_SIZE:
        return

    # Convert deque to a NumPy array for efficient calculation
    # Shape will be (CSI_WINDOW_SIZE, 64_subcarriers)
    csi_matrix = np.array(csi_data_deque)

    # Calculate the variance for each subcarrier across the time window
    variances = np.var(csi_matrix, axis=0)

    # Calculate the mean of these variances. This gives a single value
    # representing the overall signal instability.
    mean_variance = np.mean(variances)

    # Check if the instability exceeds our threshold
    if mean_variance > MOTION_THRESHOLD:
        # To avoid constant printing, only report motion once per second
        current_time = time.time()
        if current_time - last_motion_time > 1:
            print(f"MOTION DETECTED! (Variance: {mean_variance:.2f})")
            last_motion_time = current_time

def main():
    """
    Main function to set up UDP server and process incoming data.
    """
    # Create a UDP socket
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    # Bind the socket to the IP and port
    sock.bind((UDP_IP, UDP_PORT))
    print(f"Listening for CSI data on UDP port {UDP_PORT}...")

    packet_count = 0
    while True:
        try:
            # Receive data, buffer size is set to handle large packets
            data, addr = sock.recvfrom(4096)
            packet_count += 1

            csi_values = process_csi_data(data)

            if csi_values is not None:
                csi_data_deque.append(csi_values)

                # Update plot periodically to avoid overwhelming the CPU
                # A high packet rate can make matplotlib slow.
                if packet_count % 10 == 0:
                    line.set_ydata(csi_values)
                    line.set_xdata(np.arange(len(csi_values)))
                    fig.canvas.draw()
                    fig.canvas.flush_events()

                # Run motion detection algorithm on every packet
                detect_motion()

        except KeyboardInterrupt:
            print("\nStopping receiver...")
            break
        except Exception as e:
            print(f"An error occurred in the main loop: {e}")

    sock.close()
    plt.close('all')

if __name__ == "__main__":
    main()