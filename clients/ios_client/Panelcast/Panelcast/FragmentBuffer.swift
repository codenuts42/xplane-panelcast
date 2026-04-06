//
//  FragmentBuffer.swift
//  Panelcast
//
//  Created by Peter Vorwieger on 06.04.26.
//

import Foundation

final class FragmentBuffer {
    private(set) var frameID: UInt32 = 0
    private(set) var fragCount: UInt16 = 0
    private var fragments: [UInt16: Data] = [:]

    func reset(frameID: UInt32, fragCount: UInt16) {
        self.frameID = frameID
        self.fragCount = fragCount
        fragments.removeAll()
    }

    func addFragment(index: UInt16, data: Data) {
        fragments[index] = data
    }

    var isComplete: Bool {
        fragments.count == Int(fragCount)
    }

    func assemble() -> Data {
        fragments.keys.sorted().reduce(into: Data()) { result, key in
            if let frag = fragments[key] {
                result.append(frag)
            }
        }
    }
}
