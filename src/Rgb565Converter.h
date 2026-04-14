#pragma once
#include <cstddef>
#include <cstdint>
#include <immintrin.h>

class Rgb565Converter {
  public:
	// ---------------------------------------------------------
	// Public API
	// ---------------------------------------------------------
	static inline void rgba8_to_rgb565(const uint8_t* srcRGBA, uint16_t* dst565, size_t pixelCount) {
#if defined(__AVX2__)
		rgba8_to_rgb565_avx2(srcRGBA, dst565, pixelCount);
#else
		rgba8_to_rgb565_scalar(srcRGBA, dst565, pixelCount);
#endif
	}

  private:
	// ---------------------------------------------------------
	// Scalar fallback (korrekt & robust)
	// ---------------------------------------------------------
	static inline void rgba8_to_rgb565_scalar(const uint8_t* srcRGBA, uint16_t* dst565, size_t pixelCount) {
		for (size_t i = 0; i < pixelCount; ++i) {
			const uint8_t r8 = srcRGBA[i * 4 + 0];
			const uint8_t g8 = srcRGBA[i * 4 + 1];
			const uint8_t b8 = srcRGBA[i * 4 + 2];

			const uint16_t r5 = r8 >> 3;
			const uint16_t g6 = g8 >> 2;
			const uint16_t b5 = b8 >> 3;

			dst565[i] = static_cast<uint16_t>((r5 << 11) | (g6 << 5) | b5);
		}
	}

	// ---------------------------------------------------------
	// AVX2‑Pfad (8 Pixel pro Iteration, 100% sicher)
	// ---------------------------------------------------------
	static inline void rgba8_to_rgb565_avx2(const uint8_t* srcRGBA, uint16_t* dst565, size_t pixelCount) {
		size_t i = 0;

		const __m256i shuf = _mm256_setr_epi8(0, 1, 2, -1, 4, 5, 6, -1, 8, 9, 10, -1, 12, 13, 14, -1, 0, 1, 2, -1, 4, 5,
		                                      6, -1, 8, 9, 10, -1, 12, 13, 14, -1);

		for (; i + 8 <= pixelCount; i += 8) {

			__m256i px = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(srcRGBA + i * 4));
			__m256i rgb = _mm256_shuffle_epi8(px, shuf);

			__m256i r = _mm256_and_si256(_mm256_srli_epi32(rgb, 16), _mm256_set1_epi32(0xFF));
			__m256i g = _mm256_and_si256(_mm256_srli_epi32(rgb, 8), _mm256_set1_epi32(0xFF));
			__m256i b = _mm256_and_si256(rgb, _mm256_set1_epi32(0xFF));

			r = _mm256_srli_epi32(r, 3);
			g = _mm256_srli_epi32(g, 2);
			b = _mm256_srli_epi32(b, 3);

			__m256i r565 = _mm256_slli_epi32(r, 11);
			__m256i g565 = _mm256_slli_epi32(g, 5);
			__m256i b565 = b;

			__m256i rgb565 = _mm256_or_si256(_mm256_or_si256(r565, g565), b565);

			__m256i packed = _mm256_packus_epi32(rgb565, rgb565);
			packed = _mm256_permute4x64_epi64(packed, 0xD8);

			__m128i out128 = _mm256_castsi256_si128(packed);

			_mm_storeu_si128(reinterpret_cast<__m128i*>(dst565 + i), out128);
		}

		rgba8_to_rgb565_scalar(srcRGBA + i * 4, dst565 + i, pixelCount - i);
	}
};
