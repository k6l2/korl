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

While XAudio certainly has a tempting set of features, I want to shy away from C++ compilation for as long as possible, as the minimization of build times & maximization of debugging experience are prime directives of KORL which would very likely be violated with the introduction of C++, from my own experience.  Needless to say, DirectSound is _not_ on the the table of consideration, and neither is MME for similar reasons, but mostly because it just seems like abandonware from what I've read so far.  For now, I will proceed with WASAPI, until the amount of work required to implement certain features becomes too prohibitive.

[x] initialize COM
[x] enumerate over audio devices; print friendly names
[x] select/activate the default multimedia audio device
[x] obtain audio output buffer
[x] write sine wave output to the audio buffer
[x] when the audio device is disconnected, attempt to connect to the next default audio device
    - how do we detect when this audio device is disconnected?
    - maybe we just program the rest of the renderer under the assumption that we can handle this reconnection process without an "event callback" mechanism, such as IMMNotificationClient
    - nevermind; I figured out how to implement the IMMNotificationClient thing, so this should be possible now
[x] add `korl_sfx_play` API
    [x] add korl-resource WAV decoder
        - I am currently thinking that we perform the conversion from source sample rate => renderer sample rate in here, and just cache this raw audio in korl-resource
            - the main reason for this would be to prevent the need for us to have to do this processing (if necessary) at runtime
            - this will obviously incur a large spike in CPU in the form of re-caching resources if korl-audio sample rate changes, such as if the user unplugs their headphones to switch to desktop speaker audio, but emphirically this doesn't seem like a huge issue when we consider the fact that, during actual use, the user's audio configuration is almost always going to be _static_; the user should not expect the application to behave "smoothly" when they are hot-swapping their multimedia hardware configuration
            - we also likely want to cache both the original transcoded audio data, as well as our re-sampled audio data, so that when korl-audio configuration changes we don't have to continuously degrade audio by resampling resampled data, and we don't waste time having to re-transcode the source audio again to retrieve the original signal, which is likely to be time-consuming since audio is almost always going to be coming from korl-file, and even if it wasn't the requirement for korl-resource to be able to "re-obtain" the source resource data at any time seems likely to be a really annoying burdon on that API when I am thinking about it...
        - for the resampling algorithm itself, let's just throw in naive LERP for now just to see how far that takes us; later, when quality becomes an issue, we can try some other advanced filtering techniques
            - https://stackoverflow.com/q/4393545/4526664
                - https://github.com/shibatch/SSRC
                    - neat LGBL resampler specifically with 44.1 <=> 48 khz resampling in mind, which are my current audio endpoint mixer configurations strangely enough 🙃
                - http://www.nicholson.com/rhn/dsp.html#3
                    - some hilariously simple looking basic (literally) implementations of relavent audio filtering code
                - https://ccrma.stanford.edu/~jos/resample/
                    - a nice looking explanation of resampling theory; neat!
                - http://www.mega-nerd.com/SRC/index.html
                    - 2-clause BSD license, _very_ simple/clean looking C library for resampling!  honestly, I might just utilize this
                    - slightly lame that the source isn't distributed via git, but I don't have too much of an issue just dumping it into the `/code` folder
    [x] add korl-resource-audio resampler, described above
    [x] add korl-audioMixer; potentially reuse/recycle `kgtAudioMixer` code
        - I really like the "Tape" & "TapeDeck"/"Track" metaphores
        - I also like how the user only has to worry about a handle, and only if they really need to for DJ purposes
    [x] play WAV asset audio
[x] add master volume
[x] add tape volume
[x] add concept of "tape categories"
    [x] add tape category volume
[x] add per-channel tape volume (panning)
[x] add 3D spatial audio processing
    - simple amplitude attenuation equation for audio waves: https://www.nde-ed.org/Physics/Waves/attenuation.xhtml
    [x] input listener position & world-up v3f32s
    [x] input tape position & attenuation coefficient
    [x] automatically calculate tapeVolume & tapeChannelVolumes for tapes set to this mode
        [x] attenuate sound as distance to the listener increases
        [x] attenuate sound per-channel as the angle between `listenerToTape` & `channelDirection` vectors increases
        [x] use less per-channel attenuation as `listenerToTapeDistance` approaches 0
[x] add ogg/vorbis decoder support
[x] add the ability for the user to configure an audio tape to repeat
[x] add `korl_sfx_stop` API, with a fade-out seconds parameter, so the user can stop looping audio easily
[x] add tape category override option for `korl_sfx_playResource`
    - we want the ability for a request to play a `BGM` sound category tape to override `SFX` tapes, so that BGMs don't get blocked
    - it makes sense to at least log a WARNING if the request to play a Tape with non-zero override categories fails to find a free Deck
    - or perhaps we just add the concept of `priority` to `korl_sfx_playResource`?
