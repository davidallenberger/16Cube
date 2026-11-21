#include "Audio.h"
#include "ParticleEffects.h"

namespace {
void runParticleGeq(ParticleEffects::GeqLayout layout, bool liquid = false) {
    ParticleEffects::GeqSettings settings;
    settings.layout = layout;
    settings.liquid = liquid;
    settings.inputGain = Audio::controls().particleGeqGain;
    settings.brightnessGain = Audio::controls().particleGeqBrightness;
    ParticleEffects::animateGeq(0, settings, RainbowColors_p, TestDemo::audioRoutineRunning);
}
}

void Audio::animateParticleGeqClusters() {
    runParticleGeq(ParticleEffects::GeqLayout::Clusters);
}

void Audio::animateParticleGeqTiles() {
    runParticleGeq(ParticleEffects::GeqLayout::Tiles);
}

void Audio::animateParticleGeqPairedBands() {
    runParticleGeq(ParticleEffects::GeqLayout::PairedBands);
}

void Audio::particleGeq(uint8_t layout, bool liquid) {
    const ParticleEffects::GeqLayout layouts[] = {
        ParticleEffects::GeqLayout::Clusters, ParticleEffects::GeqLayout::Tiles,
        ParticleEffects::GeqLayout::PairedBands
    };
    runParticleGeq(layouts[layout % 3], liquid);
}

void Audio::animateParticleGeqNova() {
    ParticleEffects::animateNova(RainbowColors_p, Audio::controls().particleGeqGain,
                                 Audio::controls().particleGeqBrightness, TestDemo::audioRoutineRunning);
}
