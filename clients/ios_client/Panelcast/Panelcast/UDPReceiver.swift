//
//  UDPReceiver.swift
//  Panelcast
//
//  Created by Peter Vorwieger on 06.04.26.
//

import Foundation

final class UDPReceiver {
    private var socketFD: Int32 = -1
    private let port: UInt16
    private let queue = DispatchQueue(label: "udp.receiver")
    private var running = false

    var onPacket: ((Data) -> Void)?

    init(port: UInt16) {
        self.port = port
    }

    func start() {
        guard !running else { return }
        running = true

        queue.async { [weak self] in
            self?.run()
        }
    }

    func stop() {
        running = false
        if socketFD >= 0 {
            close(socketFD)
            socketFD = -1
        }
    }

    private func run() {
        socketFD = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)
        guard socketFD >= 0 else { return }

        _ = fcntl(socketFD, F_SETFL, O_NONBLOCK)

        var addr = sockaddr_in()
        addr.sin_family = sa_family_t(AF_INET)
        addr.sin_port = port.bigEndian
        addr.sin_addr = in_addr(s_addr: INADDR_ANY.bigEndian)

        _ = withUnsafePointer(to: &addr) {
            $0.withMemoryRebound(to: sockaddr.self, capacity: 1) {
                bind(socketFD, $0, socklen_t(MemoryLayout<sockaddr_in>.size))
            }
        }

        var buffer = [UInt8](repeating: 0, count: 65535)

        while running {
            let size = recv(socketFD, &buffer, buffer.count, 0)
            if size > 0 {
                onPacket?(Data(buffer[0 ..< size]))
            } else {
                usleep(1000)
            }
        }
    }
}
