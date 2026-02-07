#pragma once

#include <vector>
#include <cstring>
#include "msdfgen.h"
#include "msdfgen-ext.h"

// Minimal Core: No atlas packing, no PNG saving. Just Math.

namespace msdf_core {

    // Variation axis: 4-letter tag + value
    struct VariationAxis {
        char tag[5];    // 4-letter tag + null terminator (e.g., "wght")
        double value;   // axis value (e.g., 700.0)
    };

    // Map 4-letter tag to full name for standard axes
    // Returns nullptr if tag is not a known standard axis
    inline const char* tagToName(const char* tag) {
        if (std::strcmp(tag, "wght") == 0) return "Weight";
        if (std::strcmp(tag, "wdth") == 0) return "Width";
        if (std::strcmp(tag, "opsz") == 0) return "Optical Size";
        if (std::strcmp(tag, "ital") == 0) return "Italic";
        if (std::strcmp(tag, "slnt") == 0) return "Slant";
        // TODO: For custom axes, we'd need to query the font's name table
        return nullptr;
    }

    // Apply variation axes to a loaded font
    // Returns number of axes successfully set
    inline int applyVariationAxes(msdfgen::FreetypeHandle* ft, msdfgen::FontHandle* font,
                                   const VariationAxis* axes, int numAxes) {
        int successCount = 0;
        for (int i = 0; i < numAxes; ++i) {
            const char* name = tagToName(axes[i].tag);
            if (name) {
                if (msdfgen::setFontVariationAxis(ft, font, name, axes[i].value)) {
                    successCount++;
                }
            }
        }
        return successCount;
    }

    /**
     * Check if a glyph exists in the font (without generating it).
     * Returns true if the font contains a glyph for this codepoint.
     * Uses getGlyphIndex which returns 0 for missing glyphs.
     */
    inline bool hasGlyph(const uint8_t* fontBytes, int length, uint32_t charCode) {
        msdfgen::FreetypeHandle* ft = msdfgen::initializeFreetype();
        if (!ft) return false;

        msdfgen::FontHandle* font = msdfgen::loadFontData(ft, (msdfgen::byte*)fontBytes, length);
        if (!font) {
            msdfgen::deinitializeFreetype(ft);
            return false;
        }

        // Get glyph index - returns 0 if glyph not in font
        msdfgen::GlyphIndex glyphIndex;
        bool exists = msdfgen::getGlyphIndex(glyphIndex, font, charCode) && glyphIndex.getIndex() != 0;

        msdfgen::destroyFont(font);
        msdfgen::deinitializeFreetype(ft);
        return exists;
    }

    /**
     * Result of a single glyph generation.
     * Contains both metric data (for layout) and raw pixel data (for rendering).
     */
    struct GlyphResult {
        bool success;           // True if generation succeeded
        int width;              // Width of the bitmap in pixels
        int height;             // Height of the bitmap in pixels
        int channels;           // Number of channels (3 for MSDF, 4 for MTSDF)
        float advance;          // Horizontal advance (scaled to pixels)
        float planeBounds[4];   // Physical glyph bounds (L, B, R, T) relative to baseline
        float atlasBounds[4];   // Texture coordinates (L, B, R, T) - typically 0,0,w,h for single glyph
        std::vector<float> pixels; // Raw float data (channels * width * height)
    };

