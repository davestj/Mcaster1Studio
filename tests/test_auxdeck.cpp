/// test_auxdeck.cpp — Tests for AuxDeckModule.
///
/// Tests:
///   - Construction + identity (moduleId, displayName, version)
///   - setDeckName / deckName
///   - setAirOutDevice / airOutDeviceIndex
///   - setCueOutDevice / cueOutDeviceIndex
///   - setVolume / volume
///   - saveState / loadState round-trip
///   - createWidget returns non-null
///
/// Build:
///   cmake --build build --config Debug --target TestAuxDeck
///
/// Run:
///   build/bin/Debug/TestAuxDeck.exe

#include <QApplication>
#include <QWidget>
#include <QSettings>
#include <QTemporaryDir>
#include <cstdio>
#include <cmath>

#include "AuxDeckModule.h"

static int g_pass = 0;
static int g_fail = 0;
static int g_skip = 0;

static void check(bool ok, const char* name, const QString& detail = {}) {
    if (ok) {
        fprintf(stdout, "  [PASS] %s\n", name);
        ++g_pass;
    } else {
        fprintf(stdout, "  [FAIL] %s -- %s\n", name, detail.toUtf8().constData());
        ++g_fail;
    }
    fflush(stdout);
}

static void skip(const char* name, const QString& reason) {
    fprintf(stdout, "  [SKIP] %s -- %s\n", name, reason.toUtf8().constData());
    ++g_skip;
    fflush(stdout);
}

static void header(const char* msg) {
    fprintf(stdout, "\n%s\n", msg);
    fflush(stdout);
}

// =============================================================================
//  Test 1: Construction + Identity
// =============================================================================
static void testIdentity() {
    header("=== Test 1: AuxDeckModule Identity ===");

    auto* mod = new M1::AuxDeckModule();

    check(mod->moduleId() == "com.mcaster1.auxdeck",
          "moduleId is com.mcaster1.auxdeck",
          mod->moduleId());

    check(!mod->displayName().isEmpty(),
          "displayName is non-empty",
          mod->displayName());

    check(mod->version() == "1.0.0",
          "version is 1.0.0",
          mod->version());

    check(mod->vendor() == "Mcaster1",
          "vendor is Mcaster1",
          mod->vendor());

    QSize pref = mod->preferredSize();
    check(pref.width() == 400 && pref.height() == 300,
          "preferredSize is 400x300",
          QString("%1x%2").arg(pref.width()).arg(pref.height()));

    QSize minSz = mod->minimumModuleSize();
    check(minSz.width() == 300 && minSz.height() == 200,
          "minimumModuleSize is 300x200",
          QString("%1x%2").arg(minSz.width()).arg(minSz.height()));

    delete mod;
}

// =============================================================================
//  Test 2: Deck Name
// =============================================================================
static void testDeckName() {
    header("=== Test 2: Deck Name ===");

    auto* mod = new M1::AuxDeckModule();

    // Default name
    check(mod->deckName() == "AUX Deck",
          "Default deck name is 'AUX Deck'",
          mod->deckName());

    // Set custom name
    mod->setDeckName("Jingle Deck");
    check(mod->deckName() == "Jingle Deck",
          "setDeckName changes name to 'Jingle Deck'",
          mod->deckName());

    // Set another name
    mod->setDeckName("Interview Playback");
    check(mod->deckName() == "Interview Playback",
          "setDeckName changes name to 'Interview Playback'",
          mod->deckName());

    // Empty name
    mod->setDeckName("");
    check(mod->deckName().isEmpty(),
          "setDeckName allows empty name",
          mod->deckName());

    delete mod;
}

// =============================================================================
//  Test 3: Audio Device Configuration
// =============================================================================
static void testDeviceConfig() {
    header("=== Test 3: Audio Device Configuration ===");

    auto* mod = new M1::AuxDeckModule();

    // Default device indices
    check(mod->airOutDeviceIndex() == -1,
          "Default airOutDeviceIndex is -1 (global default)",
          QString("%1").arg(mod->airOutDeviceIndex()));

    check(mod->cueOutDeviceIndex() == -1,
          "Default cueOutDeviceIndex is -1 (global default)",
          QString("%1").arg(mod->cueOutDeviceIndex()));

    // Set AIR OUT device
    mod->setAirOutDevice(2);
    check(mod->airOutDeviceIndex() == 2,
          "setAirOutDevice(2) changes to 2",
          QString("%1").arg(mod->airOutDeviceIndex()));

    mod->setAirOutDevice(0);
    check(mod->airOutDeviceIndex() == 0,
          "setAirOutDevice(0) changes to 0",
          QString("%1").arg(mod->airOutDeviceIndex()));

    // Set CUE OUT device
    mod->setCueOutDevice(5);
    check(mod->cueOutDeviceIndex() == 5,
          "setCueOutDevice(5) changes to 5",
          QString("%1").arg(mod->cueOutDeviceIndex()));

    // Reset to default
    mod->setAirOutDevice(-1);
    check(mod->airOutDeviceIndex() == -1,
          "setAirOutDevice(-1) resets to default",
          QString("%1").arg(mod->airOutDeviceIndex()));

    mod->setCueOutDevice(-1);
    check(mod->cueOutDeviceIndex() == -1,
          "setCueOutDevice(-1) resets to default",
          QString("%1").arg(mod->cueOutDeviceIndex()));

    delete mod;
}

// =============================================================================
//  Test 4: Volume
// =============================================================================
static void testVolume() {
    header("=== Test 4: Volume ===");

    auto* mod = new M1::AuxDeckModule();

    // Default volume
    check(std::fabs(mod->volume() - 1.0f) < 0.001f,
          "Default volume is 1.0",
          QString("%1").arg(mod->volume()));

    // Set volume
    mod->setVolume(0.5f);
    check(std::fabs(mod->volume() - 0.5f) < 0.001f,
          "setVolume(0.5) changes to 0.5",
          QString("%1").arg(mod->volume()));

    // Set volume to 0
    mod->setVolume(0.0f);
    check(std::fabs(mod->volume()) < 0.001f,
          "setVolume(0.0) changes to 0.0",
          QString("%1").arg(mod->volume()));

    // Set volume to max
    mod->setVolume(1.0f);
    check(std::fabs(mod->volume() - 1.0f) < 0.001f,
          "setVolume(1.0) changes to 1.0",
          QString("%1").arg(mod->volume()));

    // Peak levels default to 0
    check(std::fabs(mod->peakL()) < 0.001f,
          "Default peakL is 0",
          QString("%1").arg(mod->peakL()));

    check(std::fabs(mod->peakR()) < 0.001f,
          "Default peakR is 0",
          QString("%1").arg(mod->peakR()));

    delete mod;
}

// =============================================================================
//  Test 5: State Persistence (saveState / loadState)
// =============================================================================
static void testStatePersistence() {
    header("=== Test 5: State Persistence ===");

    QTemporaryDir tmpDir;
    if (!tmpDir.isValid()) {
        skip("State persistence", "Could not create temp directory");
        return;
    }

    QString settingsPath = tmpDir.path() + "/test_auxdeck.ini";

    // Configure a module with non-default values
    {
        auto* mod = new M1::AuxDeckModule();
        mod->setDeckName("Recording Deck");
        mod->setAirOutDevice(3);
        mod->setCueOutDevice(7);
        mod->setVolume(0.75f);

        // Save state
        QSettings settings(settingsPath, QSettings::IniFormat);
        settings.beginGroup("AuxDeck_Test");
        mod->saveState(settings);
        settings.endGroup();
        settings.sync();

        check(QFile::exists(settingsPath), "Settings file created");
        delete mod;
    }

    // Load into a fresh module
    {
        auto* mod = new M1::AuxDeckModule();

        // Verify defaults first
        check(mod->deckName() == "AUX Deck",
              "Fresh module has default name before load");

        QSettings settings(settingsPath, QSettings::IniFormat);
        settings.beginGroup("AuxDeck_Test");
        mod->loadState(settings);
        settings.endGroup();

        // Verify loaded values
        check(mod->deckName() == "Recording Deck",
              "loadState restores deck name",
              mod->deckName());

        check(mod->airOutDeviceIndex() == 3,
              "loadState restores airOutDevice",
              QString("%1").arg(mod->airOutDeviceIndex()));

        check(mod->cueOutDeviceIndex() == 7,
              "loadState restores cueOutDevice",
              QString("%1").arg(mod->cueOutDeviceIndex()));

        check(std::fabs(mod->volume() - 0.75f) < 0.01f,
              "loadState restores volume",
              QString("%1").arg(mod->volume()));

        delete mod;
    }
}

// =============================================================================
//  Test 6: Widget Creation
// =============================================================================
static void testWidget() {
    header("=== Test 6: Widget Creation ===");

    auto* mod = new M1::AuxDeckModule();

    QWidget parent;
    QWidget* widget = mod->createWidget(&parent);

    check(widget != nullptr, "createWidget returns non-null");

    if (widget) {
        check(widget->parentWidget() == &parent || widget->parent() == &parent,
              "Widget has correct parent");

        // Widget should have a reasonable minimum size
        QSize ws = widget->minimumSizeHint();
        check(ws.width() >= 0 && ws.height() >= 0,
              "Widget minimumSizeHint is non-negative",
              QString("%1x%2").arg(ws.width()).arg(ws.height()));
    }

    delete mod;
}

// =============================================================================
//  Test 7: Multiple Instances
// =============================================================================
static void testMultipleInstances() {
    header("=== Test 7: Multiple Instances ===");

    auto* deck1 = new M1::AuxDeckModule();
    auto* deck2 = new M1::AuxDeckModule();
    auto* deck3 = new M1::AuxDeckModule();

    // Set different names
    deck1->setDeckName("Jingle Deck");
    deck2->setDeckName("Interview Deck");
    deck3->setDeckName("SFX Deck");

    // Each instance maintains its own state
    check(deck1->deckName() == "Jingle Deck",
          "Instance 1 retains its name");
    check(deck2->deckName() == "Interview Deck",
          "Instance 2 retains its name");
    check(deck3->deckName() == "SFX Deck",
          "Instance 3 retains its name");

    // Set different device configs
    deck1->setAirOutDevice(0);
    deck2->setAirOutDevice(1);
    deck3->setAirOutDevice(2);

    check(deck1->airOutDeviceIndex() == 0, "Instance 1 device 0");
    check(deck2->airOutDeviceIndex() == 1, "Instance 2 device 1");
    check(deck3->airOutDeviceIndex() == 2, "Instance 3 device 2");

    // Set different volumes
    deck1->setVolume(0.3f);
    deck2->setVolume(0.6f);
    deck3->setVolume(0.9f);

    check(std::fabs(deck1->volume() - 0.3f) < 0.01f, "Instance 1 volume 0.3");
    check(std::fabs(deck2->volume() - 0.6f) < 0.01f, "Instance 2 volume 0.6");
    check(std::fabs(deck3->volume() - 0.9f) < 0.01f, "Instance 3 volume 0.9");

    // All share the same moduleId
    check(deck1->moduleId() == deck2->moduleId(),
          "All instances share same moduleId");
    check(deck2->moduleId() == deck3->moduleId(),
          "All instances share same moduleId (2-3)");

    delete deck1;
    delete deck2;
    delete deck3;
}

// =============================================================================
int main(int argc, char* argv[]) {
    QApplication app(argc, argv);

    fprintf(stdout, "========================================================\n");
    fprintf(stdout, "  Mcaster1Studio - AuxDeckModule Tests\n");
    fprintf(stdout, "========================================================\n");
    fflush(stdout);

    testIdentity();
    testDeckName();
    testDeviceConfig();
    testVolume();
    testStatePersistence();
    testWidget();
    testMultipleInstances();

    fprintf(stdout, "\n========================================================\n");
    fprintf(stdout, "  Results: %d passed, %d failed, %d skipped\n", g_pass, g_fail, g_skip);
    fprintf(stdout, "========================================================\n\n");
    fflush(stdout);

    return g_fail > 0 ? 1 : 0;
}
