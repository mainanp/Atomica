#include "stages2d.h"

#include <GLFW/glfw3.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <deque>
#include <vector>

namespace {

constexpr double kPi = 3.14159265358979323846;
const Color kBg{8, 10, 18, 255};
const Color kNucleus{255, 90, 70, 255};
const Color kElectron{90, 190, 255, 255};
const Color kDim{45, 55, 80, 255};

inline int iround(double v) { return static_cast<int>(std::lround(v)); }

// ---------------------------------------------------------------------------------------
// Stage 1 — "planetary" atom. Electron feels Coulomb attraction; because an accelerating
// charge radiates (Larmor), it loses energy and spirals into the nucleus. This is WHY the
// classical picture fails, and why we need Bohr's levels next.
// Units: nucleus at origin, k = 1, start radius = 1 (drawn at kR0 pixels).
// ---------------------------------------------------------------------------------------
class PlanetaryStage : public Stage2D {
public:
    PlanetaryStage() { reset(); }

    void update(double dt, int w, int h) override {
        w_ = w; h_ = h;
        if (paused_) return;
        if (collapsed_) {
            collapseTimer_ -= dt;
            if (collapseTimer_ <= 0) reset();
            return;
        }
        double simTime = dt * kSimSpeed;
        int steps = 0;
        while (simTime > 0 && steps < 20000) {
            double r = std::hypot(x_, y_);
            double h = std::min(simTime, 0.002 * std::min(1.0, r * std::sqrt(r)));  // smaller step near nucleus
            step(h);
            simTime -= h;
            ++steps;
            if (std::hypot(x_, y_) < 0.04) { collapsed_ = true; collapseTimer_ = 2.5; break; }
        }
        trail_.push_back({x_, y_});
        if (trail_.size() > 500) trail_.pop_front();
    }

    void draw(Raster& r) override {
        r.clear(kBg);
        int cx = w_ / 2, cy = h_ / 2;
        double R0 = 0.36 * std::min(w_, h_);
        // trail: oldest -> newest, brightness ramps up (Bresenham segments)
        for (size_t i = 1; i < trail_.size(); ++i) {
            float t = static_cast<float>(i) / trail_.size();
            Color c{static_cast<uint8_t>(40 + 90 * t), static_cast<uint8_t>(80 + 120 * t), static_cast<uint8_t>(120 + 135 * t), 255};
            r.line(cx + iround(trail_[i - 1].x * R0), cy - iround(trail_[i - 1].y * R0),
                   cx + iround(trail_[i].x * R0),     cy - iround(trail_[i].y * R0), c);
        }
        r.glow(cx, cy, 26, Color{255, 70, 40, 255});
        r.fillCircle(cx, cy, 7, kNucleus);
        if (!collapsed_) {
            int ex = cx + iround(x_ * R0), ey = cy - iround(y_ * R0);
            r.glow(ex, ey, 16, Color{60, 140, 255, 255});
            r.fillCircle(ex, ey, 5, kElectron);
        } else {
            r.circle(cx, cy, 20 + iround(8 * std::sin(collapseTimer_ * 12)), Color{255, 220, 120, 255});
        }
    }

    void key(int k) override {
        if (k == GLFW_KEY_R) reset();
        if (k == GLFW_KEY_SPACE) paused_ = !paused_;
    }

    std::string title() const override {
        char b[160];
        if (collapsed_) return "Stage 1 - Planetary | COLLAPSED: classical electron radiated away its energy";
        std::snprintf(b, sizeof b, "Stage 1 - Planetary (classical) | r = %.2f | E = %.3f", std::hypot(x_, y_), energy());
        return b;
    }
    const char* help() const override { return "[R] restart   [Space] pause"; }

private:
    static constexpr double kSimSpeed = 4.0;     // simulation time units per real second
    static constexpr double kLarmor = 0.003;     // radiation-loss strength (bigger = faster collapse)

    struct P { double x, y; };

    double energy() const { return 0.5 * (vx_ * vx_ + vy_ * vy_) - 1.0 / std::hypot(x_, y_); }

    void accel(double x, double y, double& ax, double& ay) const {
        double r2 = x * x + y * y, r3 = r2 * std::sqrt(r2);
        ax = -x / r3; ay = -y / r3;
    }

    // velocity-Verlet for the Coulomb force, then remove the radiated energy from |v|.
    void step(double h) {
        double ax, ay;
        accel(x_, y_, ax, ay);
        x_ += vx_ * h + 0.5 * ax * h * h;
        y_ += vy_ * h + 0.5 * ay * h * h;
        double nax, nay;
        accel(x_, y_, nax, nay);
        vx_ += 0.5 * (ax + nax) * h;
        vy_ += 0.5 * (ay + nay) * h;
        // Larmor: dE/dt = -kLarmor * |a|^2   ->   dv = -kLarmor * |a|^2 / |v| * dt  (along v)
        double speed = std::hypot(vx_, vy_), a2 = nax * nax + nay * nay;
        if (speed > 1e-9) {
            double dv = kLarmor * a2 / speed * h;
            double f = std::max(0.0, (speed - dv) / speed);
            vx_ *= f; vy_ *= f;
        }
    }

    void reset() {
        x_ = 1.0; y_ = 0.0; vx_ = 0.0; vy_ = 1.0;   // circular orbit of radius 1
        trail_.clear();
        collapsed_ = false;
    }

    int w_ = 800, h_ = 600;
    double x_ = 1, y_ = 0, vx_ = 0, vy_ = 1;
    bool collapsed_ = false, paused_ = false;
    double collapseTimer_ = 0;
    std::deque<P> trail_;
};

// ---------------------------------------------------------------------------------------
// Stage 2 — Bohr model. E_n = -13.6 eV / n^2, r_n = n^2 a0, omega ~ 1/n^3.
// Up arrow: a photon flies in; when absorbed the electron jumps n -> n+1.
// Down arrow: the electron drops n -> n-1 and emits a photon with lambda = hc / dE.
// ---------------------------------------------------------------------------------------
class BohrStage : public Stage2D {
public:
    void update(double dt, int w, int h) override {
        w_ = w; h_ = h;
        theta_ += omega() * dt;
        if (theta_ > 2 * kPi) theta_ -= 2 * kPi;
        clock_ += dt;

        if (phase_ == Phase::PhotonIn) {
            photonT_ += dt;
            if (photonT_ >= kFlight) {                  // photon reached the electron: absorb
                photonActive_ = false;
                startJump(target_);
            }
        } else if (phase_ == Phase::Jumping) {
            jumpT_ += dt;
            if (jumpT_ >= kJump) { n_ = target_; phase_ = Phase::Idle; }
        }
        if (photonActive_ && phase_ != Phase::PhotonIn) {
            photonT_ += dt;
            double dist = std::hypot(photonHeadX() - w_ * 0.5, photonHeadY() - h_ * 0.5);
            if (dist > 0.75 * std::hypot(w_, h_)) photonActive_ = false;
        }
    }

    void draw(Raster& r) override {
        r.clear(kBg);
        int cx = cxOf(), cy = h_ / 2;
        // all allowed orbits (midpoint circles)
        for (int n = 1; n <= kNMax; ++n) {
            Color c = (n == n_ && phase_ == Phase::Idle) ? Color{70, 100, 150, 255} : kDim;
            r.circle(cx, cy, iround(radiusPx(n)), c);
        }
        r.glow(cx, cy, 12, Color{255, 70, 40, 255});
        r.fillCircle(cx, cy, 4, kNucleus);

        // electron
        double rad = currentRadius();
        int ex = cx + iround(rad * std::cos(theta_)), ey = cy - iround(rad * std::sin(theta_));
        r.glow(ex, ey, 14, Color{60, 140, 255, 255});
        r.fillCircle(ex, ey, 5, kElectron);

        if (photonActive_) drawPhoton(r);
        drawLevelDiagram(r);
    }

    void key(int k) override {
        if (phase_ != Phase::Idle) return;               // one transition at a time
        if (k == GLFW_KEY_UP && n_ < kNMax) {
            target_ = n_ + 1;
            lastFrom_ = n_; lastTo_ = target_; lastAbsorb_ = true;
            startPhoton(true);
        } else if (k == GLFW_KEY_DOWN && n_ > 1) {
            target_ = n_ - 1;
            lastFrom_ = n_; lastTo_ = target_; lastAbsorb_ = false;
            startJump(target_);
        }
    }

    std::string title() const override {
        char b[200];
        double e = -13.6 / (double(n_) * n_);
        if (lastFrom_ == 0)
            std::snprintf(b, sizeof b, "Stage 2 - Bohr | n = %d | E = %.2f eV", n_, e);
        else {
            double dE = 13.6 * std::fabs(1.0 / (double(lastFrom_) * lastFrom_) - 1.0 / (double(lastTo_) * lastTo_));
            double nm = 1239.84 / dE;
            const char* series = seriesName(std::min(lastFrom_, lastTo_));
            std::snprintf(b, sizeof b, "Stage 2 - Bohr | n = %d | E = %.2f eV | last: %d->%d %s dE = %.2f eV, %.0f nm (%s)",
                          n_, e, lastFrom_, lastTo_, lastAbsorb_ ? "absorb" : "emit", dE, nm, series);
        }
        return b;
    }
    const char* help() const override { return "[Up] absorb photon (excite)   [Down] emit photon (relax)"; }

private:
    enum class Phase { Idle, PhotonIn, Jumping };
    static constexpr int kNMax = 5;
    static constexpr double kFlight = 0.9;               // seconds for the incoming photon
    static constexpr double kJump = 0.6;                 // seconds for the electron to change orbit
    static constexpr double kPhotonSpeed = 520.0;        // px / s

