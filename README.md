ChuckStation2

A cross-platform PlayStation 2 emulator based on [Iris](https://github.com/allkern/iris).

Supports Windows, macOS, Linux, and Android with Vulkan graphics.

Features

- PS2 emulation (EE, IOP, VU, GS, SPU2, IPU)
- Vulkan renderer
- Windows, macOS, Linux, and Android support
- Debugging tools
- ISO, BIN/CUE, CHD, and CISO support
- PS1/PS2 memory cards
- Save states
- Custom shaders
- Gamepad, keyboard, and touch input

## Building

### Requirements

- CMake 3.21+
- Git
- C++20 compiler
- Vulkan SDK
- Android NDK 26.1+ and SDK API 34 for Android

### Clone
```bash
git clone --recursive https://github.com/your-username/ChuckStation2.git
cd ChuckStation2
```
### Desktop
```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```
### Android
```bash
cd android
./gradlew assembleDebug
```
The APK will be located in:

``android/app/build/outputs/apk/``

## Acknowledgments

ChuckStation2 is based on [Iris](https://github.com/allkern/iris).

## License

See [LICENSE](LICENSE) for license information.