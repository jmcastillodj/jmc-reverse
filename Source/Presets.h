#pragma once
#include <juce_core/juce_core.h>
#include <vector>

class JMCReverseProcessor;

namespace jmc
{
struct PresetInfo
{
    juce::String name, category;
    bool factory = true;
    int factoryIndex = -1;
    juce::File file;
};

// Factory presets are compiled in; user presets are small XML files (.jmcpreset) that hold
// parameter values only - never audio or file paths - so they work on any machine.
class PresetManager
{
public:
    explicit PresetManager (JMCReverseProcessor&, juce::File userDir = defaultUserDir());
    static juce::File defaultUserDir();

    std::vector<PresetInfo> list() const;
    bool apply (const PresetInfo&);
    bool saveUser (const juce::String& name, const juce::String& category, juce::String& error);
    bool deleteUser (const PresetInfo&);
    static juce::String sanitiseName (const juce::String&);
    juce::File getUserDir() const { return dir; }

private:
    JMCReverseProcessor& proc;
    juce::File dir;
};
} // namespace jmc
