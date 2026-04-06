//
//  FrameReassembler.swift
//  Panelcast
//
//  Created by Peter Vorwieger on 06.04.26.
//

import Foundation

final class FrameReassembler {
    private var buffers: [UInt16: FragmentBuffer] = [:]

    func process(_ data: Data) -> Frame? {
        guard let hdr = PacketHeader(data: data),
              hdr.magic == 0xABCD1234 else { return nil }

        let payloadStart = PacketHeader.size
        let payloadEnd = payloadStart + Int(hdr.payloadSize)
        guard payloadEnd <= data.count else { return nil }

        let payload = data.subdata(in: payloadStart ..< payloadEnd)

        let buf = buffers[hdr.panelID] ?? FragmentBuffer()

        if buf.frameID != hdr.frameID {
            buf.reset(frameID: hdr.frameID, fragCount: hdr.fragCount)
        }

        buf.addFragment(index: hdr.fragIndex, data: payload)
        buffers[hdr.panelID] = buf

        guard buf.isComplete else { return nil }

        let assembled = buf.assemble()
        buffers[hdr.panelID] = nil

        return Frame(
            panelID: hdr.panelID,
            width: Int(hdr.width),
            height: Int(hdr.height),
            compressed: assembled
        )
    }
}
