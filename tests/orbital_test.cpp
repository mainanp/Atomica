// Headless correctness checks for the physics core.
// Run: ctest --test-dir build --output-on-failure
#include <cmath>
#include <cstdio>
#include <cstdlib>

#include "orbital.h"

static int failures = 0;
#define CHECK(cond, ...)                                  \
    do {                                                  \
        if (!(cond)) { ++failures; std::printf("FAIL: "); std::printf(__VA_ARGS__); std::printf("\n"); } \
    } while (0)

int main() {
    // 1) Mean radius of sampled points must match <r> = (3n^2 - l(l+1)) / 2  (Bohr radii).
    struct Case { int n, l, m; };
    const Case cases[] = {{1, 0, 0}, {2, 0, 0}, {2, 1, 0}, {3, 1, 1}, {3, 2, -1}, {4, 3, 2}, {5, 2, 0}};
    for (const Case& c : cases) {
        OrbitalCloud cloud = sampleOrbital(c.n, c.l, c.m, 200000, 12345);
        double sum = 0.0;
        for (size_t i = 0; i < cloud.count(); ++i) {
            const float* p = &cloud.data[i * OrbitalCloud::kFloatsPerPoint];
            sum += std::sqrt(double(p[0]) * p[0] + double(p[1]) * p[1] + double(p[2]) * p[2]) * cloud.rScale;
        }
        double mean = sum / cloud.count(), want = expectedMeanRadius(c.n, c.l);
        std::printf("n=%d l=%d m=%+d  <r> sampled %.3f  expected %.3f\n", c.n, c.l, c.m, mean, want);
        CHECK(std::fabs(mean - want) / want < 0.02, "mean radius off for (%d,%d,%d)", c.n, c.l, c.m);
    }

    // 2) 2p_z has a nodal plane (z = 0): it should have almost no points near it,
    //    and the two lobes must carry opposite phase signs. (Render-space y == physical z.)
    {
        OrbitalCloud cloud = sampleOrbital(2, 1, 0, 100000, 7);
        size_t nearPlane = 0, pos = 0, neg = 0;
        for (size_t i = 0; i < cloud.count(); ++i) {
            const float* p = &cloud.data[i * OrbitalCloud::kFloatsPerPoint];
            if (std::fabs(p[1]) < 0.01f) ++nearPlane;
            if ((p[1] > 0) == (p[4] > 0)) ++pos; else ++neg;
        }
        std::printf("2p_z: near nodal plane %zu / %zu\n", nearPlane, cloud.count());
        CHECK(nearPlane < cloud.count() / 100, "2p_z nodal plane is not empty");
        CHECK(neg == 0 || pos == 0, "2p_z lobes should have phase sign = sign(z)");
    }

    // 3) Quantum-number validation.
    CHECK(validQuantumNumbers(3, 2, -2), "3,2,-2 should be valid");
    CHECK(!validQuantumNumbers(2, 2, 0), "l must be < n");
    CHECK(!validQuantumNumbers(3, 1, 2), "|m| must be <= l");

    std::printf(failures ? "\n%d check(s) FAILED\n" : "\nall checks passed\n", failures);
    return failures ? EXIT_FAILURE : EXIT_SUCCESS;
}
