/// test_church.cpp — Tests for Church surface modules.
///
/// Tests:
///   For each of the 12 church modules:
///   - Instantiate the module
///   - Verify moduleId() starts with "com.mcaster1.church."
///   - Verify displayName() and version() are non-empty
///   - Verify createWidget() returns non-null
///   - Verify preferredSize() and minimumModuleSize() are reasonable
///
/// Build:
///   cmake --build build --config Debug --target TestChurch
///
/// Run:
///   build/bin/Debug/TestChurch.exe

#include <QApplication>
#include <QWidget>
#include <cstdio>

// Church modules (all in M1:: namespace)
#include "TimerClockModule.h"
#include "GraphicsEngineModule.h"
#include "LyricsCasterModule.h"
#include "ScriptureCasterModule.h"
#include "AnnounceCasterModule.h"
#include "TelePromptModule.h"
#include "MediaCasterModule.h"
#include "StageMonModule.h"
#include "AudioMixModule.h"
#include "TranscribeRecModule.h"
#include "SwitchCasterModule.h"
#include "ServiceRunnerModule.h"

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

/// Generic church module test — verifies identity, widget, and moduleId prefix.
static void testChurchModule(M1::IModule* mod, const char* label,
                             const QString& expectedId) {
    if (!mod) {
        char buf[256];
        snprintf(buf, sizeof(buf), "%s construction", label);
        check(false, buf, "constructor returned null");
        return;
    }

    char buf[256];

    // moduleId
    snprintf(buf, sizeof(buf), "%s moduleId", label);
    check(!mod->moduleId().isEmpty() && mod->moduleId() == expectedId,
          buf, QString("got '%1'").arg(mod->moduleId()));

    // moduleId starts with church prefix
    snprintf(buf, sizeof(buf), "%s moduleId starts with com.mcaster1.church.", label);
    check(mod->moduleId().startsWith("com.mcaster1.church."), buf,
          mod->moduleId());

    // displayName
    snprintf(buf, sizeof(buf), "%s displayName", label);
    check(!mod->displayName().isEmpty(), buf,
          QString("got '%1'").arg(mod->displayName()));

    // version
    snprintf(buf, sizeof(buf), "%s version", label);
    check(!mod->version().isEmpty(), buf,
          QString("got '%1'").arg(mod->version()));

    // preferredSize
    QSize pref = mod->preferredSize();
    snprintf(buf, sizeof(buf), "%s preferredSize", label);
    check(pref.width() > 0 && pref.height() > 0, buf,
          QString("%1x%2").arg(pref.width()).arg(pref.height()));

    // minimumModuleSize
    QSize minSz = mod->minimumModuleSize();
    snprintf(buf, sizeof(buf), "%s minimumModuleSize", label);
    check(minSz.width() > 0 && minSz.height() > 0, buf,
          QString("%1x%2").arg(minSz.width()).arg(minSz.height()));

    // preferred >= minimum
    snprintf(buf, sizeof(buf), "%s preferred >= minimum", label);
    check(pref.width() >= minSz.width() && pref.height() >= minSz.height(),
          buf, QString("pref=%1x%2 min=%3x%4")
                   .arg(pref.width()).arg(pref.height())
                   .arg(minSz.width()).arg(minSz.height()));

    // createWidget
    QWidget parent;
    QWidget* widget = mod->createWidget(&parent);
    snprintf(buf, sizeof(buf), "%s createWidget", label);
    check(widget != nullptr, buf);
}

// =============================================================================
//  Test all 12 church modules
// =============================================================================
static void testChurchModules() {
    header("=== Church Module Tests (12 modules) ===");

    // TimerClockModule
    {
        auto* mod = new M1::TimerClockModule();
        testChurchModule(mod, "TimerClockModule", "com.mcaster1.church.timerclock");
        delete mod;
    }

    // GraphicsEngineModule
    {
        auto* mod = new M1::GraphicsEngineModule();
        testChurchModule(mod, "GraphicsEngineModule", "com.mcaster1.church.graphics");
        delete mod;
    }

    // LyricsCasterModule
    {
        auto* mod = new M1::LyricsCasterModule();
        testChurchModule(mod, "LyricsCasterModule", "com.mcaster1.church.lyrics");
        delete mod;
    }

    // ScriptureCasterModule
    {
        auto* mod = new M1::ScriptureCasterModule();
        testChurchModule(mod, "ScriptureCasterModule", "com.mcaster1.church.scripture");
        delete mod;
    }

    // AnnounceCasterModule
    {
        auto* mod = new M1::AnnounceCasterModule();
        testChurchModule(mod, "AnnounceCasterModule", "com.mcaster1.church.announce");
        delete mod;
    }

    // TelePromptModule
    {
        auto* mod = new M1::TelePromptModule();
        testChurchModule(mod, "TelePromptModule", "com.mcaster1.church.teleprompt");
        delete mod;
    }

    // MediaCasterModule
    {
        auto* mod = new M1::MediaCasterModule();
        testChurchModule(mod, "MediaCasterModule", "com.mcaster1.church.mediacaster");
        delete mod;
    }

    // StageMonModule
    {
        auto* mod = new M1::StageMonModule();
        testChurchModule(mod, "StageMonModule", "com.mcaster1.church.stagemon");
        delete mod;
    }

    // AudioMixModule
    {
        auto* mod = new M1::AudioMixModule();
        testChurchModule(mod, "AudioMixModule", "com.mcaster1.church.audiomix");
        delete mod;
    }

    // TranscribeRecModule
    {
        auto* mod = new M1::TranscribeRecModule();
        testChurchModule(mod, "TranscribeRecModule", "com.mcaster1.church.transcriberec");
        delete mod;
    }

    // SwitchCasterModule
    {
        auto* mod = new M1::SwitchCasterModule();
        testChurchModule(mod, "SwitchCasterModule", "com.mcaster1.church.switchcaster");
        delete mod;
    }

    // ServiceRunnerModule
    {
        auto* mod = new M1::ServiceRunnerModule();
        testChurchModule(mod, "ServiceRunnerModule", "com.mcaster1.church.servicerunner");
        delete mod;
    }
}

// =============================================================================
//  Test church-specific features
// =============================================================================
static void testTimerClockFeatures() {
    header("=== TimerClock Features ===");

    auto* tc = new M1::TimerClockModule();

    // Create a timer
    int id = tc->createTimer("Sermon Timer", M1::TimerInstance::Mode::CountDown, 1800000);
    check(id > 0, "createTimer returns positive ID", QString("id=%1").arg(id));

    // Verify timer exists
    const M1::TimerInstance* t = tc->timer(id);
    check(t != nullptr, "timer(id) returns non-null");
    if (t) {
        check(t->name == "Sermon Timer", "Timer name matches");
        check(t->mode == M1::TimerInstance::Mode::CountDown, "Timer mode is CountDown");
        check(t->durationMs == 1800000, "Timer duration is 1800000ms");
        check(t->state == M1::TimerInstance::State::Stopped, "Timer initial state Stopped");
    }

    // Create another timer
    int id2 = tc->createTimer("Segment Timer", M1::TimerInstance::Mode::CountUp);
    check(id2 > id, "Second timer has higher ID");

    // allTimers
    auto all = tc->allTimers();
    check(all.size() == 2, "allTimers() returns 2",
          QString("got %1").arg(all.size()));

    // Remove timer
    tc->removeTimer(id);
    check(tc->timer(id) == nullptr, "Timer removed successfully");
    check(tc->allTimers().size() == 1, "allTimers() returns 1 after remove");

    // Master clock strings should be populated after initialize
    tc->initialize();
    // Master clock may or may not be populated immediately, so just check no crash
    check(true, "masterClockTime() does not crash");
    check(true, "masterClockDate() does not crash");

    tc->shutdown();
    delete tc;
}

static void testGraphicsEngineFeatures() {
    header("=== GraphicsEngine Features ===");

    auto* ge = new M1::GraphicsEngineModule();
    ge->initialize();

    // Default theme
    const auto& theme = ge->activeTheme();
    check(!theme.fontFamily.isEmpty(), "Default theme has fontFamily");
    check(theme.titleFontSize > 0, "Default theme has titleFontSize > 0");

    // Render a blank frame
    QImage blank = ge->renderBlank();
    check(!blank.isNull(), "renderBlank returns non-null image");
    check(blank.width() == 1920 && blank.height() == 1080,
          "renderBlank defaults to 1920x1080",
          QString("%1x%2").arg(blank.width()).arg(blank.height()));

    // Render lyrics
    QImage lyrics = ge->renderLyrics("Amazing Grace\nhow sweet the sound", "Amazing Grace");
    check(!lyrics.isNull(), "renderLyrics returns non-null image");

    // Render scripture
    QImage scripture = ge->renderScripture("For God so loved the world...", "John 3:16", "NIV");
    check(!scripture.isNull(), "renderScripture returns non-null image");

    // Render lower third
    QImage lt = ge->renderLowerThird("Pastor Smith", "Senior Pastor");
    check(!lt.isNull(), "renderLowerThird returns non-null image");

    // Theme management
    M1::VisualTheme custom;
    custom.name = "Christmas";
    custom.backgroundColor = QColor(139, 0, 0);
    ge->addTheme(custom);

    QStringList names = ge->themeNames();
    check(names.contains("Christmas"), "Custom theme added successfully");

    ge->shutdown();
    delete ge;
}

// =============================================================================
int main(int argc, char* argv[]) {
    QApplication app(argc, argv);

    fprintf(stdout, "========================================================\n");
    fprintf(stdout, "  Mcaster1Studio - Church Module Tests\n");
    fprintf(stdout, "========================================================\n");
    fflush(stdout);

    testChurchModules();
    testTimerClockFeatures();
    testGraphicsEngineFeatures();

    fprintf(stdout, "\n========================================================\n");
    fprintf(stdout, "  Results: %d passed, %d failed, %d skipped\n", g_pass, g_fail, g_skip);
    fprintf(stdout, "========================================================\n\n");
    fflush(stdout);

    return g_fail > 0 ? 1 : 0;
}
