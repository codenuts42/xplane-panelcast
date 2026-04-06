//
//  PanelcastApp.swift
//  Panelcast
//
//  Created by Peter Vorwieger on 05.04.26.
//

import SwiftUI

@main
struct PanelcastApp: App {
    @StateObject private var store: PanelStore
    @State private var receiverService: ReceiverService

    init() {
        let store = PanelStore()
        _store = StateObject(wrappedValue: store)
        _receiverService = State(initialValue: ReceiverService(store: store))
    }

    var body: some Scene {
        WindowGroup {
            ContentView()
                .environmentObject(store)
                .statusBarHidden(true)
        }
    }
}
