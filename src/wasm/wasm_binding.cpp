#include <emscripten.h>
#include <vector>
#include "core.h"

// GLOBAL SCRATCH BUFFERS
// Reused across calls to avoid malloc/free overhead and fragmentation.
// 1. Font Data Buffer (Input)
std::vector<uint8_t> g_fontBuffer;
// 2. Pixel Data Buffer (Output)
std::vector<float> g_pixelBuffer;
// 3. Variation Axes Buffer (Input)
std::vector<msdf_core::VariationAxis> g_axesBuffer;

extern "C" {

    /**
     * Prepare the Font Buffer.
     * Ensures the internal C++ buffer is large enough to hold the font file.
     * 
     * @param size Required size in bytes
     * @return Pointer to the buffer. JS should copy font data here.
     */
    EMSCRIPTEN_KEEPALIVE
    uint8_t* prepare_font_buffer(int size) {
        if (g_fontBuffer.size() < size) {
            g_fontBuffer.resize(size);
        }
        return g_fontBuffer.data();
    }

    /**
     * Generate a single MSDF glyph (3 channels).
     */
    EMSCRIPTEN_KEEPALIVE
    float* generate_glyph(int fontLen, uint32_t charCode, double fontSize, double pixelRange, float* outMetrics) {
        
        // 1. Generate using Core logic
        msdf_core::GlyphResult res = msdf_core::generateOne(
            g_fontBuffer.data(), 
            fontLen, 
            charCode, 
            fontSize, 
            pixelRange
        );
        
        if (!res.success) {
            outMetrics[0] = 0.0f; // Success = false
            return nullptr;
        }

        // 2. Write Metrics
        outMetrics[0] = 1.0f; // Success
        outMetrics[1] = (float)res.width;
        outMetrics[2] = (float)res.height;
        outMetrics[3] = res.advance;
        outMetrics[4] = res.planeBounds[0]; // L
        outMetrics[5] = res.planeBounds[1]; // B
        outMetrics[6] = res.planeBounds[2]; // R
        outMetrics[7] = res.planeBounds[3]; // T
        outMetrics[8] = res.atlasBounds[0]; // Atlas L (always 0)
        outMetrics[9] = res.atlasBounds[1]; // Atlas B (always 0)
        
        // 3. Resize Output Buffer if needed
        size_t neededBytes = res.pixels.size() * sizeof(float);
        size_t neededFloats = res.pixels.size();
        
        if (g_pixelBuffer.size() < neededFloats) {
            g_pixelBuffer.resize(neededFloats);
        }
        
        // 4. Copy Pixels to Output Buffer
        std::memcpy(g_pixelBuffer.data(), res.pixels.data(), neededBytes);
        
        return g_pixelBuffer.data();
    }

    /**
     * Generate a single MTSDF glyph (4 channels).
     */
    EMSCRIPTEN_KEEPALIVE
    float* generate_mtsdf_glyph(int fontLen, uint32_t charCode, double fontSize, double pixelRange, float* outMetrics) {
        
        // 1. Generate using Core logic
        msdf_core::GlyphResult res = msdf_core::generateOneMTSDF(
            g_fontBuffer.data(), 
            fontLen, 
            charCode, 
            fontSize, 
            pixelRange
        );
        
        if (!res.success) {
            outMetrics[0] = 0.0f; // Success = false
            return nullptr;
        }

        // 2. Write Metrics
        outMetrics[0] = 1.0f; // Success
        outMetrics[1] = (float)res.width;
        outMetrics[2] = (float)res.height;
        outMetrics[3] = res.advance;
        outMetrics[4] = res.planeBounds[0]; // L
        outMetrics[5] = res.planeBounds[1]; // B
        outMetrics[6] = res.planeBounds[2]; // R
        outMetrics[7] = res.planeBounds[3]; // T
        outMetrics[8] = res.atlasBounds[0]; // Atlas L (always 0)
        outMetrics[9] = res.atlasBounds[1]; // Atlas B (always 0)
        
        // 3. Resize Output Buffer if needed
        size_t neededBytes = res.pixels.size() * sizeof(float);
        size_t neededFloats = res.pixels.size();
        
        if (g_pixelBuffer.size() < neededFloats) {
            g_pixelBuffer.resize(neededFloats);
        }
        
        // 4. Copy Pixels to Output Buffer
        std::memcpy(g_pixelBuffer.data(), res.pixels.data(), neededBytes);
        
        return g_pixelBuffer.data();
    }

    /**
     * Clear variation axes buffer.
     */
    EMSCRIPTEN_KEEPALIVE
    void clear_variation_axes() {
        g_axesBuffer.clear();
    }

    /**
     * Add a variation axis.
     * @param tag 4-letter axis tag (e.g., "wght", "opsz")
     * @param value Axis value (e.g., 700.0 for weight)
     */
    EMSCRIPTEN_KEEPALIVE
    void add_variation_axis(const char* tag, double value) {
        msdf_core::VariationAxis axis;
        std::strncpy(axis.tag, tag, 4);
        axis.tag[4] = '\0';
        axis.value = value;
        g_axesBuffer.push_back(axis);
    }

    /**
     * Generate MSDF glyph with current variation axes (3 channels).
     */
    EMSCRIPTEN_KEEPALIVE
    float* generate_glyph_var(int fontLen, uint32_t charCode, double fontSize, double pixelRange, float* outMetrics) {
        msdf_core::GlyphResult res = msdf_core::generateOneVar(
            g_fontBuffer.data(),
            fontLen,
            charCode,
            fontSize,
            pixelRange,
            g_axesBuffer.data(),
            (int)g_axesBuffer.size()
        );

        if (!res.success) {
            outMetrics[0] = 0.0f;
            return nullptr;
        }

        outMetrics[0] = 1.0f;
        outMetrics[1] = (float)res.width;
        outMetrics[2] = (float)res.height;
        outMetrics[3] = res.advance;
        outMetrics[4] = res.planeBounds[0];
        outMetrics[5] = res.planeBounds[1];
        outMetrics[6] = res.planeBounds[2];
        outMetrics[7] = res.planeBounds[3];
        outMetrics[8] = res.atlasBounds[0];
        outMetrics[9] = res.atlasBounds[1];

        size_t neededFloats = res.pixels.size();
        if (g_pixelBuffer.size() < neededFloats) {
            g_pixelBuffer.resize(neededFloats);
        }
        std::memcpy(g_pixelBuffer.data(), res.pixels.data(), neededFloats * sizeof(float));

        return g_pixelBuffer.data();
    }

    /**
     * Generate MTSDF glyph with current variation axes (4 channels).
     */
    EMSCRIPTEN_KEEPALIVE
    float* generate_mtsdf_glyph_var(int fontLen, uint32_t charCode, double fontSize, double pixelRange, float* outMetrics) {
        msdf_core::GlyphResult res = msdf_core::generateOneMTSDFVar(
            g_fontBuffer.data(),
            fontLen,
            charCode,
            fontSize,
            pixelRange,
            g_axesBuffer.data(),
            (int)g_axesBuffer.size()
        );

        if (!res.success) {
            outMetrics[0] = 0.0f;
            return nullptr;
        }

        outMetrics[0] = 1.0f;
        outMetrics[1] = (float)res.width;
        outMetrics[2] = (float)res.height;
        outMetrics[3] = res.advance;
        outMetrics[4] = res.planeBounds[0];
        outMetrics[5] = res.planeBounds[1];
        outMetrics[6] = res.planeBounds[2];
        outMetrics[7] = res.planeBounds[3];
        outMetrics[8] = res.atlasBounds[0];
        outMetrics[9] = res.atlasBounds[1];

        size_t neededFloats = res.pixels.size();
        if (g_pixelBuffer.size() < neededFloats) {
            g_pixelBuffer.resize(neededFloats);
        }
        std::memcpy(g_pixelBuffer.data(), res.pixels.data(), neededFloats * sizeof(float));

        return g_pixelBuffer.data();
    }

    /**
     * Check if a glyph exists in the font (without generating it).
     * @return 1 if glyph exists, 0 if not
     */
    EMSCRIPTEN_KEEPALIVE
    int has_glyph(int fontLen, uint32_t charCode) {
        return msdf_core::hasGlyph(g_fontBuffer.data(), fontLen, charCode) ? 1 : 0;
    }

    /**
     * Free memory logic.
     * Call this when done with a batch processing job to release heap memory.
     */
    EMSCRIPTEN_KEEPALIVE
    void free_buffers() {
        // Force deallocation
        std::vector<uint8_t>().swap(g_fontBuffer);
        std::vector<float>().swap(g_pixelBuffer);
        std::vector<msdf_core::VariationAxis>().swap(g_axesBuffer);
    }

}