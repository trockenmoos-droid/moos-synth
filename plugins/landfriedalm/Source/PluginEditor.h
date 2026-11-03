#pragma once

#include <tm/TmEditor.h>
#include "PluginProcessor.h"

class AlmEditor : public tmk::Editor
{
public:
    explicit AlmEditor (AlmProcessor& p);

private:
    void layoutBody (juce::Rectangle<int> body) override;
    static tmk::Editor::Config makeConfig (AlmProcessor& p);
    tmk::ScopeView view;
};
