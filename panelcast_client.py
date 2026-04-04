import socket
import struct
import lz4.block
import cv2
import numpy as np
import time

HEADER_SIZE = 28

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.bind(("0.0.0.0", 5000))
sock.settimeout(2.0)

# Reassembly pro Panel
panels = {}  # panelID → dict

# FPS pro Panel
last_time = {}

def init_panel(panelID):
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


def handle_fragment(data):
    if len(data) < HEADER_SIZE:
        return

    # Header: magic, frameID, panelID, fragIndex, fragCount, payloadSize, width, height, compSize
    hdr = struct.unpack("<I I H H H H I H H I", data[:HEADER_SIZE])
    magic, frameID, panelID, fragIndex, fragCount, panelCount, payloadSize, width, height, compSize = hdr

    if magic != 0xABCD1234:
        return

    # Panel initialisieren falls neu
    if panelID not in panels:
        init_panel(panelID)

    p = panels[panelID]

    # Neuer Frame?
    if frameID != p["frameID"]:
        p["frameID"] = frameID
        p["fragCount"] = fragCount
        p["received"] = 0
        p["width"] = width
        p["height"] = height
        p["compSize"] = compSize
        p["frags"] = {}

    # Fragment speichern
    payload = data[HEADER_SIZE:HEADER_SIZE + payloadSize]
    p["frags"][fragIndex] = payload
    p["received"] = len(p["frags"])

    # Frame vollständig?
    if p["received"] == p["fragCount"]:
        assemble_and_show(panelID)


def assemble_and_show(panelID):
    p = panels[panelID]

    # Komprimierte Daten zusammensetzen
    compressed = b"".join(p["frags"][i] for i in range(p["fragCount"]))

    # LZ4 dekomprimieren
    raw = lz4.block.decompress(compressed, uncompressed_size=p["width"] * p["height"] * 4)

    img = np.frombuffer(raw, dtype=np.uint8).reshape((p["height"], p["width"], 4))
    img_bgr = cv2.cvtColor(img, cv2.COLOR_RGBA2BGR)
    flipped = cv2.flip(img_bgr, 0)

    # FPS berechnen
    now = time.time()
    fps = 1.0 / (now - last_time[panelID])
    last_time[panelID] = now

    title = f"Panel {panelID} - {p['width']}x{p['height']} - {fps:.0f} FPS"

    winname = f"Panel_{panelID}"
    cv2.namedWindow(winname, cv2.WINDOW_AUTOSIZE)
    cv2.setWindowTitle(winname, title)
    cv2.imshow(winname, flipped)
    cv2.waitKey(1)


# -------------------------
# Hauptloop
# -------------------------

print("Multi-Panel-Client läuft…")

while True:
    try:
        data, addr = sock.recvfrom(2000)
    except socket.timeout:
        print("Timeout – keine Daten empfangen")
        continue

    handle_fragment(data)
