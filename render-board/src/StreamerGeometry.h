#pragma once
#include "CubeCore.h"

#include <math.h>

namespace StreamerGeometry {
    inline float getThicknessFromVolume(float volPercent) {
        float planeArea = (float)(RNDR_X * RNDR_Y);
        float targetArea = planeArea * volPercent;
        return sqrtf(targetArea) / 2.0f;
    }
}
