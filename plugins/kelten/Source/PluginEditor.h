#pragma once

#include <tm/TmEditor.h>
#include "PluginProcessor.h"

class KeltenEditor : public tmk::Editor
{
public:
    explicit KeltenEditor (KeltenProcessor& p);

private:
    void layoutBody (juce::Rectangle<int> body) override;
    static tmk::Editor::Config makeConfig (KeltenProcessor& p);
    tmk::ScopeView view;
};
