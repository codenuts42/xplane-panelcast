//
//  FragmentBuffer.swift
//  Panelcast
//
//  Created by Peter Vorwieger on 06.04.26.
//

import Foundation

final class FragmentBuffer {
    var frameID: UInt32 = 0
    var fragCount: UInt16 = 0
    var received: [UInt16: Data] = [:]

    func reset(frameID: UInt32, fragCount: UInt16) {
        self.frameID = frameID
        self.fragCount = fragCount
        received.removeAll()
    }

    func addFragment(index: UInt16, data: Data) {
        received[index] = data
    }

    var isComplete: Bool {
        received.count == Int(fragCount)
    }

    func assemble() -> Data {
        (0 ..< fragCount).reduce(into: Data()) { result, i in
            result.append(received[i]!)
        }
    }
}
