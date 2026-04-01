import socket
import struct
import lz4.block
from PIL import Image
import cv2
import numpy as np
import time

HEADER_SIZE = 32

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.bind(("0.0.0.0", 5000))
sock.settimeout(2.0)  # 2 Sekunden warten

current_frame = {}
expected_frags = 0
frameID = None

last_time = time.time()

while True:
    try:
        data, addr = sock.recvfrom(1500)
    except socket.timeout:
        print("Timeout – keine Daten empfangen")
        continue

    if len(data) < HEADER_SIZE:
        continue

    hdr = struct.unpack("<I I H H I I I I I", data[:HEADER_SIZE])
    magic, fID, fragIndex, fragCount, payloadSize, width, height, rawSize, compSize = hdr

    if magic != 0xABCD1234:
        continue

    if fID != frameID:
        frameID = fID
        current_frame = {}
        expected_frags = fragCount

    current_frame[fragIndex] = data[HEADER_SIZE:HEADER_SIZE + payloadSize]

    if len(current_frame) == expected_frags:
        compressed = b"".join(current_frame[i] for i in range(expected_frags))

        # WICHTIG: raw LZ4 block decompression
        raw = lz4.block.decompress(compressed, uncompressed_size=rawSize)

        img = np.frombuffer(raw, dtype=np.uint8).reshape((height, width, 4))
        img_bgr = cv2.cvtColor(img, cv2.COLOR_RGBA2BGR)
        flipped = cv2.flip(img_bgr, 0)

        # FPS berechnen
        current_time = time.time()
        fps = 1.0 / (current_time - last_time)
        last_time = current_time

        # Titel zusammenbauen
        title = f"Panelcast – {width} x {height} – {fps:.1f} FPS"

        cv2.namedWindow("Panelcast", cv2.WINDOW_NORMAL)
        cv2.setWindowTitle("Panelcast", title)
        cv2.imshow("Panelcast", flipped)
        cv2.waitKey(1)
