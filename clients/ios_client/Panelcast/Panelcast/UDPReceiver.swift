//
//  UDPReceiver.swift
//  Panelcast
//
//  Created by Peter Vorwieger on 06.04.26.
//

import Foundation
import UIKit

final class UDPReceiver {
    private let port: UInt16
    private let store: PanelStore
    private var socketFD: Int32 = -1

    private var buffers: [UInt16: FragmentBuffer] = [:]
    private let queue = DispatchQueue(label: "udp.receiver.queue")

    init(port: UInt16, store: PanelStore) {
        self.port = port
        self.store = store
    }

    func start() {
        DispatchQueue.global(qos: .userInitiated).async {
            self.runSocketLoop()
        }
    }

    private func runSocketLoop() {
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
                handlePacket(data)
            }
        }
    }

    private func handlePacket(_ data: Data) {
        guard let hdr = PacketHeader(data: data),
              hdr.magic == 0xABCD1234 else { return }

        let payloadStart = PacketHeader.size
        let payloadEnd = payloadStart + Int(hdr.payloadSize)
        guard payloadEnd <= data.count else { return }

        let payload = data.subdata(in: payloadStart..<payloadEnd)

        queue.async {
            let buffer = self.buffers[hdr.panelID] ?? FragmentBuffer()

            if buffer.frameID != hdr.frameID {
                buffer.reset(frameID: hdr.frameID, fragCount: hdr.fragCount)
            }

            buffer.addFragment(index: hdr.fragIndex, data: payload)
            self.buffers[hdr.panelID] = buffer

            if buffer.isComplete {
                self.processCompleteFrame(
                    panelID: hdr.panelID,
                                          width: Int(hdr.width),
                                          height: Int(hdr.height),
                                          compSize: Int(hdr.compSize),
                                          buffer: buffer)
            }
        }
    }

    private func processCompleteFrame(panelID: UInt16,
                                      width: Int,
                                      height: Int,
                                      compSize: Int,
                                      buffer: FragmentBuffer)
    {
        let compressed = buffer.assemble()
        let expectedSize = width * height * 4

        guard let raw = LZ4Swift.decompress(compressed, originalSize: expectedSize),
              let image = ImageFactory.makeImage(width: width, height: height, raw: raw)
        else { return }

        DispatchQueue.main.async {
            let panel = self.store.panel(for: panelID)
            panel.width = width
            panel.height = height
            panel.image = image
        }
    }
}
