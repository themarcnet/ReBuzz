﻿using NAudio.Wave;
using ReBuzz.Core;

namespace ReBuzz.Audio
{
    internal class AudioProvider : ISampleProvider, IReBuzzAudioProvider
    {
        public WaveFormat WaveFormat { get; }

        public CommonAudioProvider AudioSampleProvider { get; }

        public AudioProvider(ReBuzzCore buzzCore, int sampleRate, int channels, int bufferSize, bool doubleBuffer)
        {
            // Make this multi-channel compatible
            WaveFormat = WaveFormat.CreateIeeeFloatWaveFormat(sampleRate, channels);
            AudioSampleProvider = new CommonAudioProvider(buzzCore, sampleRate, channels, bufferSize, doubleBuffer);
        }

        public void ClearBuffer()
        {
            AudioSampleProvider.ClearBuffer();
        }

        // Can be multi-channel
        public int Read(float[] buffer, int offset, int count)
        {
            return AudioSampleProvider.Read(buffer, offset, count);
        }

        public void Stop()
        {
            AudioSampleProvider?.Stop();
        }

        // Always stereo
        public int ReadOverride(float[] buffer, int offset, int count)
        {
            return AudioSampleProvider.ReadOverride(buffer, offset, count);
        }
    }
}
