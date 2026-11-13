#include "Audio.h"
#include "ParticleEffects.h"

namespace {
void runParticleGeq(ParticleEffects::GeqLayout layout) {
    ParticleEffects::GeqSettings settings;
    settings.layout = layout;
    settings.inputGain = Audio::controls().particleGeqGain;
    settings.brightnessGain = Audio::controls().particleGeqBrightness;
    // Inward 16 has fewer visible particles. Give each spark more light.
    if (layout == ParticleEffects::GeqLayout::Random) settings.brightnessGain *= 1.2f;
    settings.speed = 200; // Only the strongest bins can clear the 16-voxel ceiling.
    settings.diverge = 56; // Independent lateral spread on the two floor axes.
    ParticleEffects::animateGeq(0, settings, RainbowColors_p, TestDemo::audioRoutineRunning);
}
}

void Audio::animateParticleGeqClusters() {
    runParticleGeq(ParticleEffects::GeqLayout::Clusters);
}

void Audio::animateParticleGeqRandom() {
    runParticleGeq(ParticleEffects::GeqLayout::Random);
}
