//
//  PacketHeader.swift
//  Panelcast
//
//  Created by Peter Vorwieger on 06.04.26.
//

import Foundation

struct PacketHeader {
    /// 4 + 4 + 2 + 2 + 2 + 4 + 2 + 2 + 4 = 28 Bytes
    static let size = 26

    let magic: UInt32
    let frameID: UInt32
    let panelID: UInt16
    let fragIndex: UInt16
    let fragCount: UInt16
    let payloadSize: UInt32
    let width: UInt16
    let height: UInt16
    let compSize: UInt32

    init?(data: Data) {
        guard data.count >= PacketHeader.size else { return nil }

        var offset = 0

        func read<T: FixedWidthInteger>(_ type: T.Type) -> T {
            let value = data.withUnsafeBytes { ptr in
                ptr.load(fromByteOffset: offset, as: T.self)
            }
            offset += MemoryLayout<T>.size
            return T(littleEndian: value)
        }

        self.magic = read(UInt32.self)
        self.frameID = read(UInt32.self)
        self.panelID = read(UInt16.self)
        self.fragIndex = read(UInt16.self)
        self.fragCount = read(UInt16.self)

        self.payloadSize = read(UInt32.self)
        self.width = read(UInt16.self)
        self.height = read(UInt16.self)
        self.compSize = read(UInt32.self)
    }
}