    int cxOf() const { return w_ / 2 - 60; }
    double a0px() const { return 0.44 * std::min(w_, h_) / (kNMax * kNMax); }
    double radiusPx(int n) const { return a0px() * n * n; }
    double omega() const {                                // Bohr: period ~ n^3
        double n = (phase_ == Phase::Jumping) ? 0.5 * (n_ + target_) : n_;
        return 12.0 / (n * n * n);
    }
    double currentRadius() const {
        if (phase_ != Phase::Jumping) return radiusPx(n_);
        double t = std::clamp(jumpT_ / kJump, 0.0, 1.0);
        t = t * t * (3 - 2 * t);                          // smoothstep ease
        return radiusPx(n_) + (radiusPx(target_) - radiusPx(n_)) * t;
    }

    static const char* seriesName(int nLow) {
        switch (nLow) { case 1: return "Lyman"; case 2: return "Balmer"; case 3: return "Paschen"; case 4: return "Brackett"; default: return "Pfund"; }
    }

    void startJump(int to) {
        target_ = to;
        phase_ = Phase::Jumping;
        jumpT_ = 0;
        if (!lastAbsorb_) startPhoton(false);             // relaxing: emit a photon as we fall
    }

    void startPhoton(bool incoming) {
        double hi = std::max(lastFrom_, lastTo_), lo = std::min(lastFrom_, lastTo_);
        double dE = 13.6 * (1.0 / (lo * lo) - 1.0 / (hi * hi));
        photonColor_ = wavelengthToColor(1239.84 / dE);
        photonT_ = 0;
        photonActive_ = true;
        photonIncoming_ = incoming;
        if (incoming) {
            phase_ = Phase::PhotonIn;
            // aim at where the electron will be when the photon arrives
            double th = theta_ + omega() * kFlight, rad = radiusPx(n_);
            aimX_ = cxOf() + rad * std::cos(th);
            aimY_ = h_ / 2 - rad * std::sin(th);
            dirX_ = 1.0; dirY_ = 0.0;                     // photon travels left -> right
        } else {
            double rad = currentRadius();
            double ex = cxOf() + rad * std::cos(theta_), ey = h_ / 2 - rad * std::sin(theta_);
            aimX_ = ex; aimY_ = ey;                       // emission point
            double dx = ex - cxOf(), dy = ey - h_ / 2, len = std::max(1.0, std::hypot(dx, dy));
            dirX_ = dx / len; dirY_ = dy / len;           // fly radially outward
        }
    }

    double photonHeadDist() const {                        // how far along its path the head is
        return photonIncoming_ ? kPhotonSpeed * (photonT_ - kFlight) : kPhotonSpeed * photonT_;
    }
    double photonHeadX() const { return aimX_ + dirX_ * photonHeadDist(); }
    double photonHeadY() const { return aimY_ + dirY_ * photonHeadDist(); }

    // A photon = short sine wave packet drawn as a polyline of Bresenham segments.
    void drawPhoton(Raster& r) const {
        const double len = 150.0, k = 2 * kPi / 22.0, amp = 9.0;
        const double head = photonHeadDist();
        double nx = -dirY_, ny = dirX_;                    // perpendicular
        int px = 0, py = 0; bool have = false;
        for (double s = 0; s <= len; s += 2.0) {
            double d = head - s;                           // distance along path of this sample
            double env = std::sin(kPi * s / len);          // smooth packet envelope
            double off = amp * env * std::sin(k * d - 18.0 * clock_);
            int x = iround(aimX_ + dirX_ * d + nx * off), y = iround(aimY_ + dirY_ * d + ny * off);
            if (have) r.line(px, py, x, y, photonColor_);
            px = x; py = y; have = true;
        }
    }

