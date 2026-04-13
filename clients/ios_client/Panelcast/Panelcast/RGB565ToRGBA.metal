#include <metal_stdlib>
using namespace metal;

kernel void rgb565_to_rgba8(
    const device uint16_t* src [[ buffer(0) ]],
    device uchar4* dst [[ buffer(1) ]],
    constant uint& pixelCount [[ buffer(2) ]],
    uint gid [[ thread_position_in_grid ]]
) {
    if (gid >= pixelCount) return;

    uint16_t px = src[gid];

    uint r5 = (px >> 11) & 0x1F;
    uint g6 = (px >> 5)  & 0x3F;
    uint b5 =  px        & 0x1F;

    uint r8 = (r5 * 527 + 23) >> 6;
    uint g8 = (g6 * 259 + 33) >> 6;
    uint b8 = (b5 * 527 + 23) >> 6;

    dst[gid] = uchar4(r8, g8, b8, 255);
}
