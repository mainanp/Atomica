// glutil.h — OpenGL includes + small helpers.
// We link straight against Mesa's libGL, so core-profile functions are available
// without an extension loader (GLEW/GLAD) — one less dependency to install.
#pragma once
#define GL_GLEXT_PROTOTYPES 1
#define GLFW_INCLUDE_NONE
#include <GL/gl.h>
#include <GL/glext.h>
#include <GLFW/glfw3.h>

// Compile + link a vertex/fragment program. Returns 0 (and prints the log) on failure.
GLuint buildProgram(const char* vertexSrc, const char* fragmentSrc);
