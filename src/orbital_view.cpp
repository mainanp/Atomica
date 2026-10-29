#include "orbital_view.h"

#include <algorithm>
#include <chrono>
#include <cstdio>

namespace {

// ---- point cloud shaders --------------------------------------------------------------
// Vertex: the MVP matrix (Projection * View * Model) maps each electron position from
// model space to clip space; the GPU does the perspective divide. Point size shrinks with
// distance (size ~ 1/w) so depth is readable without any lighting.
const char* kPointVS = R"GLSL(
#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec2 aAttr;           // x = |psi|^2 (0..1), y = phase sign
uniform mat4  uMVP;
uniform float uSize;
uniform float uDist;
uniform int   uPhase;
out vec3 vColor;

vec3 ramp(float t) {                          // low density -> high density
    vec3 c0 = vec3(0.10, 0.22, 0.95);
    vec3 c1 = vec3(0.70, 0.20, 0.90);
    vec3 c2 = vec3(1.00, 0.50, 0.12);
    vec3 c3 = vec3(1.00, 0.95, 0.75);
    if (t < 0.5)  return mix(c0, c1, t * 2.0);
    if (t < 0.85) return mix(c1, c2, (t - 0.5) / 0.35);
    return mix(c2, c3, (t - 0.85) / 0.15);
}

void main() {
    gl_Position  = uMVP * vec4(aPos, 1.0);
    gl_PointSize = clamp(uSize * uDist / max(gl_Position.w, 0.1), 1.0, 14.0);
    float t = pow(aAttr.x, 0.55);
    if (uPhase == 1) {
        vec3 base = aAttr.y > 0.0 ? vec3(1.00, 0.42, 0.10) : vec3(0.15, 0.55, 1.00);
        vColor = base * (0.45 + 0.55 * t);
    } else {
        vColor = ramp(t);
    }
}
)GLSL";

const char* kPointFS = R"GLSL(
#version 330 core
in vec3 vColor;
uniform float uAlpha;
out vec4 fragColor;
void main() {
    vec2 d = gl_PointCoord * 2.0 - 1.0;
    float r2 = dot(d, d);
    if (r2 > 1.0) discard;                    // round points
    fragColor = vec4(vColor, uAlpha * (1.0 - r2));
}
)GLSL";

// ---- axes (GL_LINES primitive) --------------------------------------------------------
const char* kAxesVS = R"GLSL(
#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aCol;
uniform mat4 uMVP;
out vec3 vCol;
void main() { gl_Position = uMVP * vec4(aPos, 1.0); vCol = aCol; }
)GLSL";

const char* kAxesFS = R"GLSL(
#version 330 core
in vec3 vCol;
out vec4 fragColor;
void main() { fragColor = vec4(vCol, 0.8); }
)GLSL";

// render-space axes: +x red (physical x), +y blue (physical z, the quantisation axis), -z green (physical y)
const float kAxes[] = {
    0, 0, 0,  0.9f, 0.25f, 0.25f,   1.2f, 0, 0,     0.9f, 0.25f, 0.25f,
    0, 0, 0,  0.3f, 0.5f, 1.0f,     0, 1.2f, 0,     0.3f, 0.5f, 1.0f,
    0, 0, 0,  0.3f, 0.85f, 0.35f,   0, 0, -1.2f,    0.3f, 0.85f, 0.35f,
};

}  // namespace

bool OrbitalView::init() {
    prog_ = buildProgram(kPointVS, kPointFS);
    axesProg_ = buildProgram(kAxesVS, kAxesFS);
    if (!prog_ || !axesProg_) return false;

    uMVP_ = glGetUniformLocation(prog_, "uMVP");
    uSize_ = glGetUniformLocation(prog_, "uSize");
    uAlpha_ = glGetUniformLocation(prog_, "uAlpha");
    uPhase_ = glGetUniformLocation(prog_, "uPhase");
    uDist_ = glGetUniformLocation(prog_, "uDist");
    uAxesMVP_ = glGetUniformLocation(axesProg_, "uMVP");

    glGenVertexArrays(1, &vao_);
    glGenBuffers(1, &vbo_);
    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    const GLsizei stride = OrbitalCloud::kFloatsPerPoint * sizeof(float);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride, (void*)(3 * sizeof(float)));

    glGenVertexArrays(1, &axesVao_);
    glGenBuffers(1, &axesVbo_);
    glBindVertexArray(axesVao_);
    glBindBuffer(GL_ARRAY_BUFFER, axesVbo_);
    glBufferData(GL_ARRAY_BUFFER, sizeof kAxes, kAxes, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
    glBindVertexArray(0);

    resample();
    return true;
}

void OrbitalView::shutdown() {
    glDeleteBuffers(1, &vbo_);
    glDeleteVertexArrays(1, &vao_);
    glDeleteBuffers(1, &axesVbo_);
    glDeleteVertexArrays(1, &axesVao_);
    glDeleteProgram(prog_);
    glDeleteProgram(axesProg_);
}

