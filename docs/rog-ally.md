# Demon Front on a 1080p Linux handheld

The Linux frontend can run the V105 game in a 16:9 playfield. Both translators must be run with `--wide` on the V105 images prepared as described in the main README.

```sh
mkdir -p notes
python3 tools/thumbrec.py --wide
python3 tools/m68krec.py --wide
cmake -G Ninja -S . -B build/ally -DDF_PLAY=ON -DDF_WIDE=ON -DCMAKE_CXX_COMPILER=clang++
cmake --build build/ally --target demonfront
cp build/ally/demonfront .
./demonfront
```

Install the binary next to the private `rom/` folder. It uses SDL2, OpenGL 3.3 and zlib. The desktop shortcut can launch the binary directly.

The gameplay viewport spans 598 board columns. Verified V105 translation sites extend player limits, object clipping and map activation. The camera tracks around column 299. Fixed character and stage selection artwork stays centered. The GPU reads background and text tile graphics, palette indices and sprite coverage, and composes a 1920 × 1080 color target. The original pixel art is preserved. Simulation and presentation target 60 Hz with resampled audio.

Move with the D-pad or left stick, shoot with A, jump with B, and use the special with X or LB. Start begins free play. View toggles pause. Cabinet memory persists in `sram.bin`.

The Valley regression run on the Ally covered 6,000 frames from boot, the character and stage screens, left movement to X=20, camera tracking at X=299, and right movement to X=578 in a locked encounter.

See the [six-chapter test report](testes-fases-v105.md) for the before-and-after measurements, player bounds and nine boss transitions. Where the arcade art ends, the compositor continues terrain and panoramic layers using the original assets at the same scale.
