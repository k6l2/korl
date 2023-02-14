# Renderer API Options

- Windows audio rendering APIs
    - MME (aka WinMM)
        - pros
            - maximum compatibility; this goes back to ye-olden days of Windows, so should be compatible with more systems
        - cons
            - old/deprecated/buggy
            - emulated on latest Windows
            - auto-resampling can occur in newer versions of window if sample rate conversion is needed
    - DirectSound
        - pros
            - DirectX package.. maybe simpler API?... eh
        - cons
            - COM ☹
            - deprecated by XAudio2
            - built on top of WASAPI; likely to be a pointless abstraction layer and/or bloat
    - WASAPI
        - pros
            - latest updates in Windows 8+
            - not emulated on latest Windows; lowest latency audio API
            - potential to offload mixing/effect processing onto audio hardware
        - cons
            - COM ☹
            - potential to offload mixing/effect processing onto audio hardware <- UWP apps _only_ though; C++ platform build likely required, or perhaps even C# 😢
    - XAudio2
        - pros
            - features
                - DSP; per-voice filters
                - submixing
                - run-time decompression of ADPCM
                - multichannel/surround-sound support
                - run-time sample rate upscaling; trade audio quality for CPU
                - non-blocking API
            - built specifically for low-latency applications (games, etc.); successor to DirectSound
        - cons
            - COM ☹
            - only supported in the latest versions of windows; >= XP
            - operates on top of DirectSound on XP, & WASAPI on Vista+
            - _no_ C support; only C++ 😡
- 3rd-party audio rendering APIs (which utilize a subset of the above APIs on Windows)
    - OpenAL
        - used in SFML for Vagante
        - kinda shitty implementation that continues to have bugs in latest release of Vagante
            - such as the inability for the audio stream to be re-attach to new audio device when the current audio device is disconnected
        - now proprietary asset of Creative Technology... yikes!
    - OpenAL Soft
        - LGPL licenced software implementation of OpenAL
        - uses WASAPI on Windows
        - I have no idea how this performs compared to what we used on Vagante, but it seems to have plenty of active development
    - SoLoud
        - pretty neat ZLib/LibPNG licence open-source cross-platform audio lib, which can be used with pure C via DLL linkage
        - includes a text->speech synthesizer (neat!)
    - Fmod
        - free-$2000 "Indie" license
        - used by Unity, Unreal, etc.
        - looks like the API is C++ only...
    - Wwise
        - license requires 1% of gross profits.. eh, not bad I guess but I doubt this complexity/feature-set is required or all that useful

While XAudio certainly has a tempting set of features, I want to shy away from C++ compilation for as long as possible, as the minimization of build times & maximization of debugging experience are prime directives of KORL which would very likely be violated with the introduction of C++, from my own experience.  Needless to say, DirectSound is _not_ on the the table of consideration, and neither is MME for similar reasons, but mostly because it just seems like abandonware from what I've read so far.

[x] initialize COM
[x] enumerate over audio devices; print friendly names
[x] select/activate the default multimedia audio device
[x] obtain audio output buffer
[x] write sine wave output to the audio buffer
[ ] when the audio device is disconnected, attempt to connect to the next default audio device
    - how do we detect when this audio device is disconnected?
[ ] add `korl_audio_play` API
    [ ] add korl-resource WAV decoder
    [ ] add korl-audioMixer; potentially reuse/recycle `kgtAudioMixer` code
    [ ] play WAV asset audio