#!/usr/bin/env python3
"""Exercise production fractals, recursive paths, impressions, arm motion, and deck wiring.

Host tests validate geometry/time/bounds, not ESP32 speed or visual acceptance.
The kernels and scene body are extracted verbatim, as in julia_test.py.
"""
from pathlib import Path
import os
import re
import subprocess
import tempfile
from render_regression_test import function

ROOT = Path(__file__).resolve().parents[1]
SRC = ROOT / 'render-board/src'

HARNESS = r'''
#include "TestDemo.h"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
using std::min; using std::max; using std::isfinite;
constexpr float PI=3.14159265358979323846f, TWO_PI=2*PI;
constexpr unsigned RNDR_X=CANVAS, RNDR_Y=CANVAS, RNDR_Z=16;
constexpr float RNDR_CX=(RNDR_X-1)*.5f, RNDR_CY=(RNDR_Y-1)*.5f, RNDR_CZ=7.5f;
template<class T> T constrain(T n,T lo,T hi) { return min(hi,max(lo,n)); }
struct CRGB {
    unsigned char r=0,g=0,b=0;
    CRGB() {}
    CRGB(int red,int green,int blue):r(red),g(green),b(blue) {}
    void nscale8(unsigned char v) { r=r*v/255; g=g*v/255; b=b*v/255; }
};
CRGB operator+(CRGB a,CRGB b) { return CRGB(min(255,int(a.r)+b.r),min(255,int(a.g)+b.g),min(255,int(a.b)+b.b)); }
struct CRGBPalette16 {};
CRGBPalette16 RainbowColors_p;
int16_t sin16(uint16_t phase) { return int16_t(lroundf(sinf(TWO_PI*phase/65536)*32767)); }
int16_t cos16(uint16_t phase) { return sin16(uint16_t(phase+16384)); }
CRGB blend(CRGB a,CRGB b,uint8_t weight) {
 return CRGB((a.r*(255-weight)+b.r*weight)/255,(a.g*(255-weight)+b.g*weight)/255,(a.b*(255-weight)+b.b*weight)/255);
}
constexpr int MODE_LINEAR_FLOW=0, MODE_DISTANCE_FIELD=1, LINEARBLEND=0;
CRGB ColorFromPalette(const CRGBPalette16&,unsigned char index,unsigned char value=255,int=LINEARBLEND) {
    CRGB c; c.r=value; c.g=index; c.b=255-index; return c;
}
struct TextureState {
    CRGBPalette16 palette;
    uint32_t phase=0;
    float internalPhase=0, speedMultiplier;
    int mode;
    TextureState(int m,CRGBPalette16 p,float speed):palette(p),speedMultiplier(speed),mode(m) {}
    void initMath() {}
    __ADVANCE__
    __COLOR__
};
unsigned lastRandomMaximum=0,largestRandomMaximum=0;
unsigned fakeRandom=0;
uint8_t random8(uint8_t maximum) { lastRandomMaximum=maximum;largestRandomMaximum=max(largestRandomMaximum,unsigned(maximum));return (fakeRandom++)%maximum; }
uint64_t nowMs=0;
uint32_t millis() { return uint32_t(nowMs); }
void delay(uint32_t ms) { nowMs+=ms; }
void yield() { ++nowMs; }
unsigned fibonacciPatternMask=0,fibonacciPatternLogs=0;
struct SerialStub {
 template<class... T> void printf(const char*,T...) {}
 template<class... T> void printf(const char* format,const char* name,T...) {
  if(std::strncmp(format,"%s | from=",10)==0) {
   fibonacciPatternMask|=std::strcmp(name,"Fibonacci Spiral")==0 ? 2u : 1u;
   ++fibonacciPatternLogs;
  }
 }
} Serial;
CRGB frame[RNDR_Z][RNDR_Y][RNDR_X];
unsigned frames=0,minLit=99999,maxLit=0,minFace=99999;
unsigned faceSeen[6]={};
unsigned minClear[3]={99999,99999,99999},maxClear[3]={};
uint64_t firstHash=0,lastHash=0,firstColors=0,lastColors=0;
bool geometryChanges=false,colorChanges=false,cavern=false;
unsigned stopAt=0;
bool branching=false;
void clearAll() { for(auto &plane:frame) for(auto &row:plane) for(auto &p:row) p=CRGB{}; }
void fadeAll(uint8_t amount) { for(auto &plane:frame) for(auto &row:plane) for(auto &p:row) p.nscale8(255-amount); }
void setVoxel(unsigned x,unsigned y,unsigned z,CRGB c) {
    assert(x<RNDR_X && y<RNDR_Y && z<RNDR_Z); frame[z][y][x]=c;
}
CRGB getVoxel(unsigned x,unsigned y,unsigned z) { assert(x<RNDR_X&&y<RNDR_Y&&z<RNDR_Z);return frame[z][y][x]; }
void showCube() {
    unsigned lit=0,faces[6]={}; uint64_t hash=0,colors=0;
    for(unsigned z=0;z<RNDR_Z;++z) for(unsigned y=0;y<RNDR_Y;++y) for(unsigned x=0;x<RNDR_X;++x) {
        const auto c=frame[z][y][x];
        const bool on=c.r||c.g||c.b;
        hash=hash*31+unsigned(on);
        colors=colors*31+c.r+257u*c.g+65537u*c.b;
        if(!on) continue;
        ++lit;
        if(x==0) ++faces[0]; if(x==RNDR_X-1) ++faces[1];
        if(y==0) ++faces[2]; if(y==RNDR_Y-1) ++faces[3];
        if(z==0) ++faces[4]; if(z==RNDR_Z-1) ++faces[5];
    }
    // Layer peeling deliberately leaves only the center point at reversal.
    assert(lit>(branching?0:RNDR_X*RNDR_Y*RNDR_Z/20));
    assert(lit<RNDR_X*RNDR_Y*RNDR_Z*9/10);
    minLit=min(minLit,lit); maxLit=max(maxLit,lit);
    for(unsigned axis=0;axis<3;++axis) {
        unsigned clear=0;
        const unsigned uSize=axis==0?RNDR_Y:RNDR_X;
        const unsigned vSize=axis==2?RNDR_Y:RNDR_Z;
        const unsigned depth=axis==0?RNDR_X:(axis==1?RNDR_Y:RNDR_Z);
        for(unsigned u=0;u<uSize;++u) for(unsigned v=0;v<vSize;++v) {
            bool empty=true;
            for(unsigned d=0;d<depth;++d) {
                const auto c=axis==0?frame[v][u][d]:(axis==1?frame[v][d][u]:frame[d][v][u]);
                empty &= !(c.r||c.g||c.b);
            }
            clear+=empty;
        }
        minClear[axis]=min(minClear[axis],clear);maxClear[axis]=max(maxClear[axis],clear);
    }
    for(unsigned f=0;f<6;++f) {
        faceSeen[f]=max(faceSeen[f],faces[f]);
        if(cavern) {
            const unsigned area=f<2?RNDR_Y*RNDR_Z:(f<4?RNDR_X*RNDR_Z:RNDR_X*RNDR_Y);
            if(!faces[f] || faces[f]==area) fprintf(stderr,"Cavern face %u lit=%u/%u at %llu ms, volume=%u\n",f,faces[f],area,(unsigned long long)nowMs,lit);
            assert(faces[f]>0 && faces[f]<area); // open but participating on all sides
            minFace=min(minFace,faces[f]);
        }
    }
    if(!frames) { firstHash=hash; firstColors=colors; }
    else { geometryChanges|=hash!=firstHash; colorChanges|=colors!=firstColors; }
    lastHash=hash; lastColors=colors;
    ++frames;
    if(stopAt && frames==stopAt) TestDemo::Detail::animationDurationHeld()=false;
}
namespace GeometryEngine {
__GEOMETRY__
}
class Surfaces {
public:
__SETTINGS__
__SCENE__
__CAVERN__
__SCULPTURE__
__MOUSESETTINGS__
__MICE__
__PRIMESETTINGS__
__PRIMEDIRECTOR__
__FIBSETTINGS__
__FIBDIRECTOR__
__FIBENTRY__
};
class Swirls { public:
__PINWHEEL_CONSTANTS__
__PINWHEEL__
};
void reset(bool isCavern) {
    frames=0;minLit=99999;maxLit=0;minFace=99999;geometryChanges=colorChanges=false;
    cavern=isCavern;stopAt=0;branching=false;
    for(auto &f:faceSeen) f=0;
    for(unsigned axis=0;axis<3;++axis) { minClear[axis]=99999; maxClear[axis]=0; }
    TestDemo::playbackHeld()=false;
    TestDemo::Detail::animationDurationHeld()=false;
}
GeometryEngine::FractalSample referenceBulb(GeometryEngine::Point3 c) {
    auto p=c; float derivative=1,trap=100;
    for(unsigned step=0;step<5;++step) {
        const float r=sqrtf(p.x*p.x+p.y*p.y+p.z*p.z);
        if(r>2) return {.5f*logf(r)*r/derivative,sqrtf(trap)};
        trap=min(trap,r*r);
        const float theta=r>0?acosf(constrain(p.z/r,-1.0f,1.0f)):0;
        const float phi=atan2f(p.y,p.x);
        const float r8=powf(r,8);
        derivative=8*powf(r,7)*derivative+1;
        p={r8*sinf(8*theta)*cosf(8*phi)+c.x,r8*sinf(8*theta)*sinf(8*phi)+c.y,r8*cosf(8*theta)+c.z};
    }
    const float r=sqrtf(p.x*p.x+p.y*p.y+p.z*p.z);
    return {r>2?.5f*logf(r)*r/derivative:-1.0f,sqrtf(trap)};
}
int main() {
    // Fractal fields must remain finite even on poles/at the origin.
    using namespace GeometryEngine;
    // Crisp mouse samples occupy exactly one voxel, at full material color.
    clearAll();drawCrispVoxel({2.3f,3.2f,4.1f},CRGB(255,0,0));
    drawCrispVoxel({2.3f,3.2f,4.1f},CRGB(0,120,0));
    unsigned crispCount=0;
    for(auto &plane:frame) for(auto &row:plane) for(auto &p:row) crispCount+=bool(p.r||p.g||p.b);
    assert(crispCount==1 && frame[4][3][2].r==255 && frame[4][3][2].g==0);
    // The recursive walk covers actual cube edges, closes continuously, and
    // consumes physical arclength rather than one time slot per edge length.
    for(Point3 span: {Point3{15,15,15},Point3{9,9,15}}) {
        assert(RecursiveCubePath::levelsForSpan(span)==3);
        const Surfaces::RecursiveMouseSettings mouse;
        float minimumSpeed=1000,maximumSpeed=0;
        const float total=RecursiveCubePath::length(span);
        assert(total>RecursiveCubePath::length(span,2));
        auto previous=RecursiveCubePath::point(0,span);
        for(float d=.125f;d<total;d+=.125f) {
            const auto p=RecursiveCubePath::point(d,span);
            const float speed=mouse.speedAt(d,span);
            minimumSpeed=min(minimumSpeed,speed);maximumSpeed=max(maximumSpeed,speed);
            assert(speed>=mouse.outerSpeed && speed<=mouse.outerSpeed*mouse.centerSpeedMultiplier);
            assert(p.x>=0 && p.x<=span.x && p.y>=0 && p.y<=span.y && p.z>=0 && p.z<=span.z);
            const float travelled=fabsf(p.x-previous.x)+fabsf(p.y-previous.y)+fabsf(p.z-previous.z);
            assert(travelled<=.126f); // Includes reversals within a sampling interval.
            previous=p;
        }
        assert(fabsf(minimumSpeed-mouse.outerSpeed)<.001f && maximumSpeed>2.5f*mouse.outerSpeed);
        float coarse=0,fine=0;
        for(unsigned i=0;i<100;++i) coarse=mouse.advance(coarse,span,40);
        for(unsigned i=0;i<400;++i) fine=mouse.advance(fine,span,10);
        assert(fabsf(coarse-fine)<.001f && coarse>mouse.outerSpeed*4);
        const auto closed=RecursiveCubePath::point(total,span);
        assert(fabsf(closed.x)+fabsf(closed.y)+fabsf(closed.z)<.001f);
        const auto before=RecursiveCubePath::point(-.125f,span);
        assert(fabsf(before.x)+fabsf(before.y)+fabsf(before.z)<.126f);
    }
    for(Point3 p: {Point3{0,0,0},Point3{0,0,1},Point3{0,0,-1},Point3{2,3,4},Point3{.2f,.3f,.4f}}) {
        for(auto f: {mandelboxField(p),mandelbulbField(p)}) assert(isfinite(f.distance)&&isfinite(f.orbitTrap));
        const auto fast=mandelbulbField(p),reference=referenceBulb(p);
        assert(fabsf(fast.distance-reference.distance)<.002f);
        assert(fabsf(fast.orbitTrap-reference.orbitTrap)<.002f);
        const auto a=mandelboxField(p),b=mandelboxField({p.z,p.x,p.y});
        assert(fabsf(a.distance-b.distance)<.0001f);
    }
    for(unsigned look=0;look<2;++look) {
        reset(look==0); nowMs=0;
        Surfaces::FractalSettings s; s.frameMs=1000;
        Surfaces::animateFractalVolume(90000,look==0?Surfaces::FractalLook::Cavern:Surfaces::FractalLook::Sculpture,s);
        assert(frames==90 && geometryChanges && colorChanges);
        printf("%s %ux%ux%u: lit=%u..%u, minimum lit face=%u, 90 distinct-time frames pass.\n",
               look==0?"Cavern":"Sculpture",RNDR_X,RNDR_Y,RNDR_Z,minLit,maxLit,cavern?minFace:0);
        printf("Clear rays X/Y/Z=%u..%u / %u..%u / %u..%u\n",minClear[0],maxClear[0],minClear[1],maxClear[1],minClear[2],maxClear[2]);
        // Identical renders through millis wrap, including independent material phase.
        reset(look==0);nowMs=0;s.frameMs=40;
        Surfaces::animateFractalVolume(1000,look==0?Surfaces::FractalLook::Cavern:Surfaces::FractalLook::Sculpture,s);
        const auto normal=lastHash;
        reset(look==0);nowMs=UINT32_MAX-500u;
        Surfaces::animateFractalVolume(1000,look==0?Surfaces::FractalLook::Cavern:Surfaces::FractalLook::Sculpture,s);
        assert(lastHash==normal && frames==25);
    }
    // The cavern multiplier speeds geometry AND color by exactly the same
    // elapsed-time factor; it must not change their relative phase.
    reset(true);nowMs=0;
    Surfaces::FractalSettings slow;slow.cavernSpeed=1;slow.frameMs=120;
    Surfaces::animateFractalVolume(2400,Surfaces::FractalLook::Cavern,slow);
    const auto slowGeometry=lastHash,slowColors=lastColors;
    reset(true);nowMs=0;
    Surfaces::FractalSettings fast;fast.frameMs=40;
    Surfaces::animateFractalVolume(800,Surfaces::FractalLook::Cavern,fast);
    assert(frames==20 && lastHash==slowGeometry && lastColors==slowColors);
    for(unsigned look=2;look<3;++look) {
        reset(false);branching=false;nowMs=0;
        Surfaces::FractalSettings s;s.frameMs=1000;
        Surfaces::animateFractalVolume(90000,static_cast<Surfaces::FractalLook>(look),s);
        assert(frames==90 && geometryChanges && colorChanges);
        printf("Scaffold %u %ux%ux%u: lit=%u..%u; bounded, changing geometry/colors pass.\n",
               look,RNDR_X,RNDR_Y,RNDR_Z,minLit,maxLit);
    }
    // Retained legacy scale/tumble director still honors its optional hold.
    // The played mouse audition bypasses this director and starts immediately.
    for(unsigned look=2;look<3;++look) {
        Surfaces::FractalSettings s;s.frameMs=1000;
        reset(false);nowMs=0;
        Surfaces::animateFractalVolume(5000,static_cast<Surfaces::FractalLook>(look),s);
        assert(frames==5 && !geometryChanges && colorChanges);
        const auto initial=firstHash;
        reset(false);nowMs=0;
        Surfaces::animateFractalVolume(7000,static_cast<Surfaces::FractalLook>(look),s);
        assert(frames==7 && firstHash==initial && geometryChanges);
    }
    // The played mouse director starts immediately with eight crisp heads.
    reset(false);branching=true;nowMs=0;
    Surfaces::animateRecursiveCube(40);
    assert(frames==1 && minLit==8 && maxLit==8);
    // Exercise the real director and deeper paths on both canvas sizes.
    reset(false);branching=true;nowMs=0;
    Surfaces::animateRecursiveCube(9000);
    assert(frames==225 && geometryChanges && colorChanges);
    // The repeated-scale fields must not jump when the scale phase wraps.
    {
        const float units=1.0f/min(RNDR_CX,min(RNDR_CY,RNDR_CZ));
        const RecursiveScaffoldField before(.999999f,units),after(0,units);
        for(Point3 p: {Point3{.1f,.2f,.3f},Point3{1,0,0},Point3{.8f,.8f,.8f},Point3{0,.5f,.5f}}) {
            assert(fabsf(before(p).distance-after(p).distance)<.0001f);
        }
    }
    clearAll();
    const TextureState paint(MODE_LINEAR_FLOW,RainbowColors_p,1);
    // Both path renderers stay on one plane; normal-axis antialiasing must
    // not leave a stack of retained patterns. Exercise every selected face.
    for(unsigned face=0;face<6;++face) for(bool spiral: {false,true}) {
        clearAll();
        const unsigned axis=face/2;
        PlaneFrame3D plane;
        plane.center={RNDR_CX,RNDR_CY,RNDR_CZ};
        if(axis==0) { plane.center.x=2;plane.u={0,1,0};plane.v={0,0,1}; }
        else if(axis==1) { plane.center.y=2;plane.u={1,0,0};plane.v={0,0,1}; }
        else { plane.center.z=2;plane.u={1,0,0};plane.v={0,1,0}; }
        const float halfU=axis==0?RNDR_CY:RNDR_CX,halfV=axis==2?RNDR_CY:RNDR_CZ;
        const unsigned count=spiral?drawFibonacciSpiral3D(plane,halfU,halfV,paint):drawGoldenScaffoldPlane(plane,halfU,halfV,paint);
        assert(count>=20);
        unsigned lit=0;
        for(unsigned z=0;z<RNDR_Z;++z) for(unsigned y=0;y<RNDR_Y;++y) for(unsigned x=0;x<RNDR_X;++x) {
            const auto p=frame[z][y][x];
            if(!(p.r||p.g||p.b)) continue;
            ++lit;assert((axis==0?x:axis==1?y:z)==2);
        }
        assert(lit>10);
    }
    for(bool spiral: {false,true}) {
        reset(false);branching=true;nowMs=0;largestRandomMaximum=0;
        Surfaces::FibonacciSweepSettings settings;settings.frameMs=200;
        Surfaces::animateFibonacciSweep(16000,spiral,settings);
        assert(frames==80 && geometryChanges && colorChanges && largestRandomMaximum==4);
    }
    // At an integral plane position, trails occupy exactly 4/8 planes,
    // including the leader, with no retained distant sheets or +/-Z source.
    for(bool spiral: {false,true}) {
        reset(false);branching=true;nowMs=0;fakeRandom=0;
        Surfaces::FibonacciDirectionCycle preview;
        const unsigned axis=preview.nextFace()/2;fakeRandom=0;
        Surfaces::FibonacciSweepSettings settings;settings.frameMs=800;
        settings.sweepMs=1500;settings.edgeHoldMs=0;
        Surfaces::animateFibonacciSweep(801,spiral,settings);
        const unsigned trail=spiral?8:4;
        unsigned planes=0;
        for(unsigned x=0;x<RNDR_X;++x) {
            unsigned maximum=0;
            for(unsigned z=0;z<RNDR_Z;++z) for(unsigned y=0;y<RNDR_Y;++y)
                maximum=max(maximum,unsigned(axis==0 ? frame[z][y][x].r : frame[z][x][y].r));
            if(!maximum) continue;
            ++planes;
            // Head distance is 8 voxels for the 16 cube, 4.8 on desktop.
            if(RNDR_X==16) {
                assert(x<=8 && x>8-trail);
                assert(maximum<=255*(trail-(8-x))/trail);
            }
        }
        if(RNDR_X==16) assert(planes==trail);
    }
    // Shuffled directions visit all four side faces once per bag and never
    // repeat at bag boundaries, even if the shuffle wants the previous face.
    Surfaces::FibonacciDirectionCycle directions;
    unsigned previousFace=99;
    for(unsigned bag=0;bag<16;++bag) {
        unsigned mask=0;
        for(unsigned i=0;i<4;++i) {
            const unsigned face=directions.nextFace();
            assert(face<4 && face!=previousFace && !(mask&(1u<<face)));
            mask|=1u<<face;previousFace=face;
        }
        assert(mask==15);
    }
    // A tiny move across each of the first three trailing-plane entrances
    // must not switch a plane from off to its full assigned brightness.
    for(bool spiral: {false,true}) for(unsigned crossing=1;crossing<=3;++crossing) {
        uint8_t before[RNDR_Z][RNDR_Y][RNDR_X];
        Surfaces::FibonacciSweepSettings settings;
        settings.sweepMs=(RNDR_X-1)*200;settings.edgeHoldMs=0;
        settings.frameMs=crossing*200-2;
        reset(false);branching=true;nowMs=0;fakeRandom=0;
        Surfaces::animateFibonacciSweep(settings.frameMs+1,spiral,settings);
        for(unsigned z=0;z<RNDR_Z;++z) for(unsigned y=0;y<RNDR_Y;++y) for(unsigned x=0;x<RNDR_X;++x)
            before[z][y][x]=frame[z][y][x].r;
        settings.frameMs=crossing*200+2;
        reset(false);branching=true;nowMs=0;fakeRandom=0;
        Surfaces::animateFibonacciSweep(settings.frameMs+1,spiral,settings);
        unsigned jump=0;
        for(unsigned z=0;z<RNDR_Z;++z) for(unsigned y=0;y<RNDR_Y;++y) for(unsigned x=0;x<RNDR_X;++x)
            jump=max(jump,unsigned(abs(int(frame[z][y][x].r)-before[z][y][x])));
        assert(jump<80);
        printf("Fibonacci %u crossing %u %ux%ux%u: maximum entry brightness step=%u/255.\n",
               unsigned(spiral),crossing,RNDR_X,RNDR_Y,RNDR_Z,jump);
    }
    // The entrance impression must stay at its original angle and fade
    // monotonically after the leader leaves. Delayed moving copies fail this
    // contract because they repeatedly re-enter the same starting face.
    for(bool spiral: {false,true}) {
        fakeRandom=0;Surfaces::FibonacciDirectionCycle preview;
        const unsigned face=preview.nextFace(),axis=face/2;
        const unsigned entrance=face&1 ? RNDR_X-1 : 0;
        const unsigned trail=spiral?8:4;
        reset(false);branching=true;nowMs=0;fakeRandom=0;
        Surfaces::FibonacciSweepSettings initial;
        Surfaces::animateFibonacciSweep(1,spiral,initial);
        unsigned initialPeak=0;
        for(unsigned z=0;z<RNDR_Z;++z) for(unsigned u=0;u<RNDR_X;++u)
            initialPeak=max(initialPeak,unsigned(axis==0 ? frame[z][u][entrance].r : frame[z][entrance][u].r));
        assert(initialPeak>0);
        unsigned previousPeak=initialPeak;
        for(unsigned tenths=11;tenths<=trail*10+1;tenths+=5) {
            reset(false);branching=true;nowMs=0;fakeRandom=0;
            Surfaces::FibonacciSweepSettings settings;
            settings.sweepMs=(RNDR_X-1)*1000;settings.edgeHoldMs=0;
            settings.frameMs=tenths*100;
            Surfaces::animateFibonacciSweep(settings.frameMs+1,spiral,settings);
            unsigned peak=0;
            for(unsigned z=0;z<RNDR_Z;++z) for(unsigned u=0;u<RNDR_X;++u)
                peak=max(peak,unsigned(axis==0 ? frame[z][u][entrance].r : frame[z][entrance][u].r));
            const unsigned expected=unsigned(initialPeak*max(0.0f,1-float(tenths)/(10*trail)));
            assert(peak<=previousPeak && abs(int(peak)-int(expected))<=2);
            previousPeak=peak;
        }
        assert(previousPeak==0);
        printf("Fibonacci %u %ux%ux%u: fixed entrance impression fades once to zero.\n",
               unsigned(spiral),RNDR_X,RNDR_Y,RNDR_Z);
    }
    // Force each coin result, and verify the chosen look stays fixed across
    // three sweeps within one invocation of the combined deck routine.
    for(unsigned choice=0;choice<2;++choice) {
        reset(false);branching=true;nowMs=0;fakeRandom=choice;
        fibonacciPatternMask=fibonacciPatternLogs=0;
        Surfaces::animateFibonacci(17000);
        assert(fibonacciPatternLogs==3 && fibonacciPatternMask==(choice ? 2u : 1u));
    }
    for(unsigned study=0;study<2;++study) {
        reset(false);branching=true;nowMs=0;
        Surfaces::PrimeStudySettings s;s.frameMs=1000;
        Surfaces::animatePrimeStudy(61000,static_cast<Surfaces::PrimeStudy>(study),s);
        assert(frames==61 && geometryChanges && colorChanges);
        if(study==1) for(unsigned face=0;face<6;++face) assert(faceSeen[face]>0);
        printf("Prime study %u %ux%ux%u: live geometry/colors and bounded drawing pass.\n",
               study,RNDR_X,RNDR_Y,RNDR_Z);
    }
    reset(false);branching=true;nowMs=0;
    Surfaces::PrimeStudySettings weave;weave.frameMs=192500;weave.ribbonTumblePhaseRate=0;
    Surfaces::animatePrimeStudy(192501,Surfaces::PrimeStudy::CoprimeWeave,weave);
    assert(frames==2 && firstHash==lastHash); // 2.5/3.5/5.5-second joint repeat, excluding tumble
    reset(false);branching=true;nowMs=0;
    weave.frameMs=100;weave.ribbonSpeedMultiplier=1;
    Surfaces::animatePrimeStudy(2000,Surfaces::PrimeStudy::CoprimeWeave,weave);
    const auto slowRibbonGeometry=lastHash,slowRibbonColors=lastColors;
    reset(false);branching=true;nowMs=0;
    weave.frameMs=50;weave.ribbonSpeedMultiplier=2;
    Surfaces::animatePrimeStudy(1000,Surfaces::PrimeStudy::CoprimeWeave,weave);
    assert(frames==20 && lastHash==slowRibbonGeometry && lastColors!=slowRibbonColors);
    for(bool comparison: {false,true}) {
        reset(false);branching=true;nowMs=0;
        Swirls::animatePinwheelArms(comparison ? 30000 : 1000,32,comparison);
        assert(frames>20 && geometryChanges && colorChanges);
    }
    // Freeze geometry only: cycling must still move colors across the sculpture.
    reset(false);nowMs=0;
    Surfaces::FractalSettings still;still.sculptureTumbleRate=0;still.frameMs=1000;
    Surfaces::animateFractalVolume(24000,Surfaces::FractalLook::Sculpture,still);
    assert(!geometryChanges && colorChanges);
    // A held audition runs past its nominal duration, then cooperates with release.
    reset(false);nowMs=0;stopAt=3;
    TestDemo::Detail::animationDurationHeld()=true;
    Surfaces::animateFractalSculpture(1);
    assert(frames==3);
    reset(true);nowMs=0;
    Surfaces::animateFractalCavern(80);
    assert(frames==2);
}
'''

