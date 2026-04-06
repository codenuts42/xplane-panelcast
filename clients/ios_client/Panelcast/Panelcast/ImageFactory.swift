//
//  ImageFactory.swift
//  Panelcast
//
//  Created by Peter Vorwieger on 06.04.26.
//

import UIKit

enum ImageFactory {
    static func makeImage(width: Int, height: Int, raw: Data) -> CGImage? {
        let bytesPerRow = width * 4

        return raw.withUnsafeBytes { ptr in
            guard ptr.baseAddress != nil else { return nil }

            let colorSpace = CGColorSpaceCreateDeviceRGB()

            guard let ctx = CGContext(
                data: nil,
                width: width,
                height: height,
                bitsPerComponent: 8,
                bytesPerRow: bytesPerRow,
                space: colorSpace,
                bitmapInfo: CGImageAlphaInfo.premultipliedLast.rawValue
            ) else { return nil }

            // flip upside down
            ctx.translateBy(x: 0, y: CGFloat(height))
            ctx.scaleBy(x: 1, y: -1)

            // draw raw pixel in context
            ctx.draw(
                CGImage(
                    width: width,
                    height: height,
                    bitsPerComponent: 8,
                    bitsPerPixel: 32,
                    bytesPerRow: bytesPerRow,
                    space: colorSpace,
                    bitmapInfo: CGBitmapInfo(rawValue: CGImageAlphaInfo.premultipliedLast.rawValue),
                    provider: CGDataProvider(data: raw as CFData)!,
                    decode: nil,
                    shouldInterpolate: false,
                    intent: .defaultIntent
                )!,
                in: CGRect(x: 0, y: 0, width: width, height: height)
            )

            return ctx.makeImage()
        }
    }
}
