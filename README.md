# Atomica — from a classical atom to quantum orbitals

ICS 3204 Computer Graphics semester project. An interactive application that walks through
how our picture of the hydrogen atom changed, and renders every step with graphics
algorithms we implemented ourselves.


## The four stages (press 1–4)

| Stage | What you see | Idea |
|---|---|---|
| 1 Planetary | Electron orbiting a nucleus, slowly spiralling in | Classical physics predicts the atom collapses |
| 2 Bohr | Fixed orbits, energy-level diagram, photons flying in/out | E_n = −13.6 eV/n², photon λ = hc/ΔE |
| 3 Waves | Ring-shaped standing wave with 2n nodes | 2πr = nλ: only whole wavelengths survive |
| 4 Orbitals | 3D cloud of 150 000 sampled electron positions | ψ = R_nl(r)·Y_lm(θ,φ), shapes from (n, l, m) |

## Controls

| Stage | Keys |
|---|---|
| all | `1`–`4` switch stage · `H` print help · `Esc` quit |
| 1 | `R` restart · `Space` pause |
| 2 | `↑` absorb a photon (excite) · `↓` emit a photon (relax) |
| 3 | `↑/↓` change n · `F` non-integer wavelength (wave fails to close) |
| 4 | `PgUp/PgDn` n · `↑/↓` l · `←/→` m · `+/-` point count · `P` phase colours · `Space` spin · `A` axes · `[` `]` point size · `R` reset camera · mouse drag = orbit · wheel = zoom |

## Build and run (Ubuntu 24.04)

    ./scripts/setup_ubuntu.sh        # installs packages, builds, runs tests
    ./build/atomica                  # or: ./build/atomica --stage 4

Manual steps: `sudo apt install build-essential cmake git libglfw3-dev libgl1-mesa-dev`,
then `cmake -B build -S . && cmake --build build -j && ctest --test-dir build`.

Requires OpenGL 3.3 core (any Intel/AMD/NVIDIA GPU from the last ~10 years).

## Technologies

C++17 · OpenGL 3.3 core (Mesa) · GLFW 3.3 (window/input) · CMake. No other libraries:
the matrix maths and the rasteriser are written by hand.

## Graphics techniques implemented (and why)

| Technique | Where | Why it is used |
|---|---|---|
| Bresenham line algorithm | `src/raster.cpp` `Raster::line` | Orbit trails, photon waves, standing-wave curve, energy levels |
| Midpoint circle algorithm | `Raster::circle`, `fillCircle` | Bohr orbits (circles are the model), nucleus/electron discs |
| Scan-line span filling | `Raster::fillCircle` | Solid discs from the same midpoint walk |
| Parametric curves | `stages2d.cpp` `WaveStage`, photon packets | Standing wave r(θ)=r_n+A·cos(nθ)·cos(ωt); sinusoidal photons |
| Raster operations / blending | `Raster::plot`, `plotAdd`, `glow` | Alpha blending and additive glow, done per pixel on the CPU |
| Colour techniques | `wavelengthToColor`, shader `ramp()` | Photon colour from wavelength; density → colour ramp; phase ± colouring |
| 2D animation | `stages2d.cpp` | Time-stepped physics (velocity-Verlet), eased orbit jumps |
| 3D transformations | `src/mathx.h`, `orbital_view.cpp` | Hand-written rotate/translate/perspective; MVP = P·V·M |
| Perspective projection | `Mat4::perspective` | Depth cue: point size ∝ 1/w |
| GPU pipeline (VAO/VBO, GLSL) | `orbital_view.cpp`, `main.cpp` | 150k points per frame; CPU raster uploaded as a texture |
| Additive blending | `orbital_view.cpp` | Overlapping points sum, so dense regions look brighter = probability |
| Sampling / Monte-Carlo | `src/orbital.cpp` | Inverse-CDF for r, rejection sampling for angles |

Detailed explanations to rehearse for the demo: `docs/TECHNIQUES.md`.

## Tests

`ctest --test-dir build --output-on-failure` checks the sampler against the textbook
expectation value ⟨r⟩ = (3n² − l(l+1))/2 for seven orbitals, and checks that 2p_z has an
empty nodal plane with opposite phase in its two lobes.

## Scope (by design)

A smaller, finished project beats an ambitious unfinished one. Left out on purpose:
the ray-traced renderer (hours per frame on an iGPU) and hydrogen-only physics (no
multi-electron atoms). Possible extensions: fragment-shader volume ray-marching at low
resolution, an orbital-superposition animation (ψ = ψ_a + e^{-iΔEt}ψ_b), mouse picking.

## Team and contributions

| Member | GitHub | Main responsibility |
|---|---|---|
| _name_ | _handle_ | e.g. software rasteriser (Stage 1–3 algorithms) |
| _name_ | _handle_ | e.g. orbital sampling and physics tests |
| _name_ | _handle_ | e.g. 3D renderer, shaders, camera |
| _name_ | _handle_ | e.g. Bohr stage animation and photon colours |
| _name_ | _handle_ | e.g. documentation, report, demo script |
| _name_ | _handle_ | e.g. integration, build system, testing on other machines |

Everyone commits under their own GitHub account, regularly, via pull requests.
