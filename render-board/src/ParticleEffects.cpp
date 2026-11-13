#include "Audio.h"
#include "ParticleEffects.h"

namespace {
void runParticleGeq(ParticleEffects::GeqLayout layout) {
    ParticleEffects::GeqSettings settings;
    settings.layout = layout;
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
