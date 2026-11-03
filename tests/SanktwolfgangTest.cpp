#include "TestUtil.h"
#include "../plugins/sanktwolfgang/Source/PluginProcessor.h"
#include "../plugins/sanktwolfgang/Source/PluginEditor.h"

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    juce::File dir (argc > 1 ? argv[1] : "/tmp/tmtest");
    dir.createDirectory();
    WolfgangProcessor proc;
    tt::synthSuite<WolfgangEditor> (proc, dir, "sanktwolfgang", "sanktwolfgang", { 0, 1, 2, 5, 6, 8 }, { 48, 55, 60, 64 }, 4.0, 8.0);
    std::printf ("failures=%d\n", tt::failures);
    return tt::failures;
}
