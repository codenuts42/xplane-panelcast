"""
Multi-Panel UDP Client (Threaded + Panel Removal Detection)
-----------------------------------------------------------

Thread 1: UDP-Receiver (Reassembly)
Thread 2 (Main): Dekompression + Anzeige + Panel-Cleanup

Ein Panel-Fenster wird automatisch geschlossen, wenn das Panel
in der Sender-Konfiguration entfernt wurde (d.h. keine Frames
mehr für dieses Panel eintreffen).

(c) 2025 Peter Vorwieger — All rights reserved.
"""

import socket
import struct
import threading
import time

import cv2
import lz4.block
import numpy as np

import ctypes
ctypes.windll.user32.SetProcessDPIAware()

# ---------------------------------------------------------------------------
# Protokoll / Header
# ---------------------------------------------------------------------------

HDR_FORMAT = "<I I H H H I H H I"
HEADER_SIZE = struct.calcsize(HDR_FORMAT)
MAGIC = 0xABCD1234

# ---------------------------------------------------------------------------
# UDP Socket
# ---------------------------------------------------------------------------

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.bind(("0.0.0.0", 5000))
sock.setblocking(True)

# ---------------------------------------------------------------------------
# Panel‑State (geschützt durch Lock)
# ---------------------------------------------------------------------------

panels = {}          # panelID -> state
ready_frames = {}    # panelID -> compressed bytes
state_lock = threading.Lock()
last_time = {}       # panelID -> last display time


def init_panel(panelID):
    panels[panelID] = {
        "frameID": None,
        "fragCount": 0,
        "received": 0,
        "width": 0,
        "height": 0,
        "compSize": 0,
        "frags": {},
        "last_seen": time.time(),
    }
    last_time[panelID] = time.time()


# ---------------------------------------------------------------------------
# Receiver‑Thread: UDP + Reassembly
# ---------------------------------------------------------------------------

def handle_fragment(data):
    if len(data) < HEADER_SIZE:
        return

    hdr = struct.unpack(HDR_FORMAT, data[:HEADER_SIZE])
    magic, frameID, panelID, width, height, compSize, fragIndex, fragCount, payloadSize  = hdr

    if magic != MAGIC:
        return

    with state_lock:
        if panelID not in panels:
            init_panel(panelID)

        p = panels[panelID]
        p["last_seen"] = time.time()

        # Neues Frame?
        if frameID != p["frameID"]:
            p["frameID"] = frameID
            p["fragCount"] = fragCount
            p["received"] = 0
            p["width"] = width
            p["height"] = height
            p["compSize"] = compSize
            p["frags"] = {}

        payload = data[HEADER_SIZE:HEADER_SIZE + payloadSize]
        p["frags"][fragIndex] = payload
        p["received"] = len(p["frags"])

        if p["received"] == p["fragCount"]:
            compressed = b"".join(p["frags"][i] for i in range(p["fragCount"]))
            ready_frames[panelID] = {
                "compressed": compressed,
                "width": p["width"],
                "height": p["height"],
            }


def receiver_loop():
    while True:
        data, addr = sock.recvfrom(65535)
        handle_fragment(data)


# ---------------------------------------------------------------------------
# Panel‑Cleanup (Fenster schließen, wenn Panel entfernt wurde)
# ---------------------------------------------------------------------------

def cleanup_removed_panels(timeout=1.0):
    now = time.time()
    to_remove = []

    with state_lock:
        for panelID, p in panels.items():
            if now - p["last_seen"] > timeout:
                to_remove.append(panelID)

        for panelID in to_remove:
            print(f"Panel {panelID} removed — closing window")

            # State löschen
            del panels[panelID]
            ready_frames.pop(panelID, None)
            last_time.pop(panelID, None)

            # Fenster schließen
            cv2.destroyWindow(f"Panel {panelID}")


# ---------------------------------------------------------------------------
# Anzeige im Main‑Thread
# ---------------------------------------------------------------------------

def main_loop():
    print("Threaded Multi‑Panel Client running…")

    while True:
        # Panels entfernen, die verschwunden sind
        cleanup_removed_panels()

        # Kopie der fertigen Frames ziehen
        with state_lock:
            frames = dict(ready_frames)
            ready_frames.clear()

        # Pro Panel höchstens ein Frame verarbeiten
        for panelID, info in frames.items():
            compressed = info["compressed"]
            width = info["width"]
            height = info["height"]

            raw = lz4.block.decompress(
                compressed,
                uncompressed_size=width * height * 2
            )

            img = np.frombuffer(raw, dtype=np.uint8).reshape((height, width, 2))
            img_bgr = cv2.cvtColor(img, cv2.COLOR_BGR5652BGR)
            flipped = cv2.flip(img_bgr, 0)

            now = time.time()
            fps = 1.0 / (now - last_time[panelID])
            last_time[panelID] = now

            winname = f"Panel {panelID}"
            title = f"Panel {panelID} - {width}x{height} - {fps:.0f} FPS"

            cv2.namedWindow(winname, cv2.WINDOW_AUTOSIZE)
            cv2.setWindowTitle(winname, title)
            cv2.imshow(winname, flipped)

        cv2.waitKey(1)


# ---------------------------------------------------------------------------
# Start
# ---------------------------------------------------------------------------

if __name__ == "__main__":
    t = threading.Thread(target=receiver_loop, daemon=True)
    t.start()

    try:
        main_loop()
    except KeyboardInterrupt:
        print("Exiting…")
