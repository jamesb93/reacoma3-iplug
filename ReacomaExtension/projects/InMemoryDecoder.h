#pragma once

#include "reaper_plugin.h"
#include <vector>
#include <string>
#include <algorithm>

// This class implements the ISimpleMediaDecoder interface to read audio directly
// from a std::vector in memory.
class InMemoryDecoder : public ISimpleMediaDecoder
{
public:
    // Constructor: Takes ownership of the audio data via std::move
    InMemoryDecoder(std::vector<ReaSample>&& samples, int num_channels, double sample_rate) :
        m_samples(std::move(samples)),
        m_num_channels(num_channels),
        m_sample_rate(sample_rate),
        m_position(0)
    {
    }

    virtual ~InMemoryDecoder() {}

    // Reaper will call this to get a copy of the decoder for its own use.
    // It's crucial that this returns a new instance with a copy of the data.
    virtual ISimpleMediaDecoder* Duplicate() override
    {
        // Create a copy of the sample data for the new instance
        std::vector<ReaSample> samples_copy = m_samples;
        InMemoryDecoder* new_decoder = new InMemoryDecoder(std::move(samples_copy), m_num_channels, m_sample_rate);
        return new_decoder;
    }

    // --- These methods are for file-based sources, we can provide dummy implementations ---
    virtual void Open(const char* filename, int diskreadmode, int diskreadbs, int diskreadnb) override {}
    virtual void Close(bool fullClose) override {}
    virtual const char* GetFileName() override { return ""; } // Not a file
    virtual const char* GetType() override { return "MEM_SRC"; } // Custom type identifier
    virtual void GetInfoString(char* buf, int buflen, char* title, int titlelen) override
    {
        snprintf(title, titlelen, "In-Memory Source Info");
        snprintf(buf, buflen, "Custom source generated from memory.\nChannels: %d\nSample Rate: %.0f", m_num_channels, m_sample_rate);
    }

    // --- Core metadata methods ---
    virtual bool IsOpen() override { return !m_samples.empty(); }
    virtual int GetNumChannels() override { return m_num_channels; }
    virtual int GetBitsPerSample() override { return sizeof(ReaSample) * 8; }
    virtual double GetSampleRate() override { return m_sample_rate; }

    // --- Position and Length methods ---
    virtual INT64 GetLength() override
    {
        if (m_num_channels == 0) return 0;
        return m_samples.size() / m_num_channels; // Length in sample-frames
    }

    virtual INT64 GetPosition() override { return m_position; }
    virtual void SetPosition(INT64 pos) override { m_position = pos; }

    // This is the most important method. Reaper calls this to get audio.
    virtual int ReadSamples(ReaSample* buf, int length) override
    {
        if (!buf || length <= 0) return 0;

        INT64 total_frames = GetLength();
        INT64 frames_available = total_frames - m_position;

        // Determine how many frames we can actually copy
        int frames_to_copy = static_cast<int>(std::min((INT64)length, frames_available));

        if (frames_to_copy <= 0)
        {
            return 0; // End of media
        }

        // Calculate the number of individual samples (frames * channels) to copy
        size_t samples_to_copy = frames_to_copy * m_num_channels;
        
        // Calculate the starting position in our internal vector
        size_t start_offset = m_position * m_num_channels;

        // Copy the data to the buffer Reaper provides
        memcpy(buf, &m_samples[start_offset], samples_to_copy * sizeof(ReaSample));

        // Advance our internal read position
        m_position += frames_to_copy;

        // Return the number of sample-FRAMES read
        return frames_to_copy;
    }

    // --- Not used in this example ---
    virtual int Extended(int call, void* parm1, void* parm2, void* parm3) override { return 0; }

private:
    std::vector<ReaSample> m_samples;
    int m_num_channels;
    double m_sample_rate;
    INT64 m_position;
};
