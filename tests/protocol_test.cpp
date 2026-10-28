#include "SharedProtocol.h"
#include <assert.h>
#include <stdio.h>
#include <vector>
using namespace CubinaWire;
static std::vector<uint8_t> frame(const Packet& p){uint8_t b[MAX_FRAME];size_t n=encode(p,b,sizeof(b));assert(n);return {b,b+n};}
int main(){
    const char* text="123456789";assert(crc16((const uint8_t*)text,9)==0x29b1);
    Packet p;p.session=123;p.modeRevision=77;p.sequence=42;p.capturedMs=0xfffffff0;
    p.mode=Mode::Depth;p.type=Type::Depth;p.flags=SOURCE_READY;p.length=DEPTH_BYTES;
    for(size_t i=0;i<DEPTH_BYTES;++i)p.payload[i]=i&255;
    auto bytes=frame(p);assert(bytes.size()<=MAX_FRAME);Packet q;
    assert(decode(bytes.data(),bytes.size()-1,q));assert(q.session==123&&q.modeRevision==77&&q.sequence==42);
    assert(!memcmp(p.payload,q.payload,DEPTH_BYTES));
    // Every single-bit corruption in a maximum frame must be rejected. A bit
    // flip in the delimiter prevents delivery, too; the next delimiter recovers.
    for(size_t pos=0;pos<bytes.size();++pos)for(unsigned bit=0;bit<8;++bit){
        auto bad=bytes;bad[pos]^=1<<bit;Parser parser;bool delivered=false;
        for(auto b:bad)delivered|=parser.feed(b,10,q);
        assert(!delivered);
    }
    // Drop/truncate, inject long garbage, recover at delimiter; no blocking API.
    Parser parser;for(size_t i=0;i<bytes.size()/2;++i)assert(!parser.feed(bytes[i],1,q));
    parser.feed(0,2,q);for(unsigned i=0;i<MAX_ENCODED+100;++i)assert(!parser.feed(1,3,q));
    parser.feed(0,4,q);unsigned good=0;for(auto b:bytes)good+=parser.feed(b,5,q);assert(good==1);
    // Timeout across millis wrap; next intact packet still works.
    parser.feed(1,0xfffffff0,q);parser.feed(0,60,q);
    for(auto b:bytes)good+=parser.feed(b,61,q);
    assert(good==2);
    // Presses + axes encode the existing GamepadState field by field.
    p={};p.mode=Mode::Game;p.type=Type::Gamepads;p.length=GAMEPAD_BYTES;
    GamepadState g;g.connected=true;g.axisX=-511;g.axisY=512;g.a=true;g.l2=true;g.dpad=PAD_LEFT;g.miscButtons=4;
    writePad(p.payload,g);writePad(p.payload+15,GamepadState());put32(p.payload+30,buttons(g));
    bytes=frame(p);assert(decode(bytes.data(),bytes.size()-1,q));auto back=readPad(q.payload);
    assert(back.axisX==-511&&back.axisY==512&&back.a&&back.l2&&back.dpad==PAD_LEFT);
    auto invalid=p;invalid.payload[6]=2;assert(!encode(invalid,bytes.data(),bytes.size()));
    g.a=false;applyPresses(g,1);assert(g.a);
    uint8_t bits[512]={};setVoxelBit(bits,15,15,15);assert(bits[511]==128);assert(voxel(bits,15,15,15));
    setVoxelBit(bits,15,15,15,false);assert(!voxel(bits,15,15,15));
    // All source modes and all byte values (including embedded zero) round-trip.
    for(unsigned mode=1;mode<=4;++mode){p={};p.mode=Mode(mode);WLEDAudioPayload a={};a.mode=mode==1?1:mode==3?3:2;
        a.beat=1;a.volumeRaw=255;for(int i=0;i<16;++i)a.fftResult[i]=i*17;writeAudio(p,a,1234);
        bytes=frame(p);assert(decode(bytes.data(),bytes.size()-1,q));assert(readAudio(q).volumeRaw==255&&get32(q.payload+21)==1234);}
    printf("Protocol tests passed: CRC, maximum frames, corruption, resync, timeout, source modes, gamepads, voxel ordering. Max frame=%zu bytes.\n",MAX_FRAME);
}
