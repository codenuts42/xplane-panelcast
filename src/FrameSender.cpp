/**
 * @file FrameSender.cpp
 * @brief Implementation of the background worker for LZ4 compression and UDP transmission.
 *
 * The worker thread periodically consumes raw framebuffer captures,
 * compresses them using LZ4, and sends them via UdpSender.
 *
 * (c) 2025 Peter Vorwieger — All rights reserved.
 */

#include "FrameSender.h"
#include "PanelFrameHeader.h"
#include <chrono>
#include <immintrin.h>
#include <lz4.h>

FrameSender::FrameSender(FrameTransport& transport) : transport_(transport) {
}

FrameSender::~FrameSender() {
	stop();
}

void FrameSender::start() {
	running_.store(true);
	workerThread_ = std::thread(&FrameSender::workerLoop, this);
}

void FrameSender::stop() {
	running_.store(false);
	if (workerThread_.joinable()) workerThread_.join();
}

/**
 * @brief Main worker loop.
 *
 * Pulls all pending frames from the shared buffer (swap-based, non-blocking),
 * compresses each frame, and transmits it via UDP.
 */
void FrameSender::workerLoop() {
	while (running_.load()) {

		// Local buffer to avoid holding the mutex during compression
		std::unordered_map<uint16_t, RawPanelFrame> local;

		{
			// Swap out all pending frames in one atomic operation
			std::lock_guard<std::mutex> lock(frameMutex_);
			std::swap(local, frames_);
		}

		// Process all captured frames
		for (auto& [panelID, frame] : local)
			compressAndSendPanel(frame);

		// Prevent busy-looping
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}
}

static void rgba8_to_rgb565_avx2(const char* srcRGBA, uint16_t* dst565, size_t pixelCount) {
	const uint8_t* src = reinterpret_cast<const uint8_t*>(srcRGBA);
	size_t i = 0;

	for (; i + 8 <= pixelCount; i += 8) {
		// 8 Pixel = 32 Bytes
		__m256i px = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(src + i * 4));

		const __m256i shuffle = _mm256_setr_epi8(                   //
		    0, 1, 2, -1, 4, 5, 6, -1, 8, 9, 10, -1, 12, 13, 14, -1, //
		    0, 1, 2, -1, 4, 5, 6, -1, 8, 9, 10, -1, 12, 13, 14, -1  //
		);
		__m256i rgb = _mm256_shuffle_epi8(px, shuffle);

		__m256i r = _mm256_and_si256(_mm256_srli_epi32(rgb, 16), _mm256_set1_epi32(0xFF));
		__m256i g = _mm256_and_si256(_mm256_srli_epi32(rgb, 8), _mm256_set1_epi32(0xFF));
		__m256i b = _mm256_and_si256(rgb, _mm256_set1_epi32(0xFF));

		r = _mm256_srli_epi32(r, 3); // 8 → 5 Bit
		g = _mm256_srli_epi32(g, 2); // 8 → 6 Bit
		b = _mm256_srli_epi32(b, 3); // 8 → 5 Bit

		__m256i r565 = _mm256_slli_epi32(r, 11);
		__m256i g565 = _mm256_slli_epi32(g, 5);
		__m256i b565 = b;

		__m256i rgb565 = _mm256_or_si256(_mm256_or_si256(r565, g565), b565);

		__m256i packed = _mm256_packus_epi32(rgb565, rgb565);
		packed = _mm256_permute4x64_epi64(packed, 0xD8);

		_mm256_storeu_si256(reinterpret_cast<__m256i*>(dst565 + i), packed);
	}

	for (; i < pixelCount; ++i) {
		const uint8_t r = src[i * 4 + 0];
		const uint8_t g = src[i * 4 + 1];
		const uint8_t b = src[i * 4 + 2];

		dst565[i] = static_cast<uint16_t>(((r >> 3) << 11) | ((g >> 2) << 5) | ((b >> 3)));
	}
}

/**
 * @brief Compresses a single panel frame and sends it via UDP.
 *
 * Uses LZ4_fast for high-speed compression. Frames are fragmented into MTU-sized
 * UDP packets by UdpSender.
 */
void FrameSender::compressAndSendPanel(const RawPanelFrame& f) {
	const int pixelCount = f.width * f.height;

	// 1) RGBA8 → RGB565 im Sender‑Thread
	std::vector<uint16_t> rgb565(pixelCount);
	rgba8_to_rgb565_avx2(f.pixels.data(), rgb565.data(), pixelCount);

	// 2) LZ4 auf RGB565 (2 Bytes/Pixel)
	const int rawSize = pixelCount * static_cast<int>(sizeof(uint16_t));

	const int maxComp = LZ4_compressBound(rawSize);
	std::vector<char> comp(maxComp);

	const int compSize =
	    LZ4_compress_fast(reinterpret_cast<const char*>(rgb565.data()), comp.data(), rawSize, maxComp, 8);
	if (compSize <= 0) return;
	comp.resize(compSize);

	uint32_t& counter = frameCounters_[f.panelID];
	const uint32_t frameID = counter++;

	transport_.sendFrame(f.panelID, frameID, comp.data(), compSize, f.width, f.height);
}
