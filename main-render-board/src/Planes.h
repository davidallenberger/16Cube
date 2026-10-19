#pragma once
#include "CubeCore.h"
#include "TextureEngine.h"

class Planes {
private:
    
    static inline uint8_t getDimSize(uint8_t axis) {
        if (axis == 0) return RNDR_X;
        if (axis == 1) return RNDR_Y;
        return RNDR_Z;
    }

    
    static void drawPlane(uint8_t axis, uint8_t idx, const TextureState& tex) {
        if (idx >= getDimSize(axis)) return;
        uint8_t max_u = (axis == 0) ? RNDR_Y : RNDR_X; uint8_t max_v = (axis == 2) ? RNDR_Y : RNDR_Z;
        for (uint8_t u = 0; u < max_u; u++) {
            for (uint8_t v = 0; v < max_v; v++) {
                if (axis == 0) setVoxel(idx, u, v, tex.getColor(idx, u, v));
                else if (axis == 1) setVoxel(u, idx, v, tex.getColor(u, idx, v));
                else setVoxel(u, v, idx, tex.getColor(u, v, idx));
            }
        }
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
    //These animations only make sense with an automatic color selection done internally

    static void animateBounce(uint32_t durationMs, uint32_t speedMs) {
        TexturePlan    pX = PaletteUtils::getRandomTexturePlan();
        TexturePlan   pY = PaletteUtils::getRandomTexturePlan();
        TexturePlan   pZ = PaletteUtils::getRandomTexturePlan();
     
        TextureState tX(pX.sMode, pX.palette, 1.0f);
        TextureState tY(pY.sMode, pY.palette, 1.0f);
        TextureState tZ(pZ.sMode, pZ.palette, 1.0f);

        auto pickEdge = [](uint8_t axis) -> uint8_t { return (random8() & 1) ? (getDimSize(axis) - 1) : 0; };
        uint8_t xIdx = pickEdge(0), yIdx = pickEdge(1), zIdx = pickEdge(2);
        
        uint32_t startTime = millis();
        while (millis() - startTime < durationMs) {
            uint8_t xTarget = (getDimSize(0) - 1) - xIdx; sweepAxisWithStatics(0, xIdx, xTarget, xIdx, yIdx, zIdx, tX, tY, tZ, speedMs); xIdx = xTarget;
            if (millis() - startTime >= durationMs) break;
            
            uint8_t yTarget = (getDimSize(1) - 1) - yIdx; sweepAxisWithStatics(1, yIdx, yTarget, xIdx, yIdx, zIdx, tX, tY, tZ, speedMs); yIdx = yTarget;
            if (millis() - startTime >= durationMs) break;
            
            uint8_t zTarget = (getDimSize(2) - 1) - zIdx; sweepAxisWithStatics(2, zIdx, zTarget, xIdx, yIdx, zIdx, tX, tY, tZ, speedMs); zIdx = zTarget;
        }
    }
   
      static void animateHingeDance(uint32_t durationMs, uint32_t speedMs) {
        uint8_t f_axis = random8(3); 
        uint8_t f_pos = (random8() & 1) ? (getDimSize(f_axis) - 1) : 0;
        //TexturePlan plan = {sMode, pal};
        
        uint32_t startTime = millis();
        while (millis() - startTime < durationMs) {
            TexturePlan plan  = PaletteUtils::getRandomTexturePlan();
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
                if (millis() - startTime >= durationMs) break; 
            }
            f_axis = pin_axis; f_pos = pin_pos;
        }
    }    

    static void animateSplat(uint32_t durationMs, uint32_t speedMs=0) {    
        uint32_t startTime = millis();
        while (millis() - startTime < durationMs) {
            TexturePlan plan= PaletteUtils::getRandomTexturePlan();
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
                if (millis() - startTime >= durationMs) break;
            }
            if (millis() - startTime >= durationMs) break;

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
                if (millis() - startTime >= durationMs) break;
            }
            if (millis() - startTime >= durationMs) break;

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
                if (millis() - startTime >= durationMs) break;
            }
            if (millis() - startTime >= durationMs) break;

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
                if (millis() - startTime >= durationMs) break;
            }
        }
    }
};