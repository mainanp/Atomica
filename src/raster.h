// raster.h — a tiny CPU "framebuffer" with hand-written rasterisation algorithms.
// Stages 1-3 draw ONLY through this class (no GL line/circle calls), so every pixel
// is produced by an algorithm we can explain. OpenGL is used just to show the result.
#pragma once
#include <cstdint>
#include <vector>

struct Color {
    uint8_t r = 0, g = 0, b = 0, a = 255;
};

class Raster {
public:
    Raster(int w = 1, int h = 1) { resize(w, h); }

    void resize(int w, int h);
    void clear(Color c);

    int width() const { return w_; }
    int height() const { return h_; }
    const uint8_t* data() const { return px_.data(); }

    // Single pixel, source-over alpha blending.
    void plot(int x, int y, Color c);
    // Single pixel, additive blending (saturating) — used for glows.
    void plotAdd(int x, int y, Color c, float intensity);

    // Bresenham line (integer error term, all octants).
    void line(int x0, int y0, int x1, int y1, Color c);
    // Midpoint circle (outline, 8-way symmetry).
    void circle(int cx, int cy, int r, Color c);
    // Filled disc built from the same midpoint walk, using horizontal spans.
    void fillCircle(int cx, int cy, int r, Color c);
    // Soft radial glow: intensity falls off with distance^2 (additive).
    void glow(int cx, int cy, int radius, Color c);

private:
    void hspan(int x0, int x1, int y, Color c);
    int w_ = 0, h_ = 0;
    std::vector<uint8_t> px_;  // RGBA8
};

// Convert wavelength (nm) to a displayable colour. Outside 380-780 nm returns a
// tinted stand-in (UV = violet-white, IR = deep red) so every photon is visible.
Color wavelengthToColor(double nm);
