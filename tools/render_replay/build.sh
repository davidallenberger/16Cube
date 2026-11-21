#!/usr/bin/env bash
# Build the render replay against the real render sources and the real FastLED
# (its host "stub" platform). FastLED is compiled once into build/.
set -euo pipefail
root="$(cd "$(dirname "$0")/../.." && pwd)"
fl="$root/render-board/.pio/libdeps/esp32dev/FastLED/src"
[[ -d "$fl" ]] || { echo "Missing $fl: build the render board once with PlatformIO." >&2; exit 1; }
build="$root/tools/render_replay/build"
mkdir -p "$build/fastled"
CXX=${CXX:-clang++}
defs=(-DFASTLED_STUB_IMPL -DFASTLED_TESTING -DCUBINA_ENABLE_UART_RECEIVER=1 -DINCLUDE_WLED_PALETTES=1)
if [[ ! -f "$build/libfastled_host.a" ]]; then
    echo "Compiling FastLED for the host (once)..."
    # time_stub.cpp is excluded: the replay owns millis()/micros().
    for f in $(cd "$fl" && find . -name "*.cpp" ! -path "./platforms/*") \
             $(cd "$fl" && ls platforms/stub/*.cpp | grep -v time_stub) \
             $(cd "$fl" && find platforms/shared -name "*.cpp"); do
        "$CXX" -std=gnu++17 -O2 -w "${defs[@]}" -I "$fl" -c "$fl/$f" -o "$build/fastled/$(echo "$f" | tr '/.' '__').o"
    done
    ar rcs "$build/libfastled_host.a" "$build"/fastled/*.o
fi
inc=(-I "$root/tools/render_replay/shim" -I "$fl" -I "$root/render-board/src" -I "$root/shared" -I "$root/tests/stubs")
objs=()
for f in AudioShapes.cpp ParticleEffects.cpp WledPalettes.cpp AppMemory.cpp; do
    "$CXX" -std=gnu++17 -O2 -w "${defs[@]}" "${inc[@]}" -c "$root/render-board/src/$f" -o "$build/${f%.cpp}.o"
    objs+=("$build/${f%.cpp}.o")
done
"$CXX" -std=gnu++17 -O2 -w "${defs[@]}" "${inc[@]}" "$root/tools/render_replay/render_replay.cpp" \
    "${objs[@]}" "$build/libfastled_host.a" -o "$root/tools/render_replay/render_replay"
echo "Built tools/render_replay/render_replay"
