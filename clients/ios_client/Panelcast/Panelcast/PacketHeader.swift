//
//  PacketHeader.swift
//  Panelcast
//
//  Created by Peter Vorwieger on 06.04.26.
//

import Foundation

struct PacketHeader {
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
        guard data.count >= PacketHeader.size else { return nil }

        var offset = 0
        func readU32() -> UInt32 {
            defer { offset += 4 }
            return data.subdata(in: offset..<offset + 4)
                .withUnsafeBytes { $0.load(as: UInt32.self) }
                .littleEndian
        }
        func readU16() -> UInt16 {
            defer { offset += 2 }
            return data.subdata(in: offset..<offset + 2)
                .withUnsafeBytes { $0.load(as: UInt16.self) }
                .littleEndian
        }

        magic = readU32()
        frameID = readU32()
        panelID = readU16()
        fragIndex = readU16()
        fragCount = readU16()
        panelCount = readU16()
        payloadSize = readU32()
        width = readU16()
        height = readU16()
        compSize = readU32()
    }
}
