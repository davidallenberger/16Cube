#include "Audio.h"
#include "ParticleEffects.h"

void Audio::animateParticleGeq() {
    ParticleEffects::animateGeq(0, {}, PartyColors_p, TestDemo::audioRoutineRunning);
}

void Audio::animateParticleGeqNova() {
    ParticleEffects::animateGeqNova(0, {}, TestDemo::audioRoutineRunning);
}
