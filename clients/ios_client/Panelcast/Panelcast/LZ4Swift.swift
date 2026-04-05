//
//  LZ4Swift.swift
//  Panelcast
//
//  Created by Peter Vorwieger on 05.04.26.
//


import Foundation

enum LZ4Swift {

    static func compress(_ data: Data) -> Data? {
        let maxSize = LZ4_compressBound(Int32(data.count))
        var output = Data(count: Int(maxSize))

        let compressedSize = data.withUnsafeBytes { srcPtr in
            output.withUnsafeMutableBytes { dstPtr in
                LZ4_compress_default(
                    srcPtr.baseAddress!.assumingMemoryBound(to: CChar.self),
                    dstPtr.baseAddress!.assumingMemoryBound(to: CChar.self),
                    Int32(data.count),
                    maxSize
                )
            }
        }

        guard compressedSize > 0 else { return nil }
        output.count = Int(compressedSize)
        return output
    }

    static func decompress(_ data: Data, originalSize: Int) -> Data? {
        var output = Data(count: originalSize)

        let result = data.withUnsafeBytes { srcPtr in
            output.withUnsafeMutableBytes { dstPtr in
                LZ4_decompress_safe(
                    srcPtr.baseAddress!.assumingMemoryBound(to: CChar.self),
                    dstPtr.baseAddress!.assumingMemoryBound(to: CChar.self),
                    Int32(data.count),
                    Int32(originalSize)
                )
            }
        }

        guard result >= 0 else { return nil }
        return output
    }
}
