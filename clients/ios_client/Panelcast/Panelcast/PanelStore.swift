//
//  PanelStore.swift
//  Panelcast
//
//  Created by Peter Vorwieger on 05.04.26.
//

import Combine
import SwiftUI

final class PanelStore: ObservableObject {
    @Published var localIP: String
    @Published var udpPort: UInt16 = 5000
    @Published var panels: [UInt16: PanelModel] = [:]

    private var timer: Timer?

    init() {
        localIP = getWiFiIPAddress() ?? "0.0.0.0"
        timer = Timer.scheduledTimer(withTimeInterval: 1.0, repeats: true) { _ in
            self.cleanupTimedOutPanels()
        }
    }

    func receiveFrame(panelID: UInt16, image: CGImage) {
        let panel = panels[panelID] ?? PanelModel(id: panelID)
        panel.updateImage(image)
        panels[panelID] = panel
    }

    private func cleanupTimedOutPanels() {
        for (id, panel) in panels {
            if panel.isTimedOut {
                panels.removeValue(forKey: id)
            }
        }
    }
}
