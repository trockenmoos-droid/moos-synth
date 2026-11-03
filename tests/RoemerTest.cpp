#include "TestUtil.h"
#include "../plugins/roemer/Source/PluginProcessor.h"
#include "../plugins/roemer/Source/PluginEditor.h"

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    juce::File dir (argc > 1 ? argv[1] : "/tmp/tmtest");
    dir.createDirectory();
    RoemerProcessor proc;
    tt::synthSuite<RoemerEditor> (proc, dir, "roemer", "roemer", { 0, 1, 2, 4, 5 }, { 53, 60, 65, 69 }, 3.5, 6.0);
    std::printf ("failures=%d\n", tt::failures);
    return tt::failures;
}
