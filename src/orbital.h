// orbital.h — hydrogen-atom physics: turn quantum numbers (n, l, m) into a cloud of
// random electron positions whose density follows |psi(r, theta, phi)|^2.
//
//   psi_nlm(r, theta, phi) = R_nl(r) * Y_lm(theta, phi)
//
//   R_nl  : radial part  ~ e^(-rho/2) * rho^l * L_{n-l-1}^{2l+1}(rho),  rho = 2r/(n a0)
//   Y_lm  : REAL spherical harmonics (the lobed p_x, d_xy ... shapes in chemistry books)
//
// No OpenGL in here on purpose: it is unit-tested headless (tests/orbital_test.cpp).
#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

struct OrbitalCloud {
    static constexpr int kFloatsPerPoint = 5;   // x, y, z, density(0..1), sign(+1/-1)

    int n = 1, l = 0, m = 0;
    double rScale = 1.0;          // Bohr radii: stored xyz = true position / rScale
    std::vector<float> data;      // render-space coordinates (physical z -> screen up)

    size_t count() const { return data.size() / kFloatsPerPoint; }
};

// Valid when n >= 1, 0 <= l <= n-1, |m| <= l.
bool validQuantumNumbers(int n, int l, int m);
// Clamp to the nearest valid set (used by the arrow-key UI).
void clampQuantumNumbers(int& n, int& l, int& m, int nMax);
// "2p_z", "3d_xy", ... or "(n=6, l=4, m=2)" for shapes without a classic name.
std::string orbitalName(int n, int l, int m);

// Building blocks (exposed for tests / for explaining in the report).
double radialR(int n, int l, double rBohr);                 // unnormalised R_nl(r)
double realAngular(int l, int m, double theta, double phi); // unnormalised real Y_lm

// Draw `count` electron positions from |psi|^2.
OrbitalCloud sampleOrbital(int n, int l, int m, size_t count, uint32_t seed);

// Textbook expectation value <r> = (a0/2) * (3n^2 - l(l+1))  — used to validate sampling.
double expectedMeanRadius(int n, int l);
