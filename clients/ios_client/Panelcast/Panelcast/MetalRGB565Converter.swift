//
//  MetalRGB565Converter.swift
//  Panelcast
//
//  Created by Peter Vorwieger on 13.04.26.
//

import Metal

final class MetalRGB565Converter {
    private let device: MTLDevice
    private let pipeline: MTLComputePipelineState
    private let queue: MTLCommandQueue

    init?() {
        guard let device = MTLCreateSystemDefaultDevice(),
              let library = device.makeDefaultLibrary(),
              let function = library.makeFunction(name: "rgb565_to_rgba8"),
              let queue = device.makeCommandQueue()
        else { return nil }

        self.device = device
        self.pipeline = try! device.makeComputePipelineState(function: function)
        self.queue = queue
    }

    func convertRGB565ToRGBA(rgb565Data: Data, width: Int, height: Int) -> Data? {
        let pixelCount = width * height
        let srcSize = pixelCount * 2
        let dstSize = pixelCount * 4

        guard rgb565Data.count == srcSize else { return nil }

        let srcBuffer = device.makeBuffer(
            bytes: (rgb565Data as NSData).bytes,
            length: srcSize,
            options: .storageModeShared
        )!

        let dstBuffer = device.makeBuffer(
            length: dstSize,
            options: .storageModeShared
        )!

        var count = UInt32(pixelCount)

        guard let cmd = queue.makeCommandBuffer(),
              let enc = cmd.makeComputeCommandEncoder()
        else { return nil }

        enc.setComputePipelineState(pipeline)
        enc.setBuffer(srcBuffer, offset: 0, index: 0)
        enc.setBuffer(dstBuffer, offset: 0, index: 1)
        enc.setBytes(&count, length: 4, index: 2)

        let w = pipeline.threadExecutionWidth
        let threadsPerGrid = MTLSize(width: pixelCount, height: 1, depth: 1)
        let threadsPerGroup = MTLSize(width: w, height: 1, depth: 1)

        enc.dispatchThreads(threadsPerGrid, threadsPerThreadgroup: threadsPerGroup)
        enc.endEncoding()

        cmd.commit()
        cmd.waitUntilCompleted()

        return Data(bytes: dstBuffer.contents(), count: dstSize)
    }
}