void OrbitalView::resample() {
    clampQuantumNumbers(n_, l_, m_, 6);
    auto t0 = std::chrono::steady_clock::now();
    cloud_ = sampleOrbital(n_, l_, m_, count_, 20260101u);
    sampleMs_ = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    upload();
}

void OrbitalView::upload() {
    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER, cloud_.data.size() * sizeof(float), cloud_.data.data(), GL_STATIC_DRAW);
    glBindVertexArray(0);
}

void OrbitalView::update(double dt) {
    if (autoRotate_) yaw_ += static_cast<float>(0.35 * dt);
}

void OrbitalView::draw(int fbW, int fbH) {
    glViewport(0, 0, fbW, fbH);
    glClearColor(0.01f, 0.012f, 0.025f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glDisable(GL_DEPTH_TEST);                     // additive blending: order does not matter

    // Model = identity (the atom sits at the origin); View = camera orbit; Projection = perspective.
    Mat4 proj = Mat4::perspective(0.9f, static_cast<float>(fbW) / std::max(1, fbH), 0.1f, 50.0f);
    Mat4 view = Mat4::translate(0, 0, -dist_) * Mat4::rotateX(pitch_) * Mat4::rotateY(yaw_);
    Mat4 mvp = proj * view * Mat4::identity();

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    glEnable(GL_PROGRAM_POINT_SIZE);

    glUseProgram(prog_);
    glUniformMatrix4fv(uMVP_, 1, GL_FALSE, mvp.m);
    glUniform1f(uSize_, pointSize_ * fbH / 800.0f);
    glUniform1f(uDist_, dist_);
    glUniform1f(uAlpha_, std::clamp(22000.0f / static_cast<float>(count_), 0.02f, 0.5f));
    glUniform1i(uPhase_, phaseColors_ ? 1 : 0);
    glBindVertexArray(vao_);
    glDrawArrays(GL_POINTS, 0, static_cast<GLsizei>(cloud_.count()));

    if (showAxes_) {
        glUseProgram(axesProg_);
        glUniformMatrix4fv(uAxesMVP_, 1, GL_FALSE, mvp.m);
        glBindVertexArray(axesVao_);
        glDrawArrays(GL_LINES, 0, 6);
    }
    glBindVertexArray(0);
    glDisable(GL_BLEND);
}

void OrbitalView::key(int k) {
    int n = n_, l = l_, m = m_;
    switch (k) {
        case GLFW_KEY_PAGE_UP:   ++n; break;
        case GLFW_KEY_PAGE_DOWN: --n; break;
        case GLFW_KEY_UP:        ++l; break;
        case GLFW_KEY_DOWN:      --l; break;
        case GLFW_KEY_RIGHT:     ++m; break;
        case GLFW_KEY_LEFT:      --m; break;
        case GLFW_KEY_EQUAL: case GLFW_KEY_KP_ADD:
            count_ = std::min<size_t>(600000, static_cast<size_t>(count_ * 1.5));
            resample();
            return;
        case GLFW_KEY_MINUS: case GLFW_KEY_KP_SUBTRACT:
            count_ = std::max<size_t>(20000, static_cast<size_t>(count_ / 1.5));
            resample();
            return;
        case GLFW_KEY_P:             phaseColors_ = !phaseColors_; return;
        case GLFW_KEY_SPACE:         autoRotate_ = !autoRotate_; return;
        case GLFW_KEY_A:             showAxes_ = !showAxes_; return;
        case GLFW_KEY_LEFT_BRACKET:  pointSize_ = std::max(1.0f, pointSize_ - 0.4f); return;
        case GLFW_KEY_RIGHT_BRACKET: pointSize_ = std::min(8.0f, pointSize_ + 0.4f); return;
        case GLFW_KEY_R: yaw_ = 0.6f; pitch_ = 0.35f; dist_ = 3.2f; return;
        default: return;
    }
    clampQuantumNumbers(n, l, m, 6);
    if (n != n_ || l != l_ || m != m_) {
        n_ = n; l_ = l; m_ = m;
        resample();
    }
}

void OrbitalView::mouseDrag(double dx, double dy) {
    yaw_ += static_cast<float>(dx * 0.008);
    pitch_ = std::clamp(pitch_ + static_cast<float>(dy * 0.008), -1.5f, 1.5f);
}

void OrbitalView::scroll(double dy) { dist_ = std::clamp(dist_ * (dy > 0 ? 0.92f : 1.08f), 1.2f, 9.0f); }

std::string OrbitalView::title() const {
    char b[220];
    std::snprintf(b, sizeof b, "Stage 4 - Orbital %s (n=%d l=%d m=%d) | %zuk points (%.0f ms to sample) | %s colours",
                  orbitalName(n_, l_, m_).c_str(), n_, l_, m_, cloud_.count() / 1000, sampleMs_,
                  phaseColors_ ? "phase" : "density");
    return b;
}

const char* OrbitalView::help() const {
    return "[PgUp/PgDn] n  [Up/Down] l  [Left/Right] m  [+/-] points  [P] phase colours  [mouse] orbit/zoom  [Space] spin  [A] axes  [ ] ] point size  [R] reset";
}
