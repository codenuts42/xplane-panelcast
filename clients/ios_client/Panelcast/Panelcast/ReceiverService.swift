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

    private let metalConverter = MetalRGB565Converter()

    /// Initializes the receiver service and starts listening for incoming frames.
    ///
    /// - Parameter store: The central PanelStore that receives decoded UIImages.
    init(store: PanelStore) {
        receiver = UDPReceiver(port: store.udpPort)

        receiver.onPacket = { [weak self] data in
            guard let self else { return }

            Task { // wichtig: async Kontext
                guard let frame = self.reassembler.process(data) else { return }

                // 1) LZ4 → RGB565
                let expectedSize = frame.width * frame.height * 2
                guard let rgb565 = LZ4Swift.decompress(
                    frame.compressed,
                    originalSize: expectedSize
                )
                else { return }

                // 2) GPU → RGBA8
                guard let rgba = self.metalConverter?.convertRGB565ToRGBA(
                    rgb565Data: rgb565,
                    width: frame.width,
                    height: frame.height
                ) else { return }

                // 3) RGBA8 → CGImage
                guard let cgImage = ImageFactory.makeImage(
                    width: frame.width,
                    height: frame.height,
                    raw: rgba
                ) else { return }

                // 4) SwiftUI‑Update IMMER auf MainActor
                await MainActor.run {
                    store.receiveFrame(panelID: frame.panelID, image: cgImage)
                }
            }
        }

        receiver.start()
    }
}
