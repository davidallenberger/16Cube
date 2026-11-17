#pragma once
#include "AppMemory.h"
#include "TestDemo.h"
#include "CubeCore.h"
#include "GeometryEngine.h"
#include "Explosions.h"
#include "TextureEngine.h"

class Planes {
private:
    
    static inline uint8_t getDimSize(uint8_t axis) {
        if (axis == 0) return RNDR_X;
        if (axis == 1) return RNDR_Y;
        return RNDR_Z;
    }

    
    static void drawPlane(uint8_t axis, uint8_t idx, const TextureState& tex) {
        GeometryEngine::paintPlane(axis, idx, [&tex](uint8_t x, uint8_t y, uint8_t z) {
            return tex.getColor(x, y, z);
        });
    }

    static void sweepAxisWithStatics(uint8_t axis, uint8_t startIdx, uint8_t endIdx, uint8_t xIdx, uint8_t yIdx, uint8_t zIdx, TextureState& tX, TextureState& tY, TextureState& tZ, uint32_t speedMs) {
        int step = (endIdx >= startIdx) ? 1 : -1;
        uint32_t lastFrame = millis();
        for (int i = (int)startIdx; i != (int)endIdx + step; i += step) {
            uint32_t now = millis(); uint32_t deltaMs = now - lastFrame; lastFrame = now;
            clearAll();
            if (axis != 0) drawPlane(0, xIdx, tX);
            if (axis != 1) drawPlane(1, yIdx, tY);
            if (axis != 2) drawPlane(2, zIdx, tZ);
            if (axis == 0) drawPlane(0, i, tX); else if (axis == 1) drawPlane(1, i, tY); else drawPlane(2, i, tZ);
            showCube();
            tX.advance(deltaMs); tY.advance(deltaMs); tZ.advance(deltaMs);
            if (speedMs > 0) delay(speedMs); yield();
        }
    }

    

public:
    static void animatePlaneTrails(uint32_t durationMs, uint32_t stepMs = 20) {
        constexpr uint8_t dimensions[] = {RNDR_X, RNDR_Y, RNDR_Z};
        constexpr uint8_t trailFade = 24;
        const uint8_t sweepAxis = random8(3);
        const uint8_t firstLaneAxis = (sweepAxis + 1) % 3;
        const uint8_t secondLaneAxis = (sweepAxis + 2) % 3;
        const uint16_t width = dimensions[firstLaneAxis];
        const uint16_t footprint = width * dimensions[secondLaneAxis];
        const uint16_t laneCount = (footprint + 1) / 3;
        const uint16_t travelLimit = (dimensions[sweepAxis] - 1) * 256;

        struct Lane {
            uint16_t phase;
            uint8_t first;
            uint8_t second;
            uint8_t bpm;
        };
        Lane lanes[(CUBE_SIZE * CUBE_SIZE + 1) / 3];
        uint16_t selected = 0;
        for (uint16_t cell = 0; cell < footprint && selected < laneCount; ++cell) {
            // Sample without replacement so exactly one third of the lanes
            // are selected, rounded to the nearest whole lane.
            if (random16(footprint - cell) >= laneCount - selected) continue;
            // Start near the same wall, then let the different rates spread
            // the matrix through the cube.
            lanes[selected++] = {
                uint16_t(40960u + random16(16384)),
                uint8_t(cell % width), uint8_t(cell / width), random8(8, 33)
            };
        }

        const CRGBPalette16 palette = RainbowColors_p;
        const uint32_t started = millis();
        clearAll();
        while (TestDemo::routineTimeRemaining(uint32_t(millis() - started), durationMs)) {
            fadeAll(trailFade);
            const CRGB color = ColorFromPalette(palette, beatsin8(12, 0, 255, started));
            for (uint16_t i = 0; i < laneCount; ++i) {
                const Lane &lane = lanes[i];
                uint16_t position[3];
                position[sweepAxis] = beatsin16(lane.bpm, 0, travelLimit, started, lane.phase);
                position[firstLaneAxis] = lane.first * 256;
                position[secondLaneAxis] = lane.second * 256;
                GeometryEngine::drawWuVoxel(position[0], position[1], position[2], color);
            }
            showCube();
            delay(stepMs);
        }
    }

