// Renders the plugin editor off-screen to a PNG: jmc_snapshot out.png [audio.wav] [adv] [char=N]
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <cstdio>
#include "PluginProcessor.h"
#include "PluginEditor.h"

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    if (argc < 2) { std::printf ("usage: jmc_snapshot out.png [audio.wav] [adv] [char=N]\n"); return 2; }
    JMCReverseProcessor p;
    p.prepareToPlay (48000.0, 512);
    bool adv = false;
    for (int i = 2; i < argc; ++i)
    {
        juce::String a (argv[i]);
        if (a == "adv") adv = true;
        else if (a.startsWith ("char=")) p.applyCharacter (a.substring (5).getIntValue());
        else if (juce::File (a).existsAsFile()) p.loadFile (juce::File (a));
    }
    for (int i = 0; i < 300; ++i)
    {
        p.updateRenderIfNeeded();
        if (p.getState() == jmc::EngineState::Ready || p.getState() == jmc::EngineState::Empty) { if (i > 5) break; }
        juce::Thread::sleep (10);
    }
    std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
    if (adv) if (auto* e = dynamic_cast<JMCReverseEditor*> (ed.get())) { (void) e; p.apvts.state.setProperty ("uiAdvanced", true, nullptr); ed.reset (p.createEditor()); }
    juce::MessageManager::getInstance()->runDispatchLoopUntil (500);
    auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.5f);
    juce::File out (argv[1]);
    out.deleteFile();
    juce::PNGImageFormat png;
    std::unique_ptr<juce::FileOutputStream> os (out.createOutputStream());
    png.writeImageToStream (img, *os);
    std::printf ("wrote %s (%dx%d)\n", argv[1], img.getWidth(), img.getHeight());
    return 0;
}
