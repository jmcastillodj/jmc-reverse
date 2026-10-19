#include "Presets.h"
#include "FactoryPresets.h"
#include "PluginProcessor.h"

namespace jmc
{
PresetManager::PresetManager (JMCReverseProcessor& p, juce::File userDir) : proc (p), dir (std::move (userDir)) {}

juce::File PresetManager::defaultUserDir()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
        .getChildFile ("JMC").getChildFile ("JMC REVERSE").getChildFile ("Presets");
}

juce::String PresetManager::sanitiseName (const juce::String& n)
{
    return juce::File::createLegalFileName (n.trim()).substring (0, 60).trim();
}

std::vector<PresetInfo> PresetManager::list() const
{
    std::vector<PresetInfo> out;
    const auto& f = factory::all();
    for (size_t i = 0; i < f.size(); ++i)
        out.push_back ({ f[i].name, f[i].category, true, (int) i, {} });

    juce::Array<juce::File> files = dir.findChildFiles (juce::File::findFiles, false, "*.jmcpreset");
    files.sort();
    for (auto& file : files)
        if (auto xml = juce::XmlDocument::parse (file))
            if (xml->hasTagName ("JMCPreset"))
                out.push_back ({ xml->getStringAttribute ("name", file.getFileNameWithoutExtension()),
                                 xml->getStringAttribute ("category", "User"), false, -1, file });
    return out;
}

bool PresetManager::apply (const PresetInfo& info)
{
    std::vector<std::pair<juce::String, float>> values;
    if (info.factory)
    {
        if (info.factoryIndex < 0 || info.factoryIndex >= (int) factory::all().size())
            return false;
        for (const auto& [id, v] : factory::resolve (factory::all()[(size_t) info.factoryIndex]))
            values.emplace_back (id, v);
    }
    else
    {
        auto xml = juce::XmlDocument::parse (info.file);
        if (xml == nullptr || ! xml->hasTagName ("JMCPreset"))
            return false;
        for (auto* e : xml->getChildWithTagNameIterator ("PARAM"))
            values.emplace_back (e->getStringAttribute ("id"), (float) e->getDoubleAttribute ("value"));
    }
    proc.applyValues (values, info.name);
    return true;
}

bool PresetManager::saveUser (const juce::String& name, const juce::String& category, juce::String& error)
{
    const auto clean = sanitiseName (name);
    if (clean.isEmpty())
    {
        error = "Please enter a preset name.";
        return false;
    }
    if (! dir.createDirectory().wasOk())
    {
        error = "Could not create the preset folder.";
        return false;
    }
    juce::XmlElement root ("JMCPreset");
    root.setAttribute ("version", 1);
    root.setAttribute ("name", clean);
    root.setAttribute ("category", category.isEmpty() ? "User" : category);
    for (auto* p : proc.getParameters())
        if (auto* r = dynamic_cast<juce::RangedAudioParameter*> (p))
        {
            const auto id = r->getParameterID();
            if (id == ids::trimstart || id == ids::trimend)
                continue; // trim belongs to the audio file, not to a preset
            auto* e = root.createNewChildElement ("PARAM");
            e->setAttribute ("id", id);
            e->setAttribute ("value", (double) r->convertFrom0to1 (r->getValue()));
        }
    if (! root.writeTo (dir.getChildFile (clean + ".jmcpreset")))
    {
        error = "Could not write the preset file.";
        return false;
    }
    proc.apvts.state.setProperty ("preset", clean, nullptr);
    return true;
}

bool PresetManager::deleteUser (const PresetInfo& info)
{
    return ! info.factory && info.file.existsAsFile() && info.file.deleteFile();
}
} // namespace jmc
