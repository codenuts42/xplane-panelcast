//
//  ReceiverService.swift
//  Panelcast
//
//  Created by Peter Vorwieger on 05.04.26.
//

import SwiftUI

@MainActor
final class ReceiverService {
    private let receiver: UDPReceiver
    private let reassembler = FrameReassembler()

    init(store: PanelStore) {
        receiver = UDPReceiver(port: 5000)

        receiver.onPacket = { [weak self] data in
            guard let self else { return }

            if let frame = self.reassembler.process(data) {
                self.handle(frame: frame, store: store)
            }
        }

        receiver.start()
    }

    private func handle(frame: Frame, store: PanelStore) {
        let width = frame.width
        let height = frame.height
        let expectedSize = width * height * 4

        if let raw = LZ4Swift.decompress(frame.compressed, originalSize: expectedSize),
           let cgImage = ImageFactory.makeImage(width: width, height: height, raw: raw)
        {
            let uiImage = UIImage(cgImage: cgImage)
            let panel = store.panel(for: frame.panelID)
            panel.image = uiImage
        }
    }
}
