//
//  PanelModel.swift
//  Panelcast
//
//  Created by Peter Vorwieger on 05.04.26.
//


import SwiftUI
import UIKit
internal import Combine

final class PanelModel: ObservableObject, Identifiable {
    let id: UInt16          // panelID aus deinem Protokoll
    @Published var image: UIImage?
    @Published var width: Int = 0
    @Published var height: Int = 0
    
    init(id: UInt16) {
        self.id = id
    }
}