    // Right-hand panel: energy levels on a y-axis proportional to E_n.
    void drawLevelDiagram(Raster& r) const {
        int x0 = w_ - 190, x1 = w_ - 40;
        int top = h_ / 2 - iround(0.36 * h_), bottom = h_ / 2 + iround(0.36 * h_);
        for (int n = 1; n <= kNMax; ++n) {
            double e = -13.6 / (double(n) * n);            // -13.6 .. -0.54
            // map E in [-13.6, 0] -> y in [bottom, top]
            int y = bottom - iround((e + 13.6) / 13.6 * (bottom - top));
            bool on = (n == n_);
            Color c = on ? Color{255, 220, 120, 255} : Color{70, 85, 120, 255};
            r.line(x0, y, x1, y, c);
            if (on) r.fillCircle(x0 - 10, y, 4, kElectron);
        }
        r.line(x0, top, x1, top, Color{40, 50, 70, 255});  // E = 0 (ionised)
    }

    int w_ = 800, h_ = 600;
    int n_ = 1, target_ = 1;
    int lastFrom_ = 0, lastTo_ = 0;
    bool lastAbsorb_ = true;
    Phase phase_ = Phase::Idle;
    double theta_ = 0, jumpT_ = 0, photonT_ = 0, clock_ = 0;
    bool photonActive_ = false, photonIncoming_ = true;
    double aimX_ = 0, aimY_ = 0, dirX_ = 1, dirY_ = 0;
    Color photonColor_{255, 255, 255, 255};
};

// ---------------------------------------------------------------------------------------
// Stage 3 — de Broglie: the electron is a wave. Only orbits whose circumference holds a
// whole number of wavelengths (2*pi*r = n*lambda) interfere constructively and survive.
// Ring radius is modulated by cos(k*theta) * cos(w t): a parametric curve drawn with
// Bresenham segments. Press F for a fractional k and watch the wave fail to close.
// ---------------------------------------------------------------------------------------
class WaveStage : public Stage2D {
public:
    void update(double dt, int w, int h) override {
        w_ = w; h_ = h;
        t_ += dt;
    }

    void draw(Raster& r) override {
        r.clear(kBg);
        int cx = w_ / 2, cy = h_ / 2;
        double base = 0.44 * std::min(w_, h_) * (double(n_) * n_) / 25.0;
        double amp = std::min(26.0, 0.18 * base + 8.0);
        double k = fractional_ ? n_ + 0.5 : n_;

        r.circle(cx, cy, iround(base), kDim);               // the Bohr orbit for reference
        r.glow(cx, cy, 20, Color{255, 70, 40, 255});
        r.fillCircle(cx, cy, 6, kNucleus);

        const int segs = 720;
        double envelope = std::cos(2.2 * t_);                // standing wave: amplitude breathes in time
        int px = 0, py = 0;
        for (int i = 0; i <= segs; ++i) {
            double th = 2 * kPi * i / segs;
            double disp = amp * envelope * std::cos(k * th);
            double rad = base + disp;
            int x = cx + iround(rad * std::cos(th)), y = cy - iround(rad * std::sin(th));
            double sign = std::cos(k * th) * envelope;       // colour by displacement sign
            Color c = sign >= 0 ? Color{255, 170, 80, 255} : Color{90, 170, 255, 255};
            if (i > 0) r.line(px, py, x, y, c);
            px = x; py = y;
        }
        if (!fractional_) {                                   // mark the 2n nodes (cos(n*theta) = 0)
            for (int j = 0; j < 2 * n_; ++j) {
                double th = (kPi / 2 + kPi * j) / n_;
                r.fillCircle(cx + iround(base * std::cos(th)), cy - iround(base * std::sin(th)), 4, Color{240, 240, 240, 255});
            }
        }
    }

    void key(int k) override {
        if (k == GLFW_KEY_UP && n_ < 5) ++n_;
        if (k == GLFW_KEY_DOWN && n_ > 1) --n_;
        if (k == GLFW_KEY_F) fractional_ = !fractional_;
    }

    std::string title() const override {
        char b[200];
        if (fractional_)
            std::snprintf(b, sizeof b, "Stage 3 - Waves | n = %d | %.1f wavelengths: the wave does NOT close -> destructive interference, not allowed", n_, n_ + 0.5);
        else
            std::snprintf(b, sizeof b, "Stage 3 - Waves | n = %d | circumference = %d wavelengths (2*pi*r = n*lambda), %d nodes", n_, n_, 2 * n_);
        return b;
    }
    const char* help() const override { return "[Up/Down] n   [F] toggle non-integer wavelength"; }

private:
    int w_ = 800, h_ = 600, n_ = 3;
    bool fractional_ = false;
    double t_ = 0;
};

}  // namespace

std::unique_ptr<Stage2D> makePlanetaryStage() { return std::make_unique<PlanetaryStage>(); }
std::unique_ptr<Stage2D> makeBohrStage() { return std::make_unique<BohrStage>(); }
std::unique_ptr<Stage2D> makeWaveStage() { return std::make_unique<WaveStage>(); }
