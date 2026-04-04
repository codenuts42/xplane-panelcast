# Panelcast
### Streaming of cockpit panel regions via UDP

Panelcast is an X‑Plane plugin that captures defined cockpit panel regions, compresses them using LZ4, and streams them over UDP to external applications. It is designed for low‑latency panel replication for custom avionics, remote displays, and home‑cockpit systems.

## Features
- Capture arbitrary cockpit panel regions (ROIs)
- Fast LZ4 compression
- High‑frequency UDP streaming
- OpenGL framebuffer readback
- Lightweight C++ architecture
- Cross‑platform X‑Plane plugin (`.xpl`)

## Techniques Used
- X‑Plane SDK (draw callbacks, flight loops, DataRefs)
- Trampoline callbacks to bridge C API → C++ methods
- OpenGL pixel readback via `glReadPixels`
- LZ4 compression (static build)
- Non‑blocking UDP networking

## Build Requirements
- CMake ≥ 3.16  
- Clang, MSVC, or GCC  
- X‑Plane SDK  
- OpenGL 3.3+  

## Project Structure
- `src/` – Core plugin sources
- `third_party/` – GLAD loader
- `SDK/` – X‑Plane SDK headers and libraries

## Client
A simple Python client for receiving and decoding Panelcast UDP frames is located in:
- `client/panelcast_client.py`

