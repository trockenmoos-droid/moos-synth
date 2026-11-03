#include "TestUtil.h"
#include "../plugins/kelten/Source/PluginProcessor.h"
#include "../plugins/kelten/Source/PluginEditor.h"

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    juce::File dir (argc > 1 ? argv[1] : "/tmp/tmtest");
    dir.createDirectory();
    KeltenProcessor proc;
    tt::synthSuite<KeltenEditor> (proc, dir, "kelten", "kelten", { 0, 1, 2, 3, 5 }, { 50, 57, 62 }, 3.5, 6.0);
    std::printf ("failures=%d\n", tt::failures);
    return tt::failures;
}