    static void animateSmashingPlanes(uint32_t durationMs) {
        constexpr uint32_t travelMs = 900;
        constexpr uint32_t soloTrips = 4;
        constexpr uint32_t pairedTrips = 4;
        constexpr uint32_t splitMs = soloTrips * travelMs + travelMs / 2;
        constexpr uint32_t collisionMs = splitMs + pairedTrips * travelMs;
        constexpr uint32_t stepMs = 16;
        uint8_t axis = random8(3);
        const uint32_t started = millis();
        while (TestDemo::routineTimeRemaining(uint32_t(millis() - started), durationMs)) {
            const TexturePlan firstPlan = PaletteUtils::getRandomTexturePlan(false, true);
            const TexturePlan secondPlan = PaletteUtils::getRandomTexturePlan(false, true);
            PaletteUtils::logTexture("Smashing Planes", firstPlan, 1);
            PaletteUtils::logTexture("Smashing Planes", secondPlan, 2);
            TextureState firstTexture(firstPlan.sMode, firstPlan.palette, 1.0f);
            TextureState secondTexture(secondPlan.sMode, secondPlan.palette, 1.0f);
            const uint8_t layers = getDimSize(axis);
            const float edge = layers - 1;
            const bool reverse = random8(2);
            const uint32_t launch = millis();
            uint32_t previousFrame = launch;
            // Finish a launched sequence, including the burst, before handing
            // playback back to the deck.
            while (true) {
                const uint32_t now = millis();
                const uint32_t deltaMs = now - previousFrame;
                previousFrame = now;
                const uint32_t elapsed = min(uint32_t(now - launch), collisionMs);
                firstTexture.advance(deltaMs);
                secondTexture.advance(deltaMs);
                const float trips = float(elapsed) / travelMs;
                const float sweep = edge * (1.0f - fabsf(1.0f - fmodf(trips, 2.0f)));
                const float first = reverse ? edge - sweep : sweep;
                const float second = edge - first;
                const bool split = elapsed >= splitMs;
                const bool detonate = elapsed == collisionMs;
                const uint8_t reveal = split
                    ? uint8_t(min(255u, (elapsed - splitMs) * 255u / 100u)) : 0;

                clearAll();
                for (uint8_t layer = 0; layer < layers; ++layer) {
                    // Split each sheet's light between adjacent layers so
                    // the planes glide through the midpoint and wall bounces.
                    const float a = max(0.0f, 1.0f - fabsf(layer - first));
                    const float b = split ? max(0.0f, 1.0f - fabsf(layer - second)) : 0.0f;
                    const float strength = max(a, b);
                    if (strength <= 0.0f) continue;
                    const uint8_t brightness = uint8_t(255.0f * strength);
                    const uint8_t overlap = elapsed >= splitMs + travelMs / 2
                        ? uint8_t(255.0f * min(a, b) / strength) : 0;
                    GeometryEngine::paintPlane(axis, layer, [&](uint8_t x, uint8_t y, uint8_t z) {
                        CRGB color;
                        if (b == 0.0f) {
                            color = firstTexture.getColor(x, y, z);
                        } else if (a == 0.0f && reveal == 255) {
                            color = secondTexture.getColor(x, y, z);
                        } else {
                            const CRGB firstColor = firstTexture.getColor(x, y, z);
                            const CRGB secondColor = blend(firstColor, secondTexture.getColor(x, y, z), reveal);
                            color = a >= b ? firstColor : secondColor;
                            if (overlap && !detonate) {
                                // Invert the shared hue at the crossing while
                                // keeping both textures visible on its edges.
                                const CRGB shared = blend(firstColor, secondColor, 128);
                                const CRGB inverse(255 - shared.r, 255 - shared.g, 255 - shared.b);
                                color = blend(color, inverse, overlap);
                            }
                        }
                        if (detonate) color = blend(color, CRGB::White, 192);
                        color.nscale8(brightness);
                        return color;
                    });
                }
                showCube();
                if (detonate) break;
                delay(stepMs);
            }
            Explosions::animateSmashingPlanesBurst(axis, stepMs);
            axis = (axis + 1 + random8(2)) % 3;
        }
    }

    static void animatePassingPlanes(uint32_t durationMs, uint32_t stepMs = 24) {
        // The overlap uses a prescribed mixed hue. Additive LED blending
        // alone cannot produce paint-like results such as yellow + blue = green.
        // Match the boot test colors for purple, green, orange and pink.
        const CRGB recipes[][3] = {
            {CRGB(0xA00000), CRGB(0x0000A0), CRGB(CRGB::DarkViolet)}, // Dark red + dark blue = dark purple
            {CRGB(0xFFFF00), CRGB(0x0000A0), CRGB(CRGB::Green)}, // Yellow + dark blue = green
            {CRGB(0xFF0000), CRGB(0xFFFF00), CRGB(CRGB::OrangeRed)}, // Red + yellow = orange
            {CRGB(0x0000FF), CRGB(0xFFFFFF), CRGB(CRGB::Cyan)}, // Blue + white = cyan
            {CRGB(0xFF0000), CRGB(0xFFFFFF), CRGB(CRGB::DeepPink)}, // Red + white = pink
            {CRGB(0x0000FF), CRGB(0x00FF00), CRGB(0x00B090)}, // Blue + green = teal
        };
        // Pick once so the mix stays consistent throughout the run.
        const auto &recipe = recipes[random8(sizeof(recipes) / sizeof(recipes[0]))];
#if CUBINA_LOG_ANIMATION_COLORS
        Serial.printf("Colors | Passing Planes | #%02X%02X%02X + #%02X%02X%02X | overlap #%02X%02X%02X\n",
                      recipe[0].r, recipe[0].g, recipe[0].b, recipe[1].r, recipe[1].g, recipe[1].b,
                      recipe[2].r, recipe[2].g, recipe[2].b);
#endif
        constexpr uint8_t sheetPeak = 210;
        const uint32_t started = millis();
        while (TestDemo::routineTimeRemaining(uint32_t(millis() - started), durationMs)) {
            const uint32_t elapsed = uint32_t(millis() - started);
            const float phase = TWO_PI * (elapsed % 3200) / 3200.0f;
            const auto pulse = [](float angle) -> uint8_t {
                // A soft crest with a true dark gap between consecutive sheets.
                const float v = max(0.0f, cosf(angle));
                return uint8_t(sheetPeak * v * v);
            };
            uint8_t across[RNDR_X], rising[RNDR_Z];
            for (int x = 0; x < RNDR_X; ++x) {
                across[x] = pulse(2.0f * TWO_PI * x / RNDR_X - phase);
            }
            for (int z = 0; z < RNDR_Z; ++z) {
                rising[z] = pulse(2.0f * TWO_PI * z / RNDR_Z - phase - PI / 2.0f);
            }

            // Both sheets contribute light at the crossing. Taper the boost
            // as the brighter sheet approaches full output.
            for (int z = 0; z < RNDR_Z; ++z) {
                for (int x = 0; x < RNDR_X; ++x) {
                    const uint8_t overlap = min(across[x], rising[z]);
                    const uint8_t stronger = max(across[x], rising[z]);
                    // Close the remaining brightness gap faster on faint overlaps.
                    constexpr uint32_t peakCubed = uint32_t(sheetPeak) * sheetPeak * sheetPeak;
                    const uint32_t remaining = sheetPeak - overlap;
                    const uint32_t boost = peakCubed - remaining * remaining * remaining;
                    const uint8_t brightness =
                        stronger + (255u - stronger) * boost / peakCubed;
                    // Give the mixed hue more weight on the shoulders while
                    // preserving a smooth transition from either sheet.
                    const unsigned ratio = stronger ? 255u * overlap / stronger : 0u;
                    const uint8_t mix = ratio * (510u - ratio) / 255u;
                    CRGB color = blend(across[x] >= rising[z] ? recipe[0] : recipe[1],
                                       recipe[2], mix);
                    color.nscale8(brightness);
                    for (int y = 0; y < RNDR_Y; ++y) setVoxel(x, y, z, color);
                }
            }
            showCube();
            delay(stepMs);
        }
    }

