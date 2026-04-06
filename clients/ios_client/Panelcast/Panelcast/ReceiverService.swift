//
//  ReceiverService.swift
//  Panelcast
//
//  Created by Peter Vorwieger on 05.04.26.
//

internal import Combine
import Foundation

final class ReceiverService: ObservableObject {
    private var receiver: UDPReceiver?

    init(store: PanelStore) {
        let r = UDPReceiver(port: 5000, store: store)
        r.start()
        receiver = r
    }
}
