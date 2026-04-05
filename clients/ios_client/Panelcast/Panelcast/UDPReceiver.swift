//
//  UDPReceiver.swift
//  Panelcast
//
//  Created by Peter Vorwieger on 05.04.26.
//


import Foundation
import UIKit

// Header: <I I H H H H I H H I> (little endian)
struct PanelHeader {
    let magic: UInt32
    let frameID: UInt32
    let panelID: UInt16
    let fragIndex: UInt16
    let fragCount: UInt16
    let panelCount: UInt16
    let payloadSize: UInt32
    let width: UInt16
    let height: UInt16
    let compSize: UInt32

    static let size = 4 + 4 + 2 + 2 + 2 + 2 + 4 + 2 + 2 + 4

    init?(data: Data) {
        guard data.count >= PanelHeader.size else { return nil }
        var offset = 0

        func readU32() -> UInt32 {
            defer { offset += 4 }
            return data.subdata(in: offset..<offset+4).withUnsafeBytes {
                $0.load(as: UInt32.self)
            }.littleEndian
        }

        func readU16() -> UInt16 {
            defer { offset += 2 }
            return data.subdata(in: offset..<offset+2).withUnsafeBytes {
                $0.load(as: UInt16.self)
            }.littleEndian
        }

        magic       = readU32()
        frameID     = readU32()
        panelID     = readU16()
        fragIndex   = readU16()
        fragCount   = readU16()
        panelCount  = readU16()
        payloadSize = readU32()
        width       = readU16()
        height      = readU16()
        compSize    = readU32()
    }
}

final class UDPReceiver {
    private let port: UInt16
    private let store: PanelStore
    private var socketFD: Int32 = -1

    // Panel‑State für Reassembly
    struct PanelState {
        var frameID: UInt32 = 0
        var fragCount: UInt16 = 0
        var frags: [UInt16: Data] = [:]
        var width: Int = 0
        var height: Int = 0
        var compSize: Int = 0
        var lastSeen: TimeInterval = Date().timeIntervalSince1970
    }

    private var states: [UInt16: PanelState] = [:]
    private let stateQueue = DispatchQueue(label: "panel.state.queue")

    init(port: UInt16, store: PanelStore) {
        self.port = port
        self.store = store
    }

    func start() {
        DispatchQueue.global(qos: .userInitiated).async {
            self.run()
        }

        // Periodisch Panels entfernen, die verschwunden sind
        Timer.scheduledTimer(withTimeInterval: 1.0, repeats: true) { [weak self] _ in
            self?.cleanupRemovedPanels(timeout: 1.0)
        }
    }

    private func run() {
        socketFD = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)
        guard socketFD >= 0 else { return }

        var addr = sockaddr_in()
        addr.sin_len = UInt8(MemoryLayout<sockaddr_in>.size)
        addr.sin_family = sa_family_t(AF_INET)
        addr.sin_port = port.bigEndian
        addr.sin_addr = in_addr(s_addr: INADDR_ANY.bigEndian)

        withUnsafePointer(to: &addr) {
            $0.withMemoryRebound(to: sockaddr.self, capacity: 1) {
                _ = bind(socketFD, $0, socklen_t(MemoryLayout<sockaddr_in>.size))
            }
        }

        var buffer = [UInt8](repeating: 0, count: 65535)

        while true {
            let received = recv(socketFD, &buffer, buffer.count, 0)
            if received > 0 {
                let data = Data(buffer[0..<received])
                handlePacket(data: data)
            }
        }
    }

    private func handlePacket(data: Data) {
        guard let hdr = PanelHeader(data: data) else { return }
        guard hdr.magic == 0xABCD1234 else { return }

        let panelID = hdr.panelID
        let frameID = hdr.frameID
        let fragIndex = hdr.fragIndex
        let fragCount = hdr.fragCount
        let width = Int(hdr.width)
        let height = Int(hdr.height)
        let payloadSize = Int(hdr.payloadSize)

        let payloadStart = PanelHeader.size
        let payloadEnd = payloadStart + payloadSize
        guard payloadEnd <= data.count else { return }
        let payload = data.subdata(in: payloadStart..<payloadEnd)

        stateQueue.async {
            var st = self.states[panelID] ?? PanelState()

            if st.frameID != frameID {
                st.frameID = frameID
                st.fragCount = fragCount
                st.frags.removeAll()
                st.width = width
                st.height = height
                st.compSize = Int(hdr.compSize)
            }

            st.frags[fragIndex] = payload
            st.lastSeen = Date().timeIntervalSince1970
            self.states[panelID] = st

            if st.frags.count == Int(st.fragCount) {
                self.assembleAndPublish(panelID: panelID, state: st)
                st.frags.removeAll()
                self.states[panelID] = st
            }
        }
    }

    private func assembleAndPublish(panelID: UInt16, state: PanelState) {
        let sortedKeys = state.frags.keys.sorted()
        var compressed = Data()
        for k in sortedKeys {
            if let frag = state.frags[k] {
                compressed.append(frag)
            }
        }

        let expectedSize = state.width * state.height * 4

        guard let raw = LZ4Swift.decompress(compressed, originalSize: expectedSize) else {
            return
        }

        let uiImage = raw.withUnsafeBytes { ptr -> UIImage? in
            guard let base = ptr.baseAddress else { return nil }
            let bytesPerPixel = 4
            let bytesPerRow = state.width * bytesPerPixel
            let colorSpace = CGColorSpaceCreateDeviceRGB()
            let bitmapInfo = CGBitmapInfo(rawValue:
                CGImageAlphaInfo.premultipliedLast.rawValue)

            guard let ctx = CGContext(data: UnsafeMutableRawPointer(mutating: base),
                                      width: state.width,
                                      height: state.height,
                                      bitsPerComponent: 8,
                                      bytesPerRow: bytesPerRow,
                                      space: colorSpace,
                                      bitmapInfo: bitmapInfo.rawValue) else {
                return nil
            }

            guard let cgImage = ctx.makeImage() else { return nil }
            return UIImage(cgImage: cgImage)
        }

        guard let image = uiImage else { return }

        DispatchQueue.main.async {
            let panel = self.store.panel(for: panelID)
            panel.width = state.width
            panel.height = state.height
            panel.image = image
        }
    }

    private func cleanupRemovedPanels(timeout: TimeInterval) {
        let now = Date().timeIntervalSince1970
        stateQueue.async {
            let toRemove = self.states.filter { now - $0.value.lastSeen > timeout }
                                      .map { $0.key }

            guard !toRemove.isEmpty else { return }

            DispatchQueue.main.async {
                for id in toRemove {
                    self.store.removePanel(id: id)
                }
            }

            for id in toRemove {
                self.states.removeValue(forKey: id)
            }
        }
    }
}

