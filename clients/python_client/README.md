# Panelcast Python Client

A minimal Python client for receiving and decoding UDP frame streams sent by the Panelcast X‑Plane plugin.  
The client listens on a UDP port, decompresses incoming LZ4‑compressed frame data, and displays or processes the received panel images.

## Features
- Receives UDP packets from the Panelcast plugin
- Reassembles multi‑packet frames
- LZ4 decompression
- Panel display (via Pillow / OpenCV)
- Simple, dependency‑light implementation

## Requirements
- Python 3.9+
- `lz4` Python package
- `Pillow` and `opencv-python` for image display

Install dependencies:
```
pip install lz4 pillow
```

## Usage
```
python panelcast_client.py
```

## How It Works
1. Listens for UDP packets from the Panelcast plugin  
2. Reassembles packets into a complete frame  
3. Decompresses the frame using LZ4  
4. Converts raw RGBA data into an image  
5. Displays or processes the frame  

## Folder Structure
- `client/`
- `panelcast_client.py`
- `README.md`
