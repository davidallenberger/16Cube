#!/usr/bin/env python3
"""Exercise the production four-streamer loop with bounded host drawing spies."""
from pathlib import Path
import tempfile
from render_regression_test import function, compile_and_run, SOURCE

HARNESS = r'''
#include "TestDemo.h"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstdio>
using std::min;
using std::max;
constexpr int RNDR_X=CANVAS, RNDR_Y=CANVAS, RNDR_Z=16;
constexpr float RNDR_CX=(RNDR_X-1)*0.5f, RNDR_CY=(RNDR_Y-1)*0.5f;
using CRGBPalette16=int;
using ShaderMode=int;
struct CRGB {
 int r=0;
 CRGB &operator+=(CRGB c){r=min(255,r+c.r);return *this;}
 void nscale8(uint8_t v){r=r*v/255;}
};
struct TexturePlan {ShaderMode sMode; CRGBPalette16 palette;};
static unsigned plans=0;
struct PaletteUtils {
 static TexturePlan getRandomMovingTexturePlan(){return {0,int(++plans)+1};}
};
static float headX[4]={},headY[4]={},headN[4]={};
struct TextureState {
 float internalPhase=0;
 uint32_t phase=0;
 int palette;
 TextureState(int,int pal,float):palette(pal){}
 void initMath(){}
 CRGB getColor(int x,int y,int z)const {
  const int q=palette==1?0:palette-2;
  if(z==RNDR_Z-1){headX[q]+=x;headY[q]+=y;headN[q]+=1;}
  CRGB c;c.r=palette*40;return c;
 }
};
static uint32_t clockMs=0;
uint32_t millis(){return clockMs++;}
void delay(uint32_t dt){clockMs+=dt;}
void yield(){}
float constrain(float v,float low,float high){return min(high,max(low,v));}
static unsigned picker=1;
static unsigned redirects=0;
long random(int low,int high){assert(high>low);picker=picker*1664525u+1013904223u;return low+((picker>>8)%unsigned(high-low));}
static CRGB pixels[RNDR_X*RNDR_Y*RNDR_Z];
static unsigned writes=0,frames=0;
static float lastX[4]={},lastY[4]={};
static unsigned moved[4]={};
void clearAll(){for(auto &p:pixels)p=CRGB();writes=0;for(int q=0;q<4;++q){headX[q]=headY[q]=headN[q]=0;}}
CRGB getVoxel(int x,int y,int z){return pixels[x+RNDR_X*(y+RNDR_Y*z)];}
void setVoxel(int x,int y,int z,CRGB c){
 assert(x>=0&&x<RNDR_X&&y>=0&&y<RNDR_Y&&z>=0&&z<RNDR_Z);
 pixels[x+RNDR_X*(y+RNDR_Y*z)]=c;++writes;
}
void showCube(){
 assert(writes>0&&writes<=4*RNDR_X*RNDR_Y*RNDR_Z);
 for(int q=0;q<4;++q){
  assert(headN[q]>0);
  float cx=headX[q]/headN[q],cy=headY[q]/headN[q];
  if(frames>0&&(fabsf(cx-lastX[q])>0.01f||fabsf(cy-lastY[q])>0.01f))++moved[q];
  lastX[q]=cx;lastY[q]=cy;
 }
 ++frames;
}
class Streamers {
public:
__THICKNESS__
__STREAMER__
};
int main(){
 Streamers::animateVerticalStreamer(10000,25,0.1875f,1,0);
 assert(frames>1&&plans==4);
 for(unsigned n:moved)assert(n>10);
#ifdef CLUSTERED
 assert(redirects>0);
#endif
 assert(fabsf(Streamers::getThicknessFromVolume(0.1875f/4)-sqrtf(CANVAS*CANVAS*0.1875f/4)/2)<0.0001f);
 printf("Four streamers: %dx%dx16, independent plans, bounds and work limit pass.\n",CANVAS,CANVAS);
}
'''

def main():
    text = (SOURCE / 'Streamers.h').read_text()
    code = HARNESS.replace('__THICKNESS__', function(text, 'static float getThicknessFromVolume('))
    loop = function(text, 'static void animateVerticalStreamer(')
    # Count redirections in the host copy, without adding firmware diagnostics.
    loop = loop.replace('if (redirect) {', 'if (redirect) { ++redirects;')
    code = code.replace('__STREAMER__', loop)
    with tempfile.TemporaryDirectory(prefix='cubina-streamers-') as temp:
        for canvas in (16, 10):
            compile_and_run(Path(temp), 'streamers_' + str(canvas), code, '-DCANVAS=' + str(canvas))
            # Start all four heads together to exercise collision recovery.
            clustered = code.replace('currentX[i] = (i & 1) ? RNDR_X * 0.75f : RNDR_X * 0.25f;', 'currentX[i] = RNDR_CX;')
            clustered = clustered.replace('currentY[i] = (i & 2) ? RNDR_Y * 0.75f : RNDR_Y * 0.25f;', 'currentY[i] = RNDR_CY;')
            compile_and_run(Path(temp), 'clustered_' + str(canvas), clustered, '-DCANVAS=' + str(canvas), '-DCLUSTERED')

if __name__ == '__main__':
    main()
