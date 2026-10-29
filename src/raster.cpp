#include "raster.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>

void Raster::resize(int w, int h) {
    w_ = std::max(1, w);
    h_ = std::max(1, h);
    px_.assign(static_cast<size_t>(w_) * h_ * 4, 0);
}

void Raster::clear(Color c) {
    for (size_t i = 0; i < px_.size(); i += 4) {
        px_[i] = c.r; px_[i + 1] = c.g; px_[i + 2] = c.b; px_[i + 3] = 255;
    }
}

void Raster::plot(int x, int y, Color c) {
    if (x < 0 || y < 0 || x >= w_ || y >= h_) return;
    uint8_t* p = &px_[(static_cast<size_t>(y) * w_ + x) * 4];
    if (c.a == 255) {
        p[0] = c.r; p[1] = c.g; p[2] = c.b;
    } else {
        int a = c.a;
        p[0] = static_cast<uint8_t>((c.r * a + p[0] * (255 - a)) / 255);
        p[1] = static_cast<uint8_t>((c.g * a + p[1] * (255 - a)) / 255);
        p[2] = static_cast<uint8_t>((c.b * a + p[2] * (255 - a)) / 255);
    }
    p[3] = 255;
}

void Raster::plotAdd(int x, int y, Color c, float k) {
    if (x < 0 || y < 0 || x >= w_ || y >= h_) return;
    uint8_t* p = &px_[(static_cast<size_t>(y) * w_ + x) * 4];
    p[0] = static_cast<uint8_t>(std::min(255.0f, p[0] + c.r * k));
    p[1] = static_cast<uint8_t>(std::min(255.0f, p[1] + c.g * k));
    p[2] = static_cast<uint8_t>(std::min(255.0f, p[2] + c.b * k));
}

// Bresenham: step along the major axis, keep an integer error term that says how
// far the ideal line has drifted from the pixel centre on the minor axis.
void Raster::line(int x0, int y0, int x1, int y1, Color c) {
    int dx = std::abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -std::abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    for (;;) {
        plot(x0, y0, c);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

// Midpoint circle: walk one octant, decision variable d = f(x-1/2, y+1) of
// x^2 + y^2 - r^2; mirror each point into the other 7 octants.
void Raster::circle(int cx, int cy, int r, Color c) {
    if (r <= 0) { plot(cx, cy, c); return; }
    int x = r, y = 0, d = 1 - r;
    while (x >= y) {
        plot(cx + x, cy + y, c); plot(cx - x, cy + y, c);
        plot(cx + x, cy - y, c); plot(cx - x, cy - y, c);
        plot(cx + y, cy + x, c); plot(cx - y, cy + x, c);
        plot(cx + y, cy - x, c); plot(cx - y, cy - x, c);
        ++y;
        if (d <= 0) {
            d += 2 * y + 1;
        } else {
            --x;
            d += 2 * (y - x) + 1;
        }
    }
}

void Raster::hspan(int x0, int x1, int y, Color c) {
    if (y < 0 || y >= h_) return;
    if (x0 > x1) std::swap(x0, x1);
    x0 = std::max(x0, 0);
    x1 = std::min(x1, w_ - 1);
    for (int x = x0; x <= x1; ++x) plot(x, y, c);
}

// Same midpoint walk as circle(), but instead of 8 dots we fill horizontal spans.
// Overlapping spans are harmless because fill colours are opaque.
void Raster::fillCircle(int cx, int cy, int r, Color c) {
    if (r <= 0) { plot(cx, cy, c); return; }
    int x = r, y = 0, d = 1 - r;
    while (x >= y) {
        hspan(cx - x, cx + x, cy + y, c);
        hspan(cx - x, cx + x, cy - y, c);
        hspan(cx - y, cx + y, cy + x, c);
        hspan(cx - y, cx + y, cy - x, c);
        ++y;
        if (d <= 0) {
            d += 2 * y + 1;
        } else {
            --x;
            d += 2 * (y - x) + 1;
        }
    }
}

void Raster::glow(int cx, int cy, int radius, Color c) {
    const float inv = 1.0f / (static_cast<float>(radius) * radius);
    for (int dy = -radius; dy <= radius; ++dy)
        for (int dx = -radius; dx <= radius; ++dx) {
            float t = 1.0f - (dx * dx + dy * dy) * inv;
            if (t > 0.0f) plotAdd(cx + dx, cy + dy, c, t * t);
        }
}

// Piecewise-linear approximation of the visible spectrum (Dan Bruton's classic fit).
Color wavelengthToColor(double nm) {
    if (nm < 380.0) return Color{170, 130, 255, 255};   // ultraviolet stand-in
    if (nm > 780.0) return Color{200, 40, 40, 255};     // infrared stand-in
    double r = 0, g = 0, b = 0;
    if (nm < 440)      { r = -(nm - 440) / 60.0; b = 1.0; }
    else if (nm < 490) { g = (nm - 440) / 50.0;  b = 1.0; }
    else if (nm < 510) { g = 1.0; b = -(nm - 510) / 20.0; }
    else if (nm < 580) { r = (nm - 510) / 70.0;  g = 1.0; }
    else if (nm < 645) { r = 1.0; g = -(nm - 645) / 65.0; }
    else               { r = 1.0; }
    double f = 1.0;                                     // dim the spectrum edges
    if (nm < 420)      f = 0.3 + 0.7 * (nm - 380) / 40.0;
    else if (nm > 700) f = 0.3 + 0.7 * (780 - nm) / 80.0;
    auto to8 = [&](double v) { return static_cast<uint8_t>(std::lround(255.0 * std::pow(v * f, 0.8))); };
    return Color{to8(r), to8(g), to8(b), 255};
}
