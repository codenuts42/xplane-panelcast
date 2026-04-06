//
//  ContentView.swift
//  Panelcast
//
//  Created by Peter Vorwieger on 05.04.26.
//

import SwiftUI

struct ContentView: View {
    @EnvironmentObject var store: PanelStore

    var body: some View {
        ZStack {
            Color.black.ignoresSafeArea()
            LazyVGrid(
                columns: [
                    GridItem(.flexible()),
                    GridItem(.flexible())
                ],
                spacing: 16
            ) {
                ForEach(store.panels.values.sorted(by: { $0.id < $1.id })) { panel in
                    PanelView(model: panel)
                        .frame(minHeight: 200)
                        .cornerRadius(8)
                }
            }.padding()
        }.statusBarHidden(true)
    }
}

struct PanelView: View {
    @ObservedObject var model: PanelModel

    var body: some View {
        ZStack {
            if let image = model.image {
                Image(uiImage: image)
                    .resizable()
                    .scaledToFit()
                    .frame(
                        maxWidth: image.size.width,
                        maxHeight: image.size.height
                    )
            } else {
                Text("Panel \(model.id)\nWaiting for frames…")
                    .foregroundColor(.gray)
                    .multilineTextAlignment(.center)
                    .padding()
            }
        }
    }
}