    static void animateWavingCell(uint32_t durationMs, uint32_t stepMs = 24) {
        TextureState texture(MODE_WAVING_CELL, RainbowColors_p);
        const uint32_t started = millis();
        uint32_t previous = started;
        while (TestDemo::routineTimeRemaining(uint32_t(millis() - started), durationMs)) {
            const uint32_t now = millis();
            texture.advance(now - previous);
            previous = now;
            for (uint8_t y = 0; y < RNDR_Y; ++y) drawPlane(1, y, texture);
            showCube();
            delay(stepMs);
        }
    }

    static void animateSoapTexture(uint32_t durationMs, uint32_t stepMs = 24) {
        static_assert(SoapTextureState::ScratchBytes <= SHARED_APP_MEMORY_BYTES,
                      "The Soap field must fit the animation arena");
        const auto palette = PaletteUtils::getSmartPalette(MODE_SOAP);
        SoapTextureState soap(palette, SharedAppMemory, uint32_t(random16()) << 12);
        const uint32_t started = millis();
        uint32_t previous = started;
        while (TestDemo::routineTimeRemaining(uint32_t(millis() - started), durationMs)) {
            const uint32_t now = millis();
            soap.advance(now - previous);
            previous = now;
            const auto colorAt = [&soap](uint8_t x, uint8_t y, uint8_t z) {
                return soap.getColor(x, y, z);
            };
            for (uint8_t y = 0; y < RNDR_Y; ++y) GeometryEngine::paintPlane(1, y, colorAt);
            showCube();
            delay(stepMs);
        }
    }

    //These animations only make sense with an automatic color selection done internally

    static void animateBounce(uint32_t durationMs, uint32_t speedMs) {
        TexturePlan    pX = PaletteUtils::getRandomTexturePlan(false, true);
        TexturePlan   pY = PaletteUtils::getRandomTexturePlan(false, true);
        TexturePlan   pZ = PaletteUtils::getRandomTexturePlan(false, true);
        PaletteUtils::logTexture("Plane Bounce X", pX);
        PaletteUtils::logTexture("Plane Bounce Y", pY);
        PaletteUtils::logTexture("Plane Bounce Z", pZ);
     
        TextureState tX(pX.sMode, pX.palette, 1.0f);
        TextureState tY(pY.sMode, pY.palette, 1.0f);
        TextureState tZ(pZ.sMode, pZ.palette, 1.0f);

        auto pickEdge = [](uint8_t axis) -> uint8_t { return (random8() & 1) ? (getDimSize(axis) - 1) : 0; };
        uint8_t xIdx = pickEdge(0), yIdx = pickEdge(1), zIdx = pickEdge(2);
        
        uint32_t startTime = millis();
        while (TestDemo::routineTimeRemaining(uint32_t(millis() - startTime), durationMs)) {
            uint8_t xTarget = (getDimSize(0) - 1) - xIdx; sweepAxisWithStatics(0, xIdx, xTarget, xIdx, yIdx, zIdx, tX, tY, tZ, speedMs); xIdx = xTarget;
            if (!TestDemo::routineTimeRemaining(uint32_t(millis() - startTime), durationMs)) break;
            
            uint8_t yTarget = (getDimSize(1) - 1) - yIdx; sweepAxisWithStatics(1, yIdx, yTarget, xIdx, yIdx, zIdx, tX, tY, tZ, speedMs); yIdx = yTarget;
            if (!TestDemo::routineTimeRemaining(uint32_t(millis() - startTime), durationMs)) break;
            
            uint8_t zTarget = (getDimSize(2) - 1) - zIdx; sweepAxisWithStatics(2, zIdx, zTarget, xIdx, yIdx, zIdx, tX, tY, tZ, speedMs); zIdx = zTarget;
        }
    }
   
