/*
  ==============================================================================

    LightFreeverb.h
    Created: 24 Sep 2026 6:32:01am
    Author:  Maximos Kaliakatsos-Papakostas

  ==============================================================================
*/

#pragma once

#include <array>
#include <vector>
#include <cmath>
#include <algorithm>
#include <cstddef>

#include "GraphicEQ.h"

class LightFreeverb
{
public:

    //==========================================================================
    // Preparation
    //==========================================================================

    void prepare(double sampleRate,
                 const GraphicEQ::Frequencies& frequencies)
    {
        sampleRate_ = sampleRate;

        const int combDelays[] =
        {
            1116, 1188, 1277, 1356,
            1422, 1491, 1557, 1617
        };

        const int allpassDelays[] =
        {
            556, 441, 341, 225
        };

        // Each comb owns its own 31-band GraphicEQ.
        for (std::size_t i = 0; i < combs.size(); ++i)
        {
            combs[i].setDelay(combDelays[i]);
            combs[i].prepareEQ(sampleRate, frequencies);
        }

        for (std::size_t i = 0; i < allpasses.size(); ++i)
            allpasses[i].setDelay(allpassDelays[i]);

        prepared_ = true;
    }


    //==========================================================================
    // Parameters
    //==========================================================================

    void setParameters(float roomSize,
                       float damping,
                       float wetLevel = 0.33f)
    {
        setRoomSize(roomSize);
        setDamping(damping);
        setWet(wetLevel);
    }


    void setRoomSize(float value)
    {
        value = std::clamp(value, 0.0f, 1.0f);

        for (auto& c : combs)
            c.feedback = 0.7f + value * 0.28f;
    }


    void setDamping(float value)
    {
        value = std::clamp(value, 0.0f, 1.0f);

        for (auto& c : combs)
            c.setDamping(0.0f + value * 0.01f);
//            c.setDamping(0.2f + value * 0.7f);
    }


    void setWet(float value)
    {
        wet = std::clamp(value, 0.0f, 1.0f);
    }


    void setDry(float value)
    {
        dry = std::clamp(value, 0.0f, 1.0f);
    }


    //==========================================================================
    // Graphic EQ
    //==========================================================================

    void setEQGains(const GraphicEQ::Gains& gains)
    {
        for (auto& c : combs)
            c.setEQGains(gains);
    }


    void setEQGain(std::size_t band, float gainDb)
    {
        for (auto& c : combs)
            c.setEQGain(band, gainDb);
    }


    float getEQGain(std::size_t band) const
    {
        if (combs.empty())
            return 0.0f;

        return combs[0].getEQGain(band);
    }


    //==========================================================================
    // Block processing
    //==========================================================================

    void beginBlock(std::size_t numSamples) noexcept
    {
        if (!prepared_ || numSamples == 0)
            return;

        for (auto& c : combs)
            c.beginBlock(numSamples);
    }


    //==========================================================================
    // Sample processing
    //==========================================================================

    inline float processSample(float input)
    {
        if (!prepared_)
            return input;

        const float inputCopy = input;

        float output = 0.0f;

        // Parallel comb bank.
        for (auto& c : combs)
            output += c.process(input);

        // Series allpass section.
        for (auto& a : allpasses)
            output = a.process(output);
        
        output = 0.1f*output*wet*wet;
        
        // clip output
        output = output > 0.9 ? 0.9 : output < -0.9 ? -0.9 : output;

        return output + dry * inputCopy;
    }
    
    //==============================================================================
    // Block processing
    //==============================================================================
    
    void process(float* samples,
            std::size_t numSamples) noexcept
    {
        if (!prepared_ || samples == nullptr)
            return;

        beginBlock(numSamples);

        for (std::size_t i = 0; i < numSamples; ++i)
            samples[i] = processSample(samples[i]);
    }


    //==========================================================================
    // Reset
    //==========================================================================

    void reset()
    {
        if (!prepared_)
            return;

        for (auto& c : combs)
            c.reset();

        for (auto& a : allpasses)
            a.reset();
    }


private:

    //==========================================================================
    // Comb filter
    //==========================================================================

    struct Comb
    {
        std::vector<float> buffer;

        int idx = 0;

        float feedback = 0.8f;
        float damping = 0.5f;
        float filterstore = 0.0f;

        GraphicEQ graphicEQ;


        void setDelay(int samples)
        {
            buffer.assign(samples, 0.0f);
            idx = 0;
        }


        void prepareEQ(
            double sampleRate,
            const GraphicEQ::Frequencies& frequencies)
        {
            graphicEQ.prepare(sampleRate, frequencies);
        }


        void setEQGains(const GraphicEQ::Gains& gains)
        {
            graphicEQ.setGains(gains);
        }


        void setEQGain(std::size_t band, float gainDb)
        {
            graphicEQ.setGain(band, gainDb);
        }


        float getEQGain(std::size_t band) const
        {
            return graphicEQ.getGain(band);
        }


        void beginBlock(std::size_t numSamples) noexcept
        {
            graphicEQ.beginBlock(numSamples);
        }


        void setDamping(float d)
        {
            damping = std::clamp(d, 0.0f, 1.0f);
        }


        inline float process(float input)
        {
            if (buffer.empty())
                return 0.0f;

            // Read the old comb output.
            const float output = buffer[idx];

            // Existing Freeverb-style damping filter.
            filterstore =
                output * (1.0f - damping)
                + filterstore * damping;

            // Construct the new feedback-loop sample.
            const float v =
                input + filterstore * feedback;

            // -----------------------------------------------------------------
            // IMPORTANT:
            //
            // The GraphicEQ is INSIDE the comb feedback loop.
            //
            // The EQ therefore modifies what gets stored back into the delay
            // line, and consequently modifies every subsequent recirculation.
            // -----------------------------------------------------------------
            const float equalized =
                graphicEQ.processSample(v);

            buffer[idx] = equalized;

            if (++idx >= static_cast<int>(buffer.size()))
                idx = 0;
            
            // clip comb output
            const float clipped_output = output > 0.9 ? 0.9 : output < -0.9 ? -0.9 : output;
            
//            return output;
            return clipped_output;
        }


        void reset()
        {
            std::fill(
                buffer.begin(),
                buffer.end(),
                0.0f);

            filterstore = 0.0f;
            idx = 0;

            graphicEQ.reset();
        }
    };


    //==========================================================================
    // Allpass filter
    //==========================================================================

    struct Allpass
    {
        std::vector<float> buffer;

        int idx = 0;

        float feedback = 0.5f;


        void setDelay(int samples)
        {
            buffer.assign(samples, 0.0f);
            idx = 0;
        }


        inline float process(float input)
        {
            if (buffer.empty())
                return 0.0f;

            const float bufout = buffer[idx];

            const float output =
                -input + bufout;

            buffer[idx] =
                input + bufout * feedback;

            if (++idx >= static_cast<int>(buffer.size()))
                idx = 0;

            return output;
        }


        void reset()
        {
            std::fill(
                buffer.begin(),
                buffer.end(),
                0.0f);

            idx = 0;
        }
    };


    //==========================================================================
    // Members
    //==========================================================================

    std::array<Comb, 8> combs;
    std::array<Allpass, 4> allpasses;

    double sampleRate_ = 0.0;

    float wet = 0.33f;
    float dry = 1.0f;

    bool prepared_ = false;
};
