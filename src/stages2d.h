// stages2d.h — Stages 1-3: everything is drawn with our own rasteriser (raster.h).
#pragma once
#include <memory>
#include <string>

#include "raster.h"

class Stage2D {
public:
    virtual ~Stage2D() = default;
    virtual void update(double dt, int w, int h) = 0;
    virtual void draw(Raster& r) = 0;
    virtual void key(int glfwKey) = 0;           // called on key press / repeat
    virtual std::string title() const = 0;       // short status line for the window title
    virtual const char* help() const = 0;
};

std::unique_ptr<Stage2D> makePlanetaryStage();   // Stage 1: classical atom that collapses
std::unique_ptr<Stage2D> makeBohrStage();        // Stage 2: energy levels + photons
std::unique_ptr<Stage2D> makeWaveStage();        // Stage 3: electron as a standing wave
