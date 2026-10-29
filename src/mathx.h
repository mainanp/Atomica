// mathx.h — the only linear algebra the project needs, written by hand.
// Matrices are COLUMN-MAJOR (OpenGL convention): element (row r, col c) is m[c*4 + r].
// A point is transformed as  p' = M * p  (column vector on the right),
// so  MVP = Projection * View * Model  applies Model first.
#pragma once
#include <cmath>

struct Mat4 {
    float m[16];

    static Mat4 identity() {
        Mat4 r{};
        r.m[0] = r.m[5] = r.m[10] = r.m[15] = 1.0f;
        return r;
    }

    // C = A * B   (apply B first, then A)
    friend Mat4 operator*(const Mat4& A, const Mat4& B) {
        Mat4 C{};
        for (int c = 0; c < 4; ++c)
            for (int r = 0; r < 4; ++r) {
                float s = 0.0f;
                for (int k = 0; k < 4; ++k) s += A.m[k * 4 + r] * B.m[c * 4 + k];
                C.m[c * 4 + r] = s;
            }
        return C;
    }

    static Mat4 translate(float x, float y, float z) {
        Mat4 r = identity();
        r.m[12] = x; r.m[13] = y; r.m[14] = z;
        return r;
    }

    static Mat4 scale(float x, float y, float z) {
        Mat4 r = identity();
        r.m[0] = x; r.m[5] = y; r.m[10] = z;
        return r;
    }

    // rotation about X:  [1 0 0; 0 c -s; 0 s c]
    static Mat4 rotateX(float a) {
        Mat4 r = identity();
        float c = std::cos(a), s = std::sin(a);
        r.m[5] = c;  r.m[9]  = -s;
        r.m[6] = s;  r.m[10] = c;
        return r;
    }

    // rotation about Y:  [c 0 s; 0 1 0; -s 0 c]
    static Mat4 rotateY(float a) {
        Mat4 r = identity();
        float c = std::cos(a), s = std::sin(a);
        r.m[0] = c;  r.m[8]  = s;
        r.m[2] = -s; r.m[10] = c;
        return r;
    }

    // Right-handed perspective, clip-space z in [-1, 1]  (same as gluPerspective)
    static Mat4 perspective(float fovyRad, float aspect, float zNear, float zFar) {
        Mat4 r{};
        float f = 1.0f / std::tan(fovyRad * 0.5f);
        r.m[0]  = f / aspect;
        r.m[5]  = f;
        r.m[10] = (zFar + zNear) / (zNear - zFar);
        r.m[11] = -1.0f;                       // w' = -z  -> perspective divide
        r.m[14] = (2.0f * zFar * zNear) / (zNear - zFar);
        return r;
    }
};
