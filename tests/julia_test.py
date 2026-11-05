#!/usr/bin/env python3
"""Execute the production Julia body and ordered selector on supported canvases."""
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
#include <vector>
using std::min;
using std::max;
constexpr int RNDR_X = CANVAS, RNDR_Y = CANVAS, RNDR_Z = 16;
constexpr float RNDR_CX = (RNDR_X-1)*0.5f, RNDR_CY = (RNDR_Y-1)*0.5f, RNDR_CZ = 7.5f;
using CRGB = uint32_t;
const uint32_t swatches[]={0xFF0000,0xFF8000,0xFFFF00,0x00FF00,0x0000FF,0x4B0082,0x9400D3};
unsigned trackedFrames[7]={}, firstColor=0;
unsigned long now=0;
unsigned frames=0, lit=0, minimumLit=99999, maximumLit=0;
unsigned planes[RNDR_Z]={};
bool dark[RNDR_X][RNDR_Y][RNDR_Z]={};
unsigned minClearRays=RNDR_X*RNDR_Z, maxClearRays=0;
bool paletteBins[7]={};
bool opening=false, validateOpening=true;
float suppliedClosure=0;
float closureInput() { return suppliedClosure; }
uint64_t hash=0, firstHash=0, colorHash=0;
std::vector<uint64_t> masks, colors;
unsigned long millis() { return now; }
void delay(unsigned long ms) { now+=ms; }
void yield() { ++now; }
uint8_t random8(unsigned) { assert(false && "Julia must not randomize colors"); return 0; }
struct SerialStub { template<class... Args> void printf(const char*, Args...) {} } Serial;
void clearAll() { lit=0; hash=0; colorHash=0; std::fill(planes,planes+RNDR_Z,0);
    for(auto &x:dark) for(auto &y:x) std::fill(y,y+RNDR_Z,true);
}
void setVoxel(unsigned x, unsigned y, unsigned z, uint32_t rgb) {
    assert(x<RNDR_X && y<RNDR_Y && z<RNDR_Z);
    unsigned color=0;
    while(color<7 && swatches[color]!=rgb) ++color;
    assert(color<7 || (frames*40)%6000>4000); // intermediate hues only in the 2000ms transition
    assert((rgb&0xff)==0 || (rgb&0xff00)==0 || (rgb&0xff0000)==0); // fully saturated
    if(color<7) paletteBins[color]=true;
    if (!lit) firstColor=color;
    colorHash=colorHash*31+rgb;
    dark[x][y][z]=false;
    ++lit;
    ++planes[z];
    hash=hash*31+(x+RNDR_X*(y+RNDR_Y*z))*255;
}
void showCube() {
    assert(lit>0 && lit<=RNDR_X*RNDR_Y*RNDR_Z*8/10);
    for(unsigned x: {unsigned(RNDR_X/2-1),unsigned(RNDR_X/2)}) {
        for(unsigned y=0;y<RNDR_Y;++y) {
            bool cap=false;
            for(unsigned z=0;z<RNDR_Z;++z) cap=cap || !dark[x][y][z];
            assert(cap); // never a completely dark central strand
        }
    }
    assert(planes[0]>0);
    assert(planes[RNDR_Z-1]>0);
    for(unsigned z=3;z<RNDR_Z-3;++z) {
        opening=opening || planes[z]<RNDR_X*RNDR_Y*3/4;
    }
    unsigned clear=0;
    for(unsigned x=0;x<RNDR_X;++x) for(unsigned z=0;z<RNDR_Z;++z) {
        bool allDark=true;
        for(unsigned y=0;y<RNDR_Y;++y) allDark=allDark && dark[x][y][z];
        clear+=allDark;
    }
    minClearRays=min(minClearRays,clear);
    maxClearRays=max(maxClearRays,clear);
    minimumLit=min(minimumLit,lit);
    maximumLit=max(maximumLit,lit);
    if (!frames && validateOpening) {
        firstHash=hash;
        for(unsigned x: {unsigned(RNDR_X/2-1),unsigned(RNDR_X/2)}) {
            for(unsigned y=0;y<RNDR_Y;++y) {
                unsigned total=0;
                for(unsigned z=0;z<RNDR_Z;++z) total+=!dark[x][y][z];
                assert(total==2 && !dark[x][y][0] && !dark[x][y][RNDR_Z-1]);
            }
        }
        assert(lit<RNDR_X*RNDR_Y*RNDR_Z*7/10); // at least 30% black initially
        assert(planes[RNDR_Z/2]<RNDR_X*RNDR_Y/2); // substantial central cavern
    }
    if(firstColor<7 && (frames*40)%6000<=4000) ++trackedFrames[firstColor];
    masks.push_back(hash);
    colors.push_back(colorHash);
    ++frames;
    if (frames==4) TestDemo::playbackHeld()=false;
}
struct Surfaces { __JULIA__ };
int main() {
    // Deliberately cross millis wrap and exercise Hold past the duration.
    now=0xfffffff0ul;
    TestDemo::playbackHeld()=true;
    Surfaces::animateJulia(80,40);
    assert(frames==4);
    // The fixed view does not introduce rotation between frames.
    frames=0;
    now=0;
    Surfaces::animateJulia(60000,40);
    assert(frames==1500);
    assert(maximumLit>RNDR_X*RNDR_Y*RNDR_Z*75/100); // closed end stays near 20% dark
    assert(minClearRays<RNDR_X*RNDR_Z*20/100);
    assert(maxClearRays>RNDR_X*RNDR_Z/5);
    for(bool visited:paletteBins) assert(visited);
    assert(maximumLit>minimumLit);
    assert(opening);
    frames=0; now=0; masks.clear(); colors.clear();
    std::fill(trackedFrames,trackedFrames+7,0);
    Surfaces::animateJulia(42000,40,true,false);
    for(unsigned dwell:trackedFrames) assert(dwell==101); // 4s hold plus the t=0 transition sample

    const auto cyclingMasks=masks;
    const auto cyclingColors=colors;
    assert(cyclingColors.front()!=cyclingColors.back());
    frames=0; now=0; masks.clear(); colors.clear();
    Surfaces::animateJulia(42000,40,false,false);
    assert(masks==cyclingMasks); // color cycling leaves all geometry untouched
    assert(colors.front()==colors.back()); // both switches off is stationary
    assert(colors!=cyclingColors);
    // Independent audio levels must respect the bounds at arbitrary contour
    // phases, rather than only when coupled to the timed cosine oscillator.
    validateOpening=false;
    for(unsigned level=0;level<=16;++level) {
        suppliedClosure=float(level)/16;
        frames=0; now=0;
        Surfaces::animateJulia(8000,40,true,true,closureInput);
    }
    puts("Julia: bounds, colored ceiling/floor and black opening, changing geometry, palette range, duration/Hold pass.");
}
'''

ORDER = r'''
#include "TestDemo.h"
#include <cassert>
using TestDemo::PlaybackOrder;
PlaybackOrder order=PlaybackOrder::Descending;
PlaybackOrder &TestDemo::animationOrder() { return order; }
using TestDemo::animationOrder;
bool animationSceneActive=true, animationSceneOrdered=true;
constexpr int NUM_SHAPES=2, TOTAL_CONCEPTS=5;
struct Rule { bool enabled; };
Rule SHAPE_RULES[]={{true},{false}};
Rule SCENE_RULES[]={{true},{false},{true}};
int seqConcept=-1;
int select() { __SELECT__ return seqConcept; }
namespace TestDemo { __STOP__ __ADVANCE__ }
int main() {
    assert(select()==4); // last slot/new Julia
    --seqConcept;
    assert(select()==2); // skip disabled
    --seqConcept;
    assert(select()==0);
    --seqConcept;
    assert(select()==4); // wrap
    assert(TestDemo::animationHoldUntilNext());
    assert(!TestDemo::routineTimeRemaining(100,10));
    TestDemo::Detail::animationDurationHeld()=true;
    assert(TestDemo::routineTimeRemaining(100,10));
    TestDemo::Detail::animationDurationHeld()=false;
    assert(!TestDemo::routineTimeRemaining(100,10));
    order=PlaybackOrder::Sequential;
    seqConcept=-1;
    assert(select()==0);
    ++seqConcept;
    assert(select()==2);
    order=PlaybackOrder::Descending;
    seqConcept=4;
    TestDemo::Detail::animationDurationHeld()=true;
    TestDemo::Detail::advanceAnimation();
    assert(seqConcept==3); // Next still moves once while held
    TestDemo::Detail::stopAnimation();
    assert(!TestDemo::Detail::animationDurationHeld());
    assert(!animationSceneActive);
}
'''

def main():
    code=function((SOURCE/'Streamers.h').read_text(),'static void animateJulia(')
    demo=(SOURCE/'TestDemo.cpp').read_text()
    start=demo.index('            const int direction =')
    end=demo.index('            concept = seqConcept;',start)
    with tempfile.TemporaryDirectory(prefix='cubina-julia-') as directory:
        folder=Path(directory)
        for canvas in (16,10):
            compile_and_run(folder,'julia_'+str(canvas),HARNESS.replace('__JULIA__',code),'-DCANVAS='+str(canvas))
        compile_and_run(folder,'descending',ORDER.replace('__SELECT__',demo[start:end]).replace('__STOP__',function(demo,'void Detail::stopAnimation()')).replace('__ADVANCE__',function(demo,'void Detail::advanceAnimation()')))
    print('Descending selection: last-first, disabled cards and wrap pass.')

if __name__=='__main__':
    main()
