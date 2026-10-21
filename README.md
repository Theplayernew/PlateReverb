# Plate Reverb (VST3)

Stereo plate reverb built on the Dattorro topology: pre-delay → bandwidth filter → 4 input diffusers → figure-eight tank
with modulated allpasses, damping and cross-feedback → 14 decorrelated output taps. Wet path has low/high cut and width.

Parameters: Pre-Delay, Decay, Damping, Bandwidth, Diffusion, Modulation, Low Cut, High Cut, Width, Mix.

## Build
Requires CMake 3.22+ and a C++17 compiler. JUCE 8 is fetched automatically.

    cmake -B build -DCMAKE_BUILD_TYPE=Release
    cmake --build build --config Release --target PlateReverb_VST3

Output: `build/PlateReverb_artefacts/Release/VST3/Plate Reverb.vst3`
- Windows: copy to `C:\Program Files\Common Files\VST3`
- macOS: copy to `~/Library/Audio/Plug-Ins/VST3` (unsigned; right-click-open or `xattr -cr`)
- Linux: copy to `~/.vst3`
Add `-DCOPY_AFTER_BUILD=ON` to install automatically.

No local toolchain? Push this folder to GitHub: the included workflow builds Windows, macOS and Linux VST3s
(Actions tab → artifacts).

`Source/PlateReverb.h` is framework-free; `test/test.cpp` is a stability/decay check (`g++ -std=c++17 test/test.cpp`).
