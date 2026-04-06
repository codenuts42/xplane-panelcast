//
//  PanelModel.swift
//  Panelcast
//
//  Created by Peter Vorwieger on 05.04.26.
//

import Combine
import SwiftUI
import UIKit

final class PanelModel: ObservableObject, Identifiable {
    let id: UInt16 // panelID
    @Published private(set) var image: UIImage?
    @Published private(set) var fps: Int = 0

    init(id: UInt16) {
        self.id = id
    }

    private var frameCount = 0
    private var lastTime = Date()

    func updateImage(_ newImage: UIImage) {
        DispatchQueue.main.async {
            self.image = newImage
        }

        frameCount += 1

        let now = Date()
        if now.timeIntervalSince(lastTime) >= 1.0 {
            fps = frameCount
            frameCount = 0
            lastTime = now
        }
    }
}