      static void animateHingeDance(uint32_t durationMs, uint32_t speedMs) {
        uint8_t f_axis = random8(3); 
        uint8_t f_pos = (random8() & 1) ? (getDimSize(f_axis) - 1) : 0;
        //TexturePlan plan = {sMode, pal};
        
        uint32_t startTime = millis();
        while (TestDemo::routineTimeRemaining(uint32_t(millis() - startTime), durationMs)) {
            TexturePlan plan  = PaletteUtils::getRandomTexturePlan(false, true);
            PaletteUtils::logTexture("Hinge Dance", plan);
            TextureState tex(plan.sMode, plan.palette, 1.0f);
            
            uint8_t pin_axis;
            uint8_t a1 = (f_axis + 1) % 3, a2 = (f_axis + 2) % 3; pin_axis = (random8() & 1) ? a1 : a2; 
            uint8_t l_axis = 3 - (f_axis + pin_axis); 
            uint8_t max_f = getDimSize(f_axis) - 1, max_p = getDimSize(pin_axis) - 1, max_l = getDimSize(l_axis) - 1;
            uint8_t pin_pos = (random8() & 1) ? max_p : 0;
            int8_t dir_f = (f_pos == 0) ? 1 : -1, dir_p = (pin_pos == 0) ? 1 : -1;
            uint8_t arm_len = max(max_f, max_p); 
            uint32_t normalizedDelayMs = speedMs;
            if (speedMs > 0 && arm_len > 0) {
                uint8_t max_possible_arm = max(getDimSize(0), max(getDimSize(1), getDimSize(2))) - 1;
                normalizedDelayMs = (uint32_t)((float)speedMs * ((float)max_possible_arm / (float)arm_len));
            }
            uint32_t lastFrame = millis();
            for (uint8_t s = 0; s <= arm_len; s++) {
                uint32_t now = millis(); uint32_t deltaMs = now - lastFrame; lastFrame = now;
                clearAll();
                float alpha = (float)s * (PI / (2.0f * (float)arm_len)); float sin_a = sin(alpha), cos_a = cos(alpha);
                for (uint8_t u = 0; u <= max_l; u++) {
                    for (uint8_t v = 0; v <= arm_len; v++) {
                        int16_t cf = f_pos + dir_f * lroundf((float)v * sin_a), cp = pin_pos + dir_p * lroundf((float)v * cos_a);
                        uint8_t x, y, z;
                        if (f_axis == 0) x = constrain(cf, 0, max_f); else if (pin_axis == 0) x = constrain(cp, 0, max_p); else x = u;
                        if (f_axis == 1) y = constrain(cf, 0, max_f); else if (pin_axis == 1) y = constrain(cp, 0, max_p); else y = u;
                        if (f_axis == 2) z = constrain(cf, 0, max_f); else if (pin_axis == 2) z = constrain(cp, 0, max_p); else z = u;
                        setVoxel(x, y, z, tex.getColor(x, y, z));
                    }
                }
                showCube(); tex.advance(deltaMs); if (normalizedDelayMs > 0) delay(normalizedDelayMs); yield();
                if (!TestDemo::routineTimeRemaining(uint32_t(millis() - startTime), durationMs)) break;
            }
            f_axis = pin_axis; f_pos = pin_pos;
        }
    }    

