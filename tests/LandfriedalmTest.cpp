#include "TestUtil.h"
#include "../plugins/landfriedalm/Source/PluginProcessor.h"
#include "../plugins/landfriedalm/Source/PluginEditor.h"

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    juce::File dir (argc > 1 ? argv[1] : "/tmp/tmtest");
    dir.createDirectory();
    AlmProcessor proc;
    tt::synthSuite<AlmEditor> (proc, dir, "landfriedalm", "landfriedalm", { 0, 2, 4, 6, 7 }, { 55, 62, 67, 71 }, 3.0, 5.0);
    std::printf ("failures=%d\n", tt::failures);
    return tt::failures;
}
