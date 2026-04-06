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

            if store.panels.isEmpty {
                Text("Waiting for panels…")
                    .foregroundColor(.gray)
                    .font(.title2)
            } else {
                LazyVGrid(
                    columns: [
                        GridItem(.flexible()),
                        GridItem(.flexible())
                    ],
                    spacing: 16
                ) {
                    ForEach(store.panels.values.sorted(by: { $0.id < $1.id })) { panel in
                        PanelView(model: panel)
                            .frame(minHeight: 200, maxHeight: 500)
                            .cornerRadius(8)
                    }
                }
                .padding()
            }
        }
    }
}

struct PanelView: View {
    @ObservedObject var model: PanelModel

    var body: some View {
        if let image = model.image {
            Image(uiImage: image)
                .resizable()
                .scaledToFit()
        } else {
            Text("Panel \(model.id)\nWaiting for frames…")
                .foregroundColor(.gray)
                .multilineTextAlignment(.center)
                .padding()
        }
    }
}
