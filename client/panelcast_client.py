"""
@file client.py
@brief Multi-panel UDP client for receiving, reassembling, decompressing,
       and displaying LZ4-compressed panel frames sent by the Panelcast plugin.

The client listens for fragmented UDP packets, reconstructs full frames per panel,
decompresses them using LZ4, converts them to OpenCV images, and displays them
with live FPS information.

(c) 2025 Peter Vorwieger — All rights reserved.
"""

import socket
import struct
import lz4.block
import cv2
import numpy as np
import time

# ---------------------------------------------------------------------------
# Configuration
# ---------------------------------------------------------------------------

#: Size of the UDP fragment header sent by the plugin (bytes)
HEADER_SIZE = 28

# Create UDP socket and bind to port 5000
sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.bind(("0.0.0.0", 5000))
sock.settimeout(2.0)

# Per‑panel reassembly state:
#   panelID → {frameID, fragCount, received, width, height, compSize, frags{}}
panels = {}

# Per‑panel FPS timing
last_time = {}


# ---------------------------------------------------------------------------
# Panel state management
# ---------------------------------------------------------------------------

def init_panel(panelID):
    """
    @brief Initializes the reassembly state for a new panel.

    Each panel keeps track of:
      - current frame ID
      - expected fragment count
      - received fragments
      - frame dimensions
      - compressed size
      - fragment payloads
    """
    panels[panelID] = {
        "frameID": None,
        "fragCount": 0,
        "received": 0,
        "width": 0,
        "height": 0,
        "rawSize": 0,
        "compSize": 0,
        "frags": {}
    }
    last_time[panelID] = time.time()


# ---------------------------------------------------------------------------
# Fragment handling
# ---------------------------------------------------------------------------

def handle_fragment(data):
    """
    @brief Processes a single UDP fragment.

    Validates the header, initializes panel state if needed, stores the fragment,
    and triggers frame assembly once all fragments have been received.
    """
    if len(data) < HEADER_SIZE:
        return

    # Header layout (little endian):
    # magic, frameID, panelID, fragIndex, fragCount, panelCount, payloadSize, width, height, compSize
    hdr = struct.unpack("<I I H H H H I H H I", data[:HEADER_SIZE])
    magic, frameID, panelID, fragIndex, fragCount, panelCount, payloadSize, width, height, compSize = hdr

    # Validate magic number
    if magic != 0xABCD1234:
        return

    # Initialize panel state if first time seen
    if panelID not in panels:
        init_panel(panelID)

    p = panels[panelID]

    # New frame detected?
    if frameID != p["frameID"]:
        p["frameID"] = frameID
        p["fragCount"] = fragCount
        p["received"] = 0
        p["width"] = width
        p["height"] = height
        p["compSize"] = compSize
        p["frags"] = {}

    # Store fragment payload
    payload = data[HEADER_SIZE:HEADER_SIZE + payloadSize]
    p["frags"][fragIndex] = payload
    p["received"] = len(p["frags"])

    # All fragments received?
    if p["received"] == p["fragCount"]:
        assemble_and_show(panelID)


# ---------------------------------------------------------------------------
# Frame assembly and display
# ---------------------------------------------------------------------------

def assemble_and_show(panelID):
    """
    @brief Reassembles all fragments of a panel frame, decompresses it,
           converts it to an OpenCV image, and displays it.

    Also computes and displays per-panel FPS.
    """
    p = panels[panelID]

    # Concatenate compressed fragments in correct order
    compressed = b"".join(p["frags"][i] for i in range(p["fragCount"]))

    # LZ4 decompression into raw RGBA8 framebuffer
    raw = lz4.block.decompress(
        compressed,
        uncompressed_size=p["width"] * p["height"] * 4
    )

    # Convert raw bytes → numpy RGBA → BGR → flipped image
    img = np.frombuffer(raw, dtype=np.uint8).reshape((p["height"], p["width"], 4))
    img_bgr = cv2.cvtColor(img, cv2.COLOR_RGBA2BGR)
    flipped = cv2.flip(img_bgr, 0)

    # FPS calculation
    now = time.time()
    fps = 1.0 / (now - last_time[panelID])
    last_time[panelID] = now

    title = f"Panel {panelID} - {p['width']}x{p['height']} - {fps:.0f} FPS"

    # Display window
    winname = f"Panel {panelID}"
    cv2.namedWindow(winname, cv2.WINDOW_AUTOSIZE)
    cv2.setWindowTitle(winname, title)
    cv2.imshow(winname, flipped)
    cv2.waitKey(1)


# ---------------------------------------------------------------------------
# Main loop
# ---------------------------------------------------------------------------

print("Multi-Panel Client running…")

try:
    while True:
        try:
            data, addr = sock.recvfrom(2000)
        except socket.timeout:
            print("Timeout - no data received")
            continue

        handle_fragment(data)

except KeyboardInterrupt:
    print("Exiting…")
