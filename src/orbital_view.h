// orbital_view.h — Stage 4: GPU-rendered 3D electron-probability cloud.
#pragma once
#include <string>

#include "glutil.h"
#include "mathx.h"
#include "orbital.h"

class OrbitalView {
public:
    bool init();                                  // needs a current GL context
    void shutdown();

    void update(double dt);
    void draw(int fbW, int fbH);
    void key(int glfwKey);                        // arrows / PgUp / PgDn / etc.
    void mouseDrag(double dx, double dy);         // left-button drag: orbit camera
    void scroll(double dy);                       // zoom
    std::string title() const;
    const char* help() const;

private:
    void resample();
    void upload();

    OrbitalCloud cloud_;
    int n_ = 2, l_ = 1, m_ = 0;
    size_t count_ = 150000;
    bool phaseColors_ = false, autoRotate_ = true, showAxes_ = true;
    float yaw_ = 0.6f, pitch_ = 0.35f, dist_ = 3.2f, pointSize_ = 2.2f;
    double sampleMs_ = 0;

    GLuint vao_ = 0, vbo_ = 0, prog_ = 0;
    GLuint axesVao_ = 0, axesVbo_ = 0, axesProg_ = 0;
    GLint uMVP_ = -1, uSize_ = -1, uAlpha_ = -1, uPhase_ = -1, uDist_ = -1, uAxesMVP_ = -1;
};