    /**
     * Core function to generate a single MSDF glyph (3 channels).
     */
    inline GlyphResult generateOne(const uint8_t* fontBytes, int length, uint32_t charCode, double fontSize, double pixelRange) {
        GlyphResult result;
        result.success = false;
        result.channels = 3;

        // 1. Initialize FreeType
        msdfgen::FreetypeHandle* ft = msdfgen::initializeFreetype();
        if (!ft) return result;

        // 2. Load Font
        msdfgen::FontHandle* font = msdfgen::loadFontData(ft, (msdfgen::byte*)fontBytes, length);
        if (!font) {
            msdfgen::deinitializeFreetype(ft);
            return result;
        }

        // 3. Load Shape
        msdfgen::Shape shape;
        double advance;
        if (!msdfgen::loadGlyph(shape, font, charCode, &advance)) {
            msdfgen::destroyFont(font);
            msdfgen::deinitializeFreetype(ft);
            return result;
        }

        // 4. Edge Coloring
        shape.normalize();
        msdfgen::edgeColoringSimple(shape, 3.0);

        // 5. Bounds & Dimensions
        double l = 1e240, b = 1e240, r = -1e240, t = -1e240;
        shape.bound(l, b, r, t);

        // Handle empty shapes (e.g., space character)
        if (l >= r || b >= t) {
            l = b = 0;
            r = t = 1;
        }

        msdfgen::FontMetrics metrics;
        msdfgen::getFontMetrics(metrics, font);
        double scale = fontSize / metrics.emSize;
        
        double range = pixelRange / 2.0; 
        double frameL = l * scale - range;
        double frameB = b * scale - range;
        double frameR = r * scale + range;
        double frameT = t * scale + range;
        
        int width = (int)ceil(frameR - frameL);
        int height = (int)ceil(frameT - frameB);
        
        double tx = -frameL / scale;
        double ty = -frameB / scale;
        
        msdfgen::Vector2 translate(tx, ty);
        msdfgen::Vector2 scaling(scale, scale);

        // 6. Generate Bitmap (MSDF)
        msdfgen::Bitmap<float, 3> msdf(width, height);
        msdfgen::generateMSDF(msdf, shape, msdfgen::Projection(scaling, translate), pixelRange);

        // 7. Pack Results
        result.success = true;
        result.width = width;
        result.height = height;
        result.advance = (float)(advance * scale);
        
        result.planeBounds[0] = (float)(l * scale);
        result.planeBounds[1] = (float)(b * scale);
        result.planeBounds[2] = (float)(r * scale);
        result.planeBounds[3] = (float)(t * scale);
        
        result.atlasBounds[0] = 0;
        result.atlasBounds[1] = 0;
        result.atlasBounds[2] = (float)width;
        result.atlasBounds[3] = (float)height;

        // Copy pixels (3 channels)
        result.pixels.resize(width * height * 3);
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                const float* pixel = msdf(x, y);
                int idx = (y * width + x) * 3;
                result.pixels[idx + 0] = pixel[0];
                result.pixels[idx + 1] = pixel[1];
                result.pixels[idx + 2] = pixel[2];
            }
        }

        msdfgen::destroyFont(font);
        msdfgen::deinitializeFreetype(ft);
        return result;
    }

    /**
     * Core function to generate a single MTSDF glyph (4 channels).
     */
    inline GlyphResult generateOneMTSDF(const uint8_t* fontBytes, int length, uint32_t charCode, double fontSize, double pixelRange) {
        GlyphResult result;
        result.success = false;
        result.channels = 4;

        msdfgen::FreetypeHandle* ft = msdfgen::initializeFreetype();
        if (!ft) return result;

        msdfgen::FontHandle* font = msdfgen::loadFontData(ft, (msdfgen::byte*)fontBytes, length);
        if (!font) {
            msdfgen::deinitializeFreetype(ft);
            return result;
        }

        msdfgen::Shape shape;
        double advance;
        if (!msdfgen::loadGlyph(shape, font, charCode, &advance)) {
            msdfgen::destroyFont(font);
            msdfgen::deinitializeFreetype(ft);
            return result;
        }

        shape.normalize();
        msdfgen::edgeColoringSimple(shape, 3.0);

        double l = 1e240, b = 1e240, r = -1e240, t = -1e240;
        shape.bound(l, b, r, t);

        // Handle empty shapes (e.g., space character)
        if (l >= r || b >= t) {
            l = b = 0;
            r = t = 1;
        }

        msdfgen::FontMetrics metrics;
        msdfgen::getFontMetrics(metrics, font);
        double scale = fontSize / metrics.emSize;

        double range = pixelRange / 2.0;
        double frameL = l * scale - range;
        double frameB = b * scale - range;
        double frameR = r * scale + range;
        double frameT = t * scale + range;

        int width = (int)ceil(frameR - frameL);
        int height = (int)ceil(frameT - frameB);

        double tx = -frameL / scale;
        double ty = -frameB / scale;

        msdfgen::Vector2 translate(tx, ty);
        msdfgen::Vector2 scaling(scale, scale);

        // 6. Generate Bitmap (MTSDF)
        msdfgen::Bitmap<float, 4> msdf(width, height);
        msdfgen::generateMTSDF(msdf, shape, msdfgen::Projection(scaling, translate), pixelRange);

        result.success = true;
        result.width = width;
        result.height = height;
        result.advance = (float)(advance * scale);
        
        result.planeBounds[0] = (float)(l * scale);
        result.planeBounds[1] = (float)(b * scale);
        result.planeBounds[2] = (float)(r * scale);
        result.planeBounds[3] = (float)(t * scale);
        
        result.atlasBounds[0] = 0;
        result.atlasBounds[1] = 0;
        result.atlasBounds[2] = (float)width;
        result.atlasBounds[3] = (float)height;

        // Copy pixels (4 channels)
        result.pixels.resize(width * height * 4);
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                const float* pixel = msdf(x, y);
                int idx = (y * width + x) * 4;
                result.pixels[idx + 0] = pixel[0];
                result.pixels[idx + 1] = pixel[1];
                result.pixels[idx + 2] = pixel[2];
                result.pixels[idx + 3] = pixel[3];
            }
        }

        msdfgen::destroyFont(font);
        msdfgen::deinitializeFreetype(ft);
        return result;
    }

    /**
     * Generate MSDF glyph with variation axes (3 channels).
     */
    inline GlyphResult generateOneVar(const uint8_t* fontBytes, int length, uint32_t charCode,
                                       double fontSize, double pixelRange,
                                       const VariationAxis* axes, int numAxes) {
        GlyphResult result;
        result.success = false;
        result.channels = 3;

        msdfgen::FreetypeHandle* ft = msdfgen::initializeFreetype();
        if (!ft) return result;

        msdfgen::FontHandle* font = msdfgen::loadFontData(ft, (msdfgen::byte*)fontBytes, length);
        if (!font) {
            msdfgen::deinitializeFreetype(ft);
            return result;
        }

        // Apply variation axes before loading glyph
        if (numAxes > 0) {
            applyVariationAxes(ft, font, axes, numAxes);
        }

        msdfgen::Shape shape;
        double advance;
        if (!msdfgen::loadGlyph(shape, font, charCode, &advance)) {
            msdfgen::destroyFont(font);
            msdfgen::deinitializeFreetype(ft);
            return result;
        }

        shape.normalize();
        msdfgen::edgeColoringSimple(shape, 3.0);

        double l = 1e240, b = 1e240, r = -1e240, t = -1e240;
        shape.bound(l, b, r, t);

        // Handle empty shapes (e.g., space character)
        if (l >= r || b >= t) {
            l = b = 0;
            r = t = 1;
        }

        msdfgen::FontMetrics metrics;
        msdfgen::getFontMetrics(metrics, font);
        double scale = fontSize / metrics.emSize;

        double range = pixelRange / 2.0;
        double frameL = l * scale - range;
        double frameB = b * scale - range;
        double frameR = r * scale + range;
        double frameT = t * scale + range;

        int width = (int)ceil(frameR - frameL);
        int height = (int)ceil(frameT - frameB);

        double tx = -frameL / scale;
        double ty = -frameB / scale;

        msdfgen::Vector2 translate(tx, ty);
        msdfgen::Vector2 scaling(scale, scale);

        msdfgen::Bitmap<float, 3> msdf(width, height);
        msdfgen::generateMSDF(msdf, shape, msdfgen::Projection(scaling, translate), pixelRange);

        result.success = true;
        result.width = width;
        result.height = height;
        result.advance = (float)(advance * scale);

        result.planeBounds[0] = (float)(l * scale);
        result.planeBounds[1] = (float)(b * scale);
        result.planeBounds[2] = (float)(r * scale);
        result.planeBounds[3] = (float)(t * scale);

        result.atlasBounds[0] = 0;
        result.atlasBounds[1] = 0;
        result.atlasBounds[2] = (float)width;
        result.atlasBounds[3] = (float)height;

        result.pixels.resize(width * height * 3);
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                const float* pixel = msdf(x, y);
                int idx = (y * width + x) * 3;
                result.pixels[idx + 0] = pixel[0];
                result.pixels[idx + 1] = pixel[1];
                result.pixels[idx + 2] = pixel[2];
            }
        }

        msdfgen::destroyFont(font);
        msdfgen::deinitializeFreetype(ft);
        return result;
    }

    /**
     * Generate MTSDF glyph with variation axes (4 channels).
     */
    inline GlyphResult generateOneMTSDFVar(const uint8_t* fontBytes, int length, uint32_t charCode,
                                            double fontSize, double pixelRange,
                                            const VariationAxis* axes, int numAxes) {
        GlyphResult result;
        result.success = false;
        result.channels = 4;

        msdfgen::FreetypeHandle* ft = msdfgen::initializeFreetype();
        if (!ft) return result;

        msdfgen::FontHandle* font = msdfgen::loadFontData(ft, (msdfgen::byte*)fontBytes, length);
        if (!font) {
            msdfgen::deinitializeFreetype(ft);
            return result;
        }

        // Apply variation axes before loading glyph
        if (numAxes > 0) {
            applyVariationAxes(ft, font, axes, numAxes);
        }

        msdfgen::Shape shape;
        double advance;
        if (!msdfgen::loadGlyph(shape, font, charCode, &advance)) {
            msdfgen::destroyFont(font);
            msdfgen::deinitializeFreetype(ft);
            return result;
        }

        shape.normalize();
        msdfgen::edgeColoringSimple(shape, 3.0);

        double l = 1e240, b = 1e240, r = -1e240, t = -1e240;
        shape.bound(l, b, r, t);

        // Handle empty shapes (e.g., space character)
        if (l >= r || b >= t) {
            l = b = 0;
            r = t = 1;
        }

        msdfgen::FontMetrics metrics;
        msdfgen::getFontMetrics(metrics, font);
        double scale = fontSize / metrics.emSize;

        double range = pixelRange / 2.0;
        double frameL = l * scale - range;
        double frameB = b * scale - range;
        double frameR = r * scale + range;
        double frameT = t * scale + range;

        int width = (int)ceil(frameR - frameL);
        int height = (int)ceil(frameT - frameB);

        double tx = -frameL / scale;
        double ty = -frameB / scale;

        msdfgen::Vector2 translate(tx, ty);
        msdfgen::Vector2 scaling(scale, scale);

        msdfgen::Bitmap<float, 4> msdf(width, height);
        msdfgen::generateMTSDF(msdf, shape, msdfgen::Projection(scaling, translate), pixelRange);

        result.success = true;
        result.width = width;
        result.height = height;
        result.advance = (float)(advance * scale);

        result.planeBounds[0] = (float)(l * scale);
        result.planeBounds[1] = (float)(b * scale);
        result.planeBounds[2] = (float)(r * scale);
        result.planeBounds[3] = (float)(t * scale);

        result.atlasBounds[0] = 0;
        result.atlasBounds[1] = 0;
        result.atlasBounds[2] = (float)width;
        result.atlasBounds[3] = (float)height;

        result.pixels.resize(width * height * 4);
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                const float* pixel = msdf(x, y);
                int idx = (y * width + x) * 4;
                result.pixels[idx + 0] = pixel[0];
                result.pixels[idx + 1] = pixel[1];
                result.pixels[idx + 2] = pixel[2];
                result.pixels[idx + 3] = pixel[3];
            }
        }

        msdfgen::destroyFont(font);
        msdfgen::deinitializeFreetype(ft);
        return result;
    }
}
