//
//  PanelStore.swift
//  Panelcast
//
//  Created by Peter Vorwieger on 05.04.26.
//

import Combine
import SwiftUI

final class PanelStore: ObservableObject {
    @Published var panels: [UInt16: PanelModel] = [:]

    func panel(for id: UInt16) -> PanelModel {
        if let existing = panels[id] {
            return existing
        }
        let model = PanelModel(id: id)
        panels[id] = model
        return model
    }

    func removePanel(id: UInt16) {
        panels.removeValue(forKey: id)
    }
}
