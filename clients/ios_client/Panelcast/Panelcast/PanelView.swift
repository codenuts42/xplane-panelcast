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
            let original = image.size

            let scale = min(
                maxWidth / original.width,
                maxHeight / original.height,
                1
            )

            Image(uiImage: image)
                .resizable()
                .scaledToFit()
                .frame(
                    width: original.width * scale,
                    height: original.height * scale
                )
                .animation(.easeInOut(duration: 0.25), value: scale)

        } else {
            Text("Panel \(model.id)\nWaiting for frames…")
                .foregroundColor(.gray)
                .multilineTextAlignment(.center)
                .padding()
        }
    }
}
