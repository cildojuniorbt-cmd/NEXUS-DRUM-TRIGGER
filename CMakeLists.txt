cmake_minimum_required(VERSION 3.22)
project(NexusDrumTrigger VERSION 0.3.1 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Testes do núcleo de áudio (não precisam de JUCE nem de placa de som)
enable_testing()
add_executable(test_core tests/test_core.cpp)
add_test(NAME core COMMAND test_core)

# App (JUCE)
option(NEXUS_BUILD_APP "Compilar o app com interface (JUCE)" ON)
if(NEXUS_BUILD_APP)
  include(FetchContent)
  FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.4
    GIT_SHALLOW ON)
  FetchContent_MakeAvailable(JUCE)

  juce_add_gui_app(NexusDrumTrigger
    PRODUCT_NAME "Nexus Drum Trigger"
    COMPANY_NAME "Nexus Works Digital")

  juce_add_binary_data(NexusAssets SOURCES assets/kit.png)

  target_sources(NexusDrumTrigger PRIVATE
    app/Main.cpp
    app/MainComponent.cpp)

  target_compile_definitions(NexusDrumTrigger PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_APPLICATION_NAME_STRING="Nexus Drum Trigger"
    JUCE_APPLICATION_VERSION_STRING="${PROJECT_VERSION}")

  target_include_directories(NexusDrumTrigger PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

  target_link_libraries(NexusDrumTrigger
    PRIVATE NexusAssets
            juce::juce_audio_utils
    PUBLIC  juce::juce_recommended_config_flags
            juce::juce_recommended_warning_flags)
endif()
