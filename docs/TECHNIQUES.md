# Explaining the graphics (demo / viva cheat sheet)

## Software rasteriser (Stages 1–3)

**Bresenham line.** For a line with slope in [0,1] we step x by one pixel and need to decide
whether y stays or increments. Keeping an integer error `err` = (distance from ideal line to the
current pixel centre, scaled by 2·dx) avoids floating point. The implementation uses the
all-octant form: `e2 = 2*err; if (e2 >= dy) {err += dy; x += sx;} if (e2 <= dx) {err += dx; y += sy;}`.

**Midpoint circle.** Only one octant (0° to 45°) is computed. At each step we pick between
pixel E (x+1, y) and SE (x+1, y−1) by evaluating the circle function f(x,y)=x²+y²−r² at the
midpoint between them. Decision variable update: `d += 2y+1` (stay) or `d += 2(y−x)+1` (move in).
The other 7 octants are mirror images → 8 plots per step.

**Why CPU-rasterise at all?** Because the algorithms ARE the graphics content of the course.
The GPU only displays the finished image (one textured triangle).

**Photons / standing waves** are parametric curves: sample the parameter, compute (x, y),
connect consecutive samples with Bresenham segments.

## 3D pipeline (Stage 4)

Vertex → **Model** (identity, atom at origin) → **View** (camera: translate(0,0,−d)·Rx(pitch)·Ry(yaw))
→ **Projection** (perspective, fov 0.9 rad) → clip space → GPU divides by w → viewport.
Matrices are column-major, so `MVP = P * V * M` applies M first. Point size = size·d / w
gives a perspective depth cue without any lighting model.

**Why additive blending, no depth test?** Each point is a semi-transparent sample of a
probability cloud; the result should be the SUM of contributions, which is commutative, so
no sorting is needed.

## The physics → graphics mapping

* ψ_nlm = R_nl(r) · Y_lm(θ,φ). |ψ|² is the probability density; we **draw samples from it**.
* Radial: tabulate P(r) = r²R², build the cumulative distribution, invert it with a binary
  search (inverse-transform sampling).
* Angular: pick cosθ ∈ [−1,1] and φ ∈ [0,2π) uniformly (uniform on the sphere), accept with
  probability |Y|²/max|Y|² (rejection sampling).
* Real spherical harmonics (cos mφ / sin mφ combinations) give the lobed shapes in chemistry
  textbooks; the complex Y_lm for m≠0 would give rotationally-symmetric rings.
* Phase colouring uses sign(R·Y): orange = positive, blue = negative lobes.
* Validation: sample mean radius equals the analytic ⟨r⟩ = (3n² − l(l+1))/2 (unit test).

## Likely questions

1. *Why does the classical electron spiral in?* Accelerating charges radiate (Larmor): dE/dt ∝ −a².
2. *Why is Stage 2's electron slower on outer orbits?* Bohr: ω ∝ 1/n³.
3. *Why can't n = 2.5 exist?* Stage 3, key `F`: the wave does not close on itself.
4. *Why do we link libGL directly instead of GLEW/GLAD?* Mesa exports the core-profile entry points; fewer dependencies.
