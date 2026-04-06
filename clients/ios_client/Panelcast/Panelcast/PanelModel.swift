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

    init(id: UInt16) {
        self.id = id
    }

    private var frameCount = 0
    private var lastTime = Date()
    private var lastFrameTime = Date()

    func updateImage(_ newImage: UIImage) {
        DispatchQueue.main.async {
            self.image = newImage
        }
        frameCount += 1
        lastFrameTime = Date()
    }

    var isTimedOut: Bool {
        Date().timeIntervalSince(lastFrameTime) > 1.0 // 2 Sekunden ohne Daten
    }
}