def main():
    sourceGeometry=(SRC/'GeometryEngine.h').read_text()
    geometry=sourceGeometry[sourceGeometry.index('    struct Point3 {'):sourceGeometry.index('    // Draw an ellipse centered')]
    geometry+='\n'+function(sourceGeometry,'inline void drawWuVoxel(')
    geometry+='\n'+function(sourceGeometry,'inline void drawSdfLine3D(')
    geometry+='\n'+sourceGeometry[sourceGeometry.index('    // Orthonormal plane frame'):sourceGeometry.index('   inline void drawHyperbolicParaboloid(')]
    surfaces=(SRC/'Surfaces.h').read_text()
    settings=surfaces[surfaces.index('    enum class FractalLook'):surfaces.index('    // One director for')]
    texture=(SRC/'TextureEngine.h').read_text()
    code=HARNESS.replace('__GEOMETRY__',geometry).replace('__SETTINGS__',settings)
    code=code.replace('__SCENE__',function(surfaces,'static void animateFractalVolume('))
    code=code.replace('__CAVERN__',function(surfaces,'static void animateFractalCavern('))
    code=code.replace('__SCULPTURE__',function(surfaces,'static void animateFractalSculpture('))
    code=code.replace('__MOUSESETTINGS__',surfaces[surfaces.index('    struct RecursiveMouseSettings'):surfaces.index('    static void animateRecursiveCube(')])
    code=code.replace('__MICE__',function(surfaces,'static void animateRecursiveCube('))
    code=code.replace('__PRIMESETTINGS__',surfaces[surfaces.index('    enum class PrimeStudy'):surfaces.index('    // Geometric auditions')])
    code=code.replace('__PRIMEDIRECTOR__',function(surfaces,'static void animatePrimeStudy('))
    code=code.replace('__FIBSETTINGS__',surfaces[surfaces.index('    struct FibonacciSweepSettings'):surfaces.index('    // One leader crosses')])
    code=code.replace('__FIBDIRECTOR__',function(surfaces,'static void animateFibonacciSweep('))
    code=code.replace('__FIBENTRY__',function(surfaces,'static void animateFibonacci('))
    code=code.replace('__ADVANCE__',function(texture,'void advance('))
    code=code.replace('__COLOR__',function(texture,'inline CRGB getFlowColor('))
    swirls=(SRC/'Swirls.h').read_text()
    code=code.replace('__PINWHEEL_CONSTANTS__',swirls[swirls.index('    static constexpr uint32_t pinwheelArmHoldMs'):swirls.index('    static void animatePinwheelArms(')])
    code=code.replace('__PINWHEEL__',function(swirls,'static void animatePinwheelArms('))
    # The small material fixture exercises the real phase/sampler, not unrelated shaders.
    demo=(SRC/'TestDemo.cpp').read_text()
    order=demo.split('constexpr ConceptType orderedConcepts[] = {')[1].split('};')[0]
    ids=re.findall(r'CONCEPT_(?:SHAPE|SCENE)_[A-Z_]+',order)
    pinwheel=ids.index('CONCEPT_SCENE_PINWHEEL_ARMS')
    assert ids[pinwheel+1:pinwheel+4]==['CONCEPT_SCENE_PRIME_SYMMETRY','CONCEPT_SCENE_COPRIME_WEAVE','CONCEPT_SCENE_ATOMIC_ORBITS']
    mouse=ids.index('CONCEPT_SCENE_WILD_MOUSE')
    assert ids[mouse-1:mouse+3]==['CONCEPT_SCENE_PS_SPRAY','CONCEPT_SCENE_WILD_MOUSE','CONCEPT_SCENE_RECURSIVE_CUBE','CONCEPT_SCENE_PIPES']
    dna=ids.index('CONCEPT_SCENE_DNA_SPIRAL')
    assert ids[dna-1]=='CONCEPT_SCENE_FIBONACCI'
    assert len(ids)==len(set(ids))==80
    julia=ids.index('CONCEPT_SCENE_JULIA')
    assert ids[julia-1]=='CONCEPT_SCENE_FRACTAL_CAVERN' # descending playback
    assert '/* Fractal Cavern */ { true, 1 }' in demo
    assert '/* Fractal Sculpture */ { false, 1 }' in demo
    with tempfile.TemporaryDirectory(prefix='cubina-fractals-') as tmp:
        path=Path(tmp)/'fractal.cpp';path.write_text(code)
        for size in (16,10):
            exe=Path(tmp)/f'fractal-{size}'
            subprocess.run([os.environ.get('CXX','g++'),'-std=c++11','-O2','-fsanitize=address,undefined','-g',
                            f'-DCANVAS={size}','-I',str(SRC),str(path),'-o',str(exe)],check=True)
            subprocess.run([str(exe)],check=True)

if __name__=='__main__': main()
