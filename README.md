# Plate Reverb (VST3)

Stereo plate reverb built on the Dattorro topology: pre-delay → bandwidth filter → 4 input diffusers → figure-eight tank
with modulated allpasses, damping and cross-feedback → 14 decorrelated output taps. Wet path has low/high cut and width.
All DSP runs in 64-bit float internally; the plugin accepts both 32-bit and 64-bit (double) host buffers.

Parameters: Pre-Delay, Reverb Time (0.1–20 s RT60), Mix, Diffusion, Motion, Width, Damping, Bandwidth, Low Cut, High Cut.

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
