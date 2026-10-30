#pragma once

#include "CubeCore.h"
#include "TextureEngine.h"

class Depth {
  public:
    static void run ();
    static void renderIcon (TextureState &texture);
    static void animateOccupancy ();
};
