//
//  PanelcastApp.swift
//  Panelcast
//
//  Created by Peter Vorwieger on 05.04.26.
//

import SwiftUI

@main
struct PanelcastApp: App {
    @StateObject private var store = PanelStore()
    private var receiver: UDPReceiver!

    init() {
        let store = PanelStore()
        _store = StateObject(wrappedValue: store)
        receiver = UDPReceiver(port: 5000, store: store)
        receiver.start()
    }

    var body: some Scene {
        WindowGroup {
            ContentView()
                .environmentObject(store)
        }
    }
}
