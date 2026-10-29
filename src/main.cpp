// Atomica — from a classical atom to quantum orbitals.
// Window / input / frame loop. Stages 1-3 are CPU-rasterised (raster.h) and shown through
// one textured triangle; Stage 4 is rendered by the GPU (orbital_view.cpp).
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <cstdio>
#include <memory>
#include <string>

#include "glutil.h"
#include "orbital_view.h"
#include "raster.h"
#include "stages2d.h"

namespace {

const char* kBlitVS = R"GLSL(
#version 330 core
out vec2 uv;
void main() {                                  // one oversized triangle covers the screen
    vec2 p = vec2((gl_VertexID << 1) & 2, gl_VertexID & 2);
    uv = vec2(p.x, 1.0 - p.y);                 // flip Y: raster row 0 is the TOP of the image
    gl_Position = vec4(p * 2.0 - 1.0, 0.0, 1.0);
}
)GLSL";

const char* kBlitFS = R"GLSL(
#version 330 core
in vec2 uv;
uniform sampler2D tex;
out vec4 fragColor;
void main() { fragColor = texture(tex, uv); }
)GLSL";

struct App {
    GLFWwindow* win = nullptr;
    int stage = 1;                             // 1..4
    Raster raster;
    GLuint blitProg = 0, blitVao = 0, tex = 0;
    int texW = 0, texH = 0;
    std::unique_ptr<Stage2D> stages2d[3];
    OrbitalView orbitals;
    bool dragging = false;
    double lastX = 0, lastY = 0;
    int dumpFrames = -1;                       // --screenshot support (testing)
};

void setStage(App& app, int s) {
    app.stage = s;
    const char* help = s == 4 ? app.orbitals.help() : app.stages2d[s - 1]->help();
    std::printf("\n== Stage %d ==\n%s\n   [1-4] switch stage   [Esc] quit\n", s, help);
}

void onKey(GLFWwindow* w, int key, int, int action, int) {
    auto* app = static_cast<App*>(glfwGetWindowUserPointer(w));
    if (action == GLFW_RELEASE) return;
    if (key == GLFW_KEY_ESCAPE) { glfwSetWindowShouldClose(w, GLFW_TRUE); return; }
    if (key >= GLFW_KEY_1 && key <= GLFW_KEY_4) { setStage(*app, key - GLFW_KEY_0); return; }
    if (key == GLFW_KEY_H) { setStage(*app, app->stage); return; }
    if (app->stage == 4) app->orbitals.key(key);
    else if (action == GLFW_PRESS || key == GLFW_KEY_UP || key == GLFW_KEY_DOWN) app->stages2d[app->stage - 1]->key(key);
}

void onMouseButton(GLFWwindow* w, int button, int action, int) {
    auto* app = static_cast<App*>(glfwGetWindowUserPointer(w));
    if (button == GLFW_MOUSE_BUTTON_LEFT) {
        app->dragging = (action == GLFW_PRESS);
        glfwGetCursorPos(w, &app->lastX, &app->lastY);
    }
}

void onCursor(GLFWwindow* w, double x, double y) {
    auto* app = static_cast<App*>(glfwGetWindowUserPointer(w));
    if (app->dragging && app->stage == 4) app->orbitals.mouseDrag(x - app->lastX, y - app->lastY);
    app->lastX = x; app->lastY = y;
}

void onScroll(GLFWwindow* w, double, double dy) {
    auto* app = static_cast<App*>(glfwGetWindowUserPointer(w));
    if (app->stage == 4) app->orbitals.scroll(dy);
}

void ensureTexture(App& app, int w, int h) {
    if (app.tex && w == app.texW && h == app.texH) return;
    if (!app.tex) glGenTextures(1, &app.tex);
    glBindTexture(GL_TEXTURE_2D, app.tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    app.texW = w; app.texH = h;
    app.raster.resize(w, h);
}

}  // namespace

int main(int argc, char** argv) {
    App app;
    int startStage = 1;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--stage" && i + 1 < argc) startStage = std::atoi(argv[++i]);
        else if (a == "--frames" && i + 1 < argc) app.dumpFrames = std::atoi(argv[++i]);
    }

    if (!glfwInit()) { std::fprintf(stderr, "glfwInit failed\n"); return 1; }
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_SAMPLES, 4);

    app.win = glfwCreateWindow(1200, 800, "Atomica", nullptr, nullptr);
    if (!app.win) { std::fprintf(stderr, "Could not create an OpenGL 3.3 window\n"); glfwTerminate(); return 1; }
    glfwMakeContextCurrent(app.win);
    glfwSwapInterval(1);                       // vsync: keeps the iGPU cool and the fans quiet
    std::printf("GL: %s | %s\n", glGetString(GL_RENDERER), glGetString(GL_VERSION));

    glfwSetWindowUserPointer(app.win, &app);
    glfwSetKeyCallback(app.win, onKey);
    glfwSetMouseButtonCallback(app.win, onMouseButton);
    glfwSetCursorPosCallback(app.win, onCursor);
    glfwSetScrollCallback(app.win, onScroll);

    app.blitProg = buildProgram(kBlitVS, kBlitFS);
    glGenVertexArrays(1, &app.blitVao);        // core profile needs a VAO even with no attributes
    if (!app.blitProg || !app.orbitals.init()) { glfwTerminate(); return 1; }

    app.stages2d[0] = makePlanetaryStage();
    app.stages2d[1] = makeBohrStage();
    app.stages2d[2] = makeWaveStage();
    setStage(app, std::clamp(startStage, 1, 4));

    auto last = std::chrono::steady_clock::now();
    double titleTimer = 0, fps = 0;
    int frames = 0, shown = 0;

    while (!glfwWindowShouldClose(app.win)) {
        glfwPollEvents();
        auto now = std::chrono::steady_clock::now();
        double dt = std::min(0.1, std::chrono::duration<double>(now - last).count());
        last = now;

        int fbW, fbH;
        glfwGetFramebufferSize(app.win, &fbW, &fbH);
        if (fbW < 2 || fbH < 2) { glfwWaitEvents(); continue; }   // minimised

        if (app.stage == 4) {
            app.orbitals.update(dt);
            app.orbitals.draw(fbW, fbH);
        } else {
            ensureTexture(app, fbW, fbH);
            Stage2D& s = *app.stages2d[app.stage - 1];
            s.update(dt, fbW, fbH);
            s.draw(app.raster);

            glViewport(0, 0, fbW, fbH);
            glBindTexture(GL_TEXTURE_2D, app.tex);
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, fbW, fbH, GL_RGBA, GL_UNSIGNED_BYTE, app.raster.data());
            glUseProgram(app.blitProg);
            glBindVertexArray(app.blitVao);
            glDrawArrays(GL_TRIANGLES, 0, 3);
        }

        glfwSwapBuffers(app.win);

        ++frames;
        titleTimer += dt;
        if (titleTimer >= 0.3) {
            fps = frames / titleTimer;
            frames = 0; titleTimer = 0;
            std::string t = "Atomica | " + (app.stage == 4 ? app.orbitals.title() : app.stages2d[app.stage - 1]->title());
            char f[32]; std::snprintf(f, sizeof f, " | %.0f fps", fps);
            glfwSetWindowTitle(app.win, (t + f).c_str());
        }
        if (app.dumpFrames > 0 && ++shown >= app.dumpFrames) break;
    }

    app.orbitals.shutdown();
    glfwDestroyWindow(app.win);
    glfwTerminate();
    return 0;
}
