//
//  ContentView.swift
//  Panelcast
//
//  Created by Peter Vorwieger on 05.04.26.
//

import SwiftUI

struct ContentView: View {
    @EnvironmentObject var store: PanelStore

    let spacing: CGFloat = 16
    let minPanelWidth: CGFloat = 300 // Mindestbreite pro Panel

    var body: some View {
        GeometryReader { geo in
            ZStack {
                Color.black.ignoresSafeArea()

                if store.panels.isEmpty {
                    VStack(spacing: 36) {
                        Image("Logo").cornerRadius(16)
                        VStack(spacing: 8) {
                            Text("Waiting for panels…")
                                .foregroundColor(Color(white: 0.85))
                                .font(.title2)
                            Text("\(store.localIP):\(String(store.udpPort))")
                                .font(.footnote)
                                .foregroundColor(Color(white: 0.75))
                        }
                    }
                } else {
                    let availableWidth = geo.size.width - 32
                    let panelCount = store.panels.count
                    let columnsCount = min(panelCount, Int(availableWidth / minPanelWidth))
                    let rowsCount = min(1, panelCount / columnsCount)
                    let maxPanelHeight = geo.size.height / Double(rowsCount)
                    let columnWidth = (availableWidth - CGFloat(columnsCount - 1) * spacing)
                        / CGFloat(columnsCount)

                    let columns = Array(
                        repeating: GridItem(.flexible(), spacing: spacing),
                        count: columnsCount
                    )

                    LazyVGrid(columns: columns, spacing: spacing) {
                        ForEach(store.panels.values.sorted(by: { $0.id < $1.id })) { panel in
                            PanelView(
                                model: panel,
                                maxWidth: columnWidth,
                                maxHeight: maxPanelHeight
                            )
                            .cornerRadius(8)
                        }
                    }
                    .padding()
                }
            }
        }
    }
}
