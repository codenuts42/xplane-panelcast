import socket
import struct
import lz4.block
from PIL import Image
import cv2
import numpy as np

HEADER_SIZE = 32

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.bind(("0.0.0.0", 5000))

current_frame = {}
expected_frags = 0
frameID = None

while True:
    data, addr = sock.recvfrom(1500)

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

        cv2.imshow("Panelcast", img_bgr)
        cv2.waitKey(1)
