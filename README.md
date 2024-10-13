# ReBuzz - Complete Build fork
Hello, this is my Complete ReBuzz fork.  This fork allows for a complete build of ReBuzz, including dependencies, 3rd party stuff, and runtime files.

You do not (should not - I have not tested assertion) need to install ReBuzz to build this fork of Rebuzz.

This fork will build / copy everything required into the output 'bin' folder, which is located at the root of your local git repository.  ReBuzz can then be run from that location.

# What is ReBuzz ?
ReBuzz is a modular digital audio workstation (DAW) built upon the foundation of [Jeskola Buzz](https://jeskola.net/buzz/) software. Written in C#, ReBuzz combines modern features with the beloved workflow of its predecessor. While it’s still in development, users should exercise some caution regarding stability and other potential uncertainties. The primary focus is on providing a stable experience and robust VST support.

## Features
* 32 and 64 bit VST2/3 support
* 32 and 64 bit buzz machine support
* Modern Pattern Editor, Modern Sequence Editor, AudioBlock, EnvelopeBlock, CMC, TrackScript...
* Multi-process architecture
* Multi-io for native and managed machines
* Includes [NWaves](https://github.com/ar1st0crat/NWaves) .NET DSP library for audio processing
* bmx and bmxml file support
* ...

## Download
[ReBuzz Installer](https://github.com/wasteddesign/ReBuzz/releases/latest)

Requires:
1. [.NET 8.0 Desktop Runtime - Windows x64](https://dotnet.microsoft.com/en-us/download/dotnet/thank-you/runtime-desktop-8.0.3-windows-x64-installer?cid=getdotnetcore)
2. [Latest Microsoft Visual C++ Redistributable](https://learn.microsoft.com/en-us/cpp/windows/latest-supported-vc-redist?view=msvc-170)

## How to build?
1. You need Visual Studio 2022.  Community Edition will do.

2. Clone this ReBuzz Fork.  Make sure you also get all the submodules (using -recurse-submodules)

3. Load the ReBuzz.sln into VS 2022

4. Select Debug Config.  (Release might work, but I've not tested that yet)

5. Build all.

6. You can now run ReBuzz directly from within Visual Studio.  (it does not need to run from c:\Program Files)


## How can I help?
All the basic functionality is implemented but there many areas to improve. In general, contributions are needed in every part of the software, but here are few items to look into:

- [ ] Pick an [issue](https://github.com/wasteddesign/ReBuzz/issues) and start contributing to Rebuzz development today!
- [ ] Improve stability, fix bugs and issues
- [ ] Cleanup code and architecture
- [ ] Add comments and documentation
- [ ] Improve Audio wave handling (Wavetable)
- [ ] Improve file handling to support older songs
- [ ] Reduce latency, optimize code

You might want to improve also
- [ReBuzz GUI Components](https://github.com/wasteddesign/ReBuzzGUI)
- [ReBuzzEngine](https://github.com/wasteddesign/ReBuzzEngine)
- [ModernPatternEditor](https://github.com/wasteddesign/ModernPatternEditor)

Let's make this a good one.
