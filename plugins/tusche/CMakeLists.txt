set(TM_VST3_CATS Instrument Sampler)
tm_add_plugin(TUSCHE "TUSCHE" Tsch TRUE 0.1.0
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

# Christophs Tuschezeichnungen (Papier + fünf Blätter für die Anzeige)
juce_add_binary_data(tusche_assets
    HEADER_NAME TuscheAssets.h
    NAMESPACE TuscheAssets
    SOURCES
        assets/papier.jpg
        assets/blatt1.jpg
        assets/blatt2.jpg
        assets/blatt3.jpg
        assets/blatt4.jpg
        assets/blatt5.jpg)
set_target_properties(tusche_assets PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_link_libraries(TUSCHE PRIVATE tusche_assets)
