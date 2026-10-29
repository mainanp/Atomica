#include "orbital.h"

#include <algorithm>
#include <cmath>
#include <random>

namespace {

constexpr double kPi = 3.14159265358979323846;

// Generalised Laguerre polynomial L_k^alpha(x) by the three-term recurrence.
double laguerre(int k, double alpha, double x) {
    if (k == 0) return 1.0;
    double Lm2 = 1.0, Lm1 = 1.0 + alpha - x;
    if (k == 1) return Lm1;
    for (int i = 2; i <= k; ++i) {
        double Li = ((2.0 * i - 1.0 + alpha - x) * Lm1 - (i - 1.0 + alpha) * Lm2) / i;
        Lm2 = Lm1;
        Lm1 = Li;
    }
    return Lm1;
}

// Associated Legendre P_l^m(x), m >= 0, WITHOUT the Condon-Shortley sign
// (the sign only flips whole lobes; it does not change any density).
double assocLegendre(int l, int m, double x) {
    double pmm = 1.0;
    if (m > 0) {
        double somx2 = std::sqrt((1.0 - x) * (1.0 + x));
        double fact = 1.0;
        for (int i = 1; i <= m; ++i) { pmm *= fact * somx2; fact += 2.0; }
    }
    if (l == m) return pmm;
    double pmmp1 = x * (2.0 * m + 1.0) * pmm;
    if (l == m + 1) return pmmp1;
    double pll = 0.0;
    for (int ll = m + 2; ll <= l; ++ll) {
        pll = (x * (2.0 * ll - 1.0) * pmmp1 - (ll + m - 1.0) * pmm) / (ll - m);
        pmm = pmmp1;
        pmmp1 = pll;
    }
    return pll;
}

}  // namespace

bool validQuantumNumbers(int n, int l, int m) {
    return n >= 1 && l >= 0 && l <= n - 1 && std::abs(m) <= l;
}

void clampQuantumNumbers(int& n, int& l, int& m, int nMax) {
    n = std::clamp(n, 1, nMax);
    l = std::clamp(l, 0, n - 1);
    m = std::clamp(m, -l, l);
}

std::string orbitalName(int n, int l, int m) {
    static const char letters[] = "spdfgh";
    std::string s = std::to_string(n);
    if (l >= 0 && l <= 5) s += letters[l];
    switch (l) {
        case 0: return s;
        case 1: return s + (m == 0 ? "_z" : m == 1 ? "_x" : "_y");
        case 2: {
            const char* sub[] = {"_xy", "_yz", "_z2", "_xz", "_x2-y2"};
            return s + sub[m + 2];
        }
        default: return s + " (m=" + std::to_string(m) + ")";
    }
}

double radialR(int n, int l, double r) {
    double rho = 2.0 * r / n;
    return std::exp(-0.5 * rho) * std::pow(rho, l) * laguerre(n - l - 1, 2.0 * l + 1.0, rho);
}

double realAngular(int l, int m, double theta, double phi) {
    double x = std::cos(theta);
    int am = std::abs(m);
    double P = assocLegendre(l, am, x);
    if (m == 0) return P;
    return P * (m > 0 ? std::cos(am * phi) : std::sin(am * phi));
}

double expectedMeanRadius(int n, int l) { return 0.5 * (3.0 * n * n - l * (l + 1.0)); }

OrbitalCloud sampleOrbital(int n, int l, int m, size_t count, uint32_t seed) {
    clampQuantumNumbers(n, l, m, 12);
    OrbitalCloud cloud;
    cloud.n = n; cloud.l = l; cloud.m = m;

    // ---- 1. radial distribution P(r) = r^2 R^2, tabulated, then inverted (inverse-CDF sampling)
    const int N = 8192;
    const double rMax = 2.0 * n * n + 15.0 * n;            // far beyond where e^(-r/n) dies out
    const double dr = rMax / N;
    std::vector<double> cdf(N + 1, 0.0);
    double maxR2 = 0.0;
    for (int i = 1; i <= N; ++i) {
        double r0 = (i - 1) * dr, r1 = i * dr;
        double R0 = radialR(n, l, r0), R1 = radialR(n, l, r1);
        double p0 = r0 * r0 * R0 * R0, p1 = r1 * r1 * R1 * R1;
        cdf[i] = cdf[i - 1] + 0.5 * (p0 + p1) * dr;         // trapezoid rule
        maxR2 = std::max(maxR2, R1 * R1);
    }
    maxR2 = std::max(maxR2, std::pow(radialR(n, l, 0.0), 2));
    const double total = cdf[N];
    for (auto& c : cdf) c /= total;

    auto invCdf = [&](double u) {
        size_t i = std::lower_bound(cdf.begin(), cdf.end(), u) - cdf.begin();
        if (i == 0) return 0.0;
        double c0 = cdf[i - 1], c1 = cdf[i];
        double t = (c1 > c0) ? (u - c0) / (c1 - c0) : 0.0;
        return (i - 1 + t) * dr;
    };
    cloud.rScale = invCdf(0.99);                             // 99 % of the electron lives inside this

    // ---- 2. angular part: rejection sampling on |Y|^2 (find its maximum on a grid first)
    double maxY2 = 0.0;
    for (int it = 0; it <= 180; ++it)
        for (int ip = 0; ip < 360; ++ip) {
            double y = realAngular(l, m, it * kPi / 180.0, ip * kPi / 180.0 * 1.0);
            maxY2 = std::max(maxY2, y * y);
        }
    maxY2 *= 1.02;

    // ---- 3. draw the points
    std::mt19937 rng(seed);
    std::uniform_real_distribution<double> U(0.0, 1.0);
    cloud.data.resize(count * OrbitalCloud::kFloatsPerPoint);
    const double inv = 1.0 / cloud.rScale;

    for (size_t k = 0; k < count; ++k) {
        double r = invCdf(U(rng));
        double theta, phi, Y;
        for (;;) {
            double c = 2.0 * U(rng) - 1.0;                   // cos(theta) uniform => uniform on sphere
            theta = std::acos(c);
            phi = 2.0 * kPi * U(rng);
            Y = realAngular(l, m, theta, phi);
            if (U(rng) * maxY2 <= Y * Y) break;
        }
        double R = radialR(n, l, r);
        double psi = R * Y;
        double dens = std::min(1.0, (psi * psi) / (maxR2 * maxY2));

        double st = std::sin(theta);
        double px = r * st * std::cos(phi), py = r * st * std::sin(phi), pz = r * std::cos(theta);
        float* o = &cloud.data[k * OrbitalCloud::kFloatsPerPoint];
        o[0] = static_cast<float>(px * inv);                 // physical z axis points up on screen
        o[1] = static_cast<float>(pz * inv);
        o[2] = static_cast<float>(-py * inv);
        o[3] = static_cast<float>(dens);
        o[4] = psi >= 0.0 ? 1.0f : -1.0f;
    }
    return cloud;
}
