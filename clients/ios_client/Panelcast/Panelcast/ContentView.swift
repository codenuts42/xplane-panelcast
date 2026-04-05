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
        LazyVGrid(
            columns: [GridItem(.flexible()),
                      GridItem(.flexible())],
            spacing: 16) {
                ForEach(Array(store.panels.values), id: \.id) { panel in
                    PanelView(model: panel)
                        .frame(minHeight: 200)
                        .background(Color.black.opacity(0.8))
                        .cornerRadius(8)
                }
            }.padding()
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
                    .scaleEffect(x: 1, y: -1)
            } else {
                Text("Panel \(model.id)\nWaiting for frames…")
                    .foregroundColor(.gray)
                    .multilineTextAlignment(.center)
                    .padding()
            }
        }
    }
}

