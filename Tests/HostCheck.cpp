// Loads the built VST3 *bundle* through JUCE's VST3 host and exercises it like a DAW would:
// jmc_hostcheck "/path/to/JMC REVERSE.vst3" [audio.wav]
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <atomic>
#include <cstdio>
#include <thread>

static int failures = 0;
#define CHECK(cond, msg) do { if (! (cond)) { ++failures; std::printf ("FAIL: %s  [%s]\n", msg, #cond); } else std::printf ("ok:   %s\n", msg); } while (0)

static std::unique_ptr<juce::AudioPluginInstance> make (juce::AudioPluginFormatManager& fm, const juce::PluginDescription& d, double sr, int bs, juce::String& err)
{
    return fm.createPluginInstance (d, sr, bs, err);
}

static bool finite (const juce::AudioBuffer<float>& b)
{
    for (int c = 0; c < b.getNumChannels(); ++c)
        for (int i = 0; i < b.getNumSamples(); ++i)
            if (! std::isfinite (b.getSample (c, i))) return false;
    return true;
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    if (argc < 2) { std::printf ("usage: jmc_hostcheck plugin.vst3 [audio.wav]\n"); return 2; }
    const juce::String bundle (argv[1]);
    const juce::String wavPath = argc > 2 ? juce::String (argv[2]) : juce::String();

    juce::AudioPluginFormatManager fm;
    fm.addFormat (std::make_unique<juce::VST3PluginFormat>());
    juce::OwnedArray<juce::PluginDescription> found;
    for (auto* f : fm.getFormats()) f->findAllTypesForFile (found, bundle);
    CHECK (found.size() == 1, "VST3 bundle scans and exposes exactly one plugin");
    if (found.isEmpty()) return 1;
    const auto& desc = *found[0];
    std::printf ("      %s | %s | %s | in %d out %d | fx=%d synth=%d\n", desc.name.toRawUTF8(), desc.manufacturerName.toRawUTF8(),
                 desc.category.toRawUTF8(), desc.numInputChannels, desc.numOutputChannels, ! desc.isInstrument, desc.isInstrument);
    CHECK (desc.name == "JMC REVERSE" && ! desc.isInstrument, "description: JMC REVERSE, effect (not an instrument)");

    juce::String err;
    juce::Random rnd (1234);
    {
        auto probe = make (fm, desc, 48000.0, 256, err);
        CHECK (probe != nullptr && probe->getTotalNumInputChannels() == 2 && probe->getTotalNumOutputChannels() == 2 && ! probe->acceptsMidi(), "instance: stereo in/out, no MIDI");
    }

    // ---- every sample rate x block size
    {
        bool ok = true;
        for (double sr : { 44100.0, 48000.0, 88200.0, 96000.0, 192000.0 })
            for (int bs : { 16, 64, 256, 512, 2048 })
            {
                auto p = make (fm, desc, sr, bs, err);
                if (p == nullptr) { ok = false; std::printf ("      create failed: %s\n", err.toRawUTF8()); continue; }
                p->setPlayConfigDetails (2, 2, sr, bs);
                p->prepareToPlay (sr, bs);
                juce::AudioBuffer<float> buf (2, bs), ref;
                juce::MidiBuffer midi;
                for (int b = 0; b < 40; ++b)
                {
                    for (int c = 0; c < 2; ++c) for (int i = 0; i < bs; ++i) buf.setSample (c, i, 0.5f * (rnd.nextFloat() * 2 - 1));
                    ref.makeCopyOf (buf);
                    p->processBlock (buf, midi);
                    for (int c = 0; c < 2; ++c) for (int i = 0; i < bs; ++i) if (std::abs (buf.getSample (c, i) - ref.getSample (c, i)) > 1e-7f) ok = false;
                }
                p->releaseResources();
            }
        CHECK (ok, "5 sample rates x 5 block sizes: loads, prepares, passes audio through untouched");
    }

    // ---- mono and stereo layouts
    {
        auto p = make (fm, desc, 48000.0, 256, err);
        auto layout = p->getBusesLayout();
        layout.getMainInputChannelSet() = juce::AudioChannelSet::mono();
        layout.getMainOutputChannelSet() = juce::AudioChannelSet::mono();
        const bool monoOk = p->setBusesLayout (layout);
        p->prepareToPlay (48000.0, 256);
        juce::AudioBuffer<float> buf (1, 256);
        juce::MidiBuffer midi;
        for (int i = 0; i < 256; ++i) buf.setSample (0, i, 0.3f);
        p->processBlock (buf, midi);
        CHECK (monoOk && finite (buf), "mono layout accepted and processes");
        p->releaseResources();
    }

    // ---- automation storm + state + editor open/close while audio runs on another thread
    {
        auto p = make (fm, desc, 48000.0, 256, err);
        p->setPlayConfigDetails (2, 2, 48000.0, 256);
        p->prepareToPlay (48000.0, 256);
        std::atomic<bool> run { true };
        std::atomic<bool> bad { false };
        std::thread audio ([&]
        {
            juce::AudioBuffer<float> buf (2, 256);
            juce::MidiBuffer midi;
            juce::Random r (99);
            while (run.load())
            {
                for (int c = 0; c < 2; ++c) for (int i = 0; i < 256; ++i) buf.setSample (c, i, 0.4f * (r.nextFloat() * 2 - 1));
                p->processBlock (buf, midi);
                if (! finite (buf)) bad = true;
                juce::Thread::sleep (1);
            }
        });
        juce::MemoryBlock blob;
        for (int round = 0; round < 40; ++round)
        {
            for (auto* par : p->getParameters())
                if (rnd.nextInt (3) == 0) par->setValueNotifyingHost (rnd.nextFloat());
            if (round % 5 == 0)
            {
                p->getStateInformation (blob);
                p->setStateInformation (blob.getData(), (int) blob.getSize());
            }
            if (round % 8 == 0)
            {
                std::unique_ptr<juce::AudioProcessorEditor> ed (p->createEditorIfNeeded());
                if (ed != nullptr) { ed->setSize (1000, 772); juce::MessageManager::getInstance()->runDispatchLoopUntil (30); }
            }
            juce::Thread::sleep (5);
        }
        run = false;
        audio.join();
        CHECK (! bad.load(), "40 rounds of random automation + state recall + editor open/close during processing: audio stays finite");
        p->releaseResources();
    }

    // ---- create / destroy while loading and rendering are in flight
    if (wavPath.isNotEmpty())
    {
        juce::XmlElement xml ("JMCREVERSE");
        xml.setAttribute ("sourcePath", wavPath);
        xml.setAttribute ("character", 9);
        juce::MemoryBlock state;
        juce::AudioProcessor::copyXmlToBinary (xml, state);
        bool ok = true;
        for (int i = 0; i < 40; ++i)
        {
            auto p = make (fm, desc, 48000.0, 256, err);
            if (p == nullptr) { ok = false; break; }
            p->setPlayConfigDetails (2, 2, 48000.0, 256);
            p->prepareToPlay (48000.0, 256);
            p->setStateInformation (state.getData(), (int) state.getSize());
            juce::AudioBuffer<float> buf (2, 256);
            juce::MidiBuffer midi;
            for (int b = 0; b < i % 7; ++b) { buf.clear(); p->processBlock (buf, midi); if (! finite (buf)) ok = false; }
            if (i % 3 == 0) { std::unique_ptr<juce::AudioProcessorEditor> ed (p->createEditorIfNeeded()); }
            if (i % 4 == 0) p->prepareToPlay (96000.0, 512); // sample-rate change while the source is loading
            juce::Thread::sleep ((juce::uint32) (i % 5) * 3);
            // destroyed here, possibly mid-load / mid-render
        }
        CHECK (ok, "40 create/load/render/destroy cycles with the instance closed mid-work: no crash, no hang");
    }

    std::printf ("\n%s (%d failures)\n", failures == 0 ? "ALL PASSED" : "FAILED", failures);
    return failures == 0 ? 0 : 1;
}
