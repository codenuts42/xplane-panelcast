//
//  ReceiverService.swift
//  Panelcast
//
//  Created by Peter Vorwieger on 05.04.26.
//

import SwiftUI

@MainActor
final class ReceiverService {
    /// UDP receiver is responsible for delivering raw packet data.
    private let receiver: UDPReceiver

    /// Reassembles fragmented or multi‑packet frames into complete image frames.
    private let reassembler = FrameReassembler()

    /// Initializes the receiver service and starts listening for incoming frames.
    ///
    /// - Parameter store: The central PanelStore that receives decoded UIImages.
    init(store: PanelStore) {
        receiver = UDPReceiver(port: 5000)

        receiver.onPacket = { [weak self] data in
            guard let self else { return }

            // Attempt to reassemble a full frame from incoming packet data.
            // If the frame is complete it will be returned.
            guard let frame = self.reassembler.process(data) else { return }

            // Decompress the raw pixel buffer (LZ4).
            let expectedSize = frame.width * frame.height * 4
            guard let raw = LZ4Swift.decompress(
                frame.compressed,
                originalSize: expectedSize
            ) else { return }

            // Convert raw RGBA bytes into a CGImage.
            guard let cgImage = ImageFactory.makeImage(
                width: frame.width,
                height: frame.height,
                raw: raw
            ) else { return }

            // Forward the final UIImage to the store.
            store.receiveFrame(
                panelID: frame.panelID,
                image: cgImage
            )
        }

        receiver.start()
    }
}
