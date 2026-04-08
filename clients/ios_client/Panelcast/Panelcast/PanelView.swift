//
//  PanelView.swift
//  Panelcast
//
//  Created by Peter Vorwieger on 06.04.26.
//

import SwiftUI

struct PanelView: View {
    @ObservedObject var model: PanelModel
    let maxWidth: CGFloat
    let maxHeight: CGFloat

    var body: some View {
        if let image = model.image {
            let scale = min(
                1,
                maxWidth / CGFloat(image.width),
                maxHeight / CGFloat(image.height)
            )
            Image(image, scale: scale, label: Text(""))
                .resizable()
                .scaledToFit()
                .scaleEffect(x: 1, y: -1) // flip
                .animation(.easeInOut(duration: 0.25), value: scale)
        } else {
            Text("Panel \(model.id)\nWaiting for frames…")
                .foregroundColor(.gray)
        }
    }
}