    static void animateSplat(uint32_t durationMs, uint32_t speedMs=0) {    
        uint32_t startTime = millis();
        while (TestDemo::routineTimeRemaining(uint32_t(millis() - startTime), durationMs)) {
            TexturePlan plan= PaletteUtils::getRandomTexturePlan(false, true);
            PaletteUtils::logTexture("Plane Splat", plan);
            TextureState tex(plan.sMode, plan.palette, 1.0f);
            
            uint8_t axis = random8(3), max_pos = getDimSize(axis) - 1, start_pos = (random8() & 1) ? 0 : max_pos;
            uint8_t end_pos = max_pos - start_pos; int8_t dir = (start_pos == 0) ? 1 : -1;
            uint8_t dimU = (axis == 0) ? RNDR_Y : RNDR_X, dimV = (axis == 2) ? RNDR_Y : RNDR_Z;
            uint8_t travel_dist[16][16]; 
            for (uint8_t u = 0; u < dimU; u++) for (uint8_t v = 0; v < dimV; v++) travel_dist[u][v] = random8(getDimSize(axis));
            uint32_t lastFrame = millis();

            for (uint8_t step = 0; step <= max_pos; step++) {
                uint32_t now = millis(); uint32_t deltaMs = now - lastFrame; lastFrame = now;
                clearAll(); 
                for (uint8_t u = 0; u < dimU; u++) {
                    for (uint8_t v = 0; v < dimV; v++) {
                        uint8_t d = (step < travel_dist[u][v]) ? step : travel_dist[u][v], depth = start_pos + (dir * d), x, y, z; 
                        if (axis == 0) { x = depth; y = u; z = v; } else if (axis == 1) { x = u; y = depth; z = v; } else { x = u; y = v; z = depth; }
                        setVoxel(x, y, z, tex.getColor(x, y, z));
                    }
                }
                showCube(); tex.advance(deltaMs); if (speedMs > 0) delay(speedMs); yield(); 
                if (!TestDemo::routineTimeRemaining(uint32_t(millis() - startTime), durationMs)) break;
            }
            if (!TestDemo::routineTimeRemaining(uint32_t(millis() - startTime), durationMs)) break;

            for(int p = 0; p < 15; p++) { 
                uint32_t now = millis(); uint32_t deltaMs = now - lastFrame; lastFrame = now;
                clearAll(); 
                for (uint8_t u = 0; u < dimU; u++) {
                    for (uint8_t v = 0; v < dimV; v++) {
                        uint8_t depth = start_pos + (dir * travel_dist[u][v]), x, y, z;
                        if (axis == 0) { x = depth; y = u; z = v; } else if (axis == 1) { x = u; y = depth; z = v; } else { x = u; y = v; z = depth; }
                        setVoxel(x, y, z, tex.getColor(x, y, z));
                    }
                }
                showCube(); tex.advance(deltaMs); delay(max(speedMs, 20U)); 
                if (!TestDemo::routineTimeRemaining(uint32_t(millis() - startTime), durationMs)) break;
            }
            if (!TestDemo::routineTimeRemaining(uint32_t(millis() - startTime), durationMs)) break;

            for (uint8_t step = 0; step <= max_pos; step++) {
                uint32_t now = millis(); uint32_t deltaMs = now - lastFrame; lastFrame = now;
                clearAll(); 
                for (uint8_t u = 0; u < dimU; u++) {
                    for (uint8_t v = 0; v < dimV; v++) {
                        uint8_t sd = start_pos + (dir * travel_dist[u][v]); int16_t cd = sd + (dir * step);
                        if (dir > 0 && cd > end_pos) cd = end_pos; if (dir < 0 && cd < end_pos) cd = end_pos;
                        uint8_t x, y, z; if (axis == 0) { x = (uint8_t)cd; y = u; z = v; } else if (axis == 1) { x = u; y = (uint8_t)cd; z = v; } else { x = u; y = v; z = (uint8_t)cd; }
                        setVoxel(x, y, z, tex.getColor(x, y, z));
                    }
                }
                showCube(); tex.advance(deltaMs); if (speedMs > 0) delay(speedMs); yield(); 
                if (!TestDemo::routineTimeRemaining(uint32_t(millis() - startTime), durationMs)) break;
            }
            if (!TestDemo::routineTimeRemaining(uint32_t(millis() - startTime), durationMs)) break;

            for(int p = 0; p < 15; p++) { 
                uint32_t now = millis(); uint32_t deltaMs = now - lastFrame; lastFrame = now;
                clearAll(); 
                for (uint8_t u = 0; u < dimU; u++) {
                    for (uint8_t v = 0; v < dimV; v++) {
                        uint8_t x, y, z; if (axis == 0) { x = end_pos; y = u; z = v; } else if (axis == 1) { x = u; y = end_pos; z = v; } else { x = u; y = v; z = end_pos; }
                        setVoxel(x, y, z, tex.getColor(x, y, z));
                    }
                }
                showCube(); tex.advance(deltaMs); delay(max(speedMs, 20U)); 
                if (!TestDemo::routineTimeRemaining(uint32_t(millis() - startTime), durationMs)) break;
            }
        }
    }
};
