#pragma once

#include <tm/TmEditor.h>
#include "PluginProcessor.h"

class RoemerEditor : public tmk::Editor
{
public:
    explicit RoemerEditor (RoemerProcessor& p);

private:
    void layoutBody (juce::Rectangle<int> body) override;
    static tmk::Editor::Config makeConfig (RoemerProcessor& p);
    tmk::ScopeView view;
};
