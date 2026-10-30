#pragma once

#include "CubeCore.h"
#include "TextureEngine.h"
#include "UARTReceiver.h"

// Static animation collection, like Streamers and Surfaces. Implementations
// are grouped in Audio.cpp; callers do not need to construct an Audio object.
// Future volume/frequency/kick routines can be added as independent methods.
class Audio {
  public:
    static void run ();
    static void renderIcon (TextureState &texture);

    // Current default effect. Each invocation starts its animation state anew.
    // It runs until the existing CubeSystem checkpoint exits the active app.
    static void animatePuddles ();

    // General audio diagnostics; never consumes a kick or a button event.
    // Call regularly to drain its bounded USB queue. Internal static buffers
    // belong only to this method and survive an app's nonlocal exit safely.
    static void debug ();
    static void debug (const RemoteInputs::Audio &audio);
};
