//
//  ImageFactory.swift
//  Panelcast
//
//  Created by Peter Vorwieger on 06.04.26.
//


import UIKit

enum ImageFactory {
    static func makeImage(width: Int, height: Int, raw: Data) -> UIImage? {
        raw.withUnsafeBytes { ptr -> UIImage? in
            guard let base = ptr.baseAddress else { return nil }

            let bytesPerRow = width * 4
            let colorSpace = CGColorSpaceCreateDeviceRGB()
            let bitmapInfo = CGBitmapInfo(rawValue:
                CGImageAlphaInfo.premultipliedLast.rawValue)

            guard let ctx = CGContext(
                data: UnsafeMutableRawPointer(mutating: base),
                width: width,
                height: height,
                bitsPerComponent: 8,
                bytesPerRow: bytesPerRow,
                space: colorSpace,
                bitmapInfo: bitmapInfo.rawValue
            ) else { return nil }

            guard let cgImage = ctx.makeImage() else { return nil }
            return UIImage(cgImage: cgImage)
        }
    }
}
