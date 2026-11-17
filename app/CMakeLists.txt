cmake_minimum_required(VERSION 3.22)

set(LTC_VERSION 0.1.0)          # versión de paquete (macOS)
set(LTC_VERSION_SHORT "0.1")    # versión osc[AV]lab (v0.1, v0.2, ...)
project(OSCAVLAB_LTC VERSION ${LTC_VERSION} LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "")

# JUCE: usa ./JUCE si existe (offline), si no lo descarga una vez.
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/JUCE/CMakeLists.txt")
    add_subdirectory(JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.4
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

option(LTC_APP "Compilar la app" ON)
if(LTC_APP)
    juce_add_gui_app(OscAVlabLTC
        PRODUCT_NAME "osc[AV]lab LTC"
        COMPANY_NAME "osc[AV]lab"
        BUNDLE_ID "com.oscavlab.ltc"
        VERSION ${LTC_VERSION}
        MICROPHONE_PERMISSION_ENABLED TRUE
        MICROPHONE_PERMISSION_TEXT "osc[AV]lab LTC necesita la entrada de audio para leer el LTC.")

    juce_generate_juce_header(OscAVlabLTC)
    juce_add_binary_data(LtcAssets SOURCES assets/logo-oscavlab.png)

    target_sources(OscAVlabLTC PRIVATE
        Source/Main.cpp
        Source/Engine.cpp
        Source/MidiOut.cpp
        Source/MainComponent.cpp)

    target_compile_definitions(OscAVlabLTC PRIVATE
        JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:OscAVlabLTC,JUCE_PRODUCT_NAME>"
        JUCE_APPLICATION_VERSION_STRING="$<TARGET_PROPERTY:OscAVlabLTC,JUCE_VERSION>"
        OSCAVLAB_VERSION="${LTC_VERSION_SHORT}"
        JUCE_USE_CURL=0
        JUCE_WEB_BROWSER=0)

    target_link_libraries(OscAVlabLTC PRIVATE
        LtcAssets
        juce::juce_audio_utils
        juce::juce_audio_devices
        juce::juce_audio_formats
        juce::juce_gui_extra
        PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
endif()

# Pruebas del motor: cmake -B build -DLTC_TESTS=ON && cmake --build build --target LtcTests
option(LTC_TESTS "Compilar pruebas del motor" OFF)
if(LTC_TESTS)
    juce_add_console_app(LtcTests PRODUCT_NAME "LtcTests")
    juce_generate_juce_header(LtcTests)
    target_sources(LtcTests PRIVATE tests/LtcTests.cpp Source/Engine.cpp Source/MidiOut.cpp)
    target_compile_definitions(LtcTests PRIVATE JUCE_USE_CURL=0 JUCE_WEB_BROWSER=0)
    target_link_libraries(LtcTests PRIVATE juce::juce_audio_utils juce::juce_audio_devices
        PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
endif()
