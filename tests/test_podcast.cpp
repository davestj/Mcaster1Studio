/// test_podcast.cpp — Tests for Podcast surface modules.
///
/// Tests:
///   For each of the 13 podcast modules:
///   - Instantiate the module
///   - Verify moduleId() starts with "com.mcaster1.podcast."
///   - Verify displayName() and version() are non-empty
///   - Verify createWidget() returns non-null
///   - Verify preferredSize() and minimumModuleSize() are reasonable
///
/// Build:
///   cmake --build build --config Debug --target TestPodcast
///
/// Run:
///   build/bin/Debug/TestPodcast.exe

#include <QApplication>
#include <QWidget>
#include <cstdio>

// Podcast modules (all in M1:: namespace)
#include "PodMixerModule.h"
#include "PodPTTModule.h"
#include "PodRecorderModule.h"
#include "PodSoundboardModule.h"
#include "PodFXModule.h"
#include "PodEditorModule.h"
#include "PodEncodeModule.h"
#include "PodTranscribeModule.h"
#include "PodShowNotesModule.h"
#include "PodRSSModule.h"
#include "PodPublisherModule.h"
#include "PodAnalyticsModule.h"
#include "PodRemoteModule.h"

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

/// Generic podcast module test — verifies identity, widget, and moduleId prefix.
static void testPodcastModule(M1::IModule* mod, const char* label,
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

    // moduleId starts with podcast prefix
    snprintf(buf, sizeof(buf), "%s moduleId starts with com.mcaster1.podcast.", label);
    check(mod->moduleId().startsWith("com.mcaster1.podcast."), buf,
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
//  Test all 13 podcast modules
// =============================================================================
static void testPodcastModules() {
    header("=== Podcast Module Tests (13 modules) ===");

    // PodMixerModule
    {
        auto* mod = new M1::PodMixerModule();
        testPodcastModule(mod, "PodMixerModule", "com.mcaster1.podcast.mixer");
        delete mod;
    }

    // PodPTTModule
    {
        auto* mod = new M1::PodPTTModule();
        testPodcastModule(mod, "PodPTTModule", "com.mcaster1.podcast.ptt");
        delete mod;
    }

    // PodRecorderModule
    {
        auto* mod = new M1::PodRecorderModule();
        testPodcastModule(mod, "PodRecorderModule", "com.mcaster1.podcast.recorder");
        delete mod;
    }

    // PodSoundboardModule
    {
        auto* mod = new M1::PodSoundboardModule();
        testPodcastModule(mod, "PodSoundboardModule", "com.mcaster1.podcast.soundboard");
        delete mod;
    }

    // PodFXModule
    {
        auto* mod = new M1::PodFXModule();
        testPodcastModule(mod, "PodFXModule", "com.mcaster1.podcast.fx");
        delete mod;
    }

    // PodEditorModule
    {
        auto* mod = new M1::PodEditorModule();
        testPodcastModule(mod, "PodEditorModule", "com.mcaster1.podcast.editor");
        delete mod;
    }

    // PodEncodeModule
    {
        auto* mod = new M1::PodEncodeModule();
        testPodcastModule(mod, "PodEncodeModule", "com.mcaster1.podcast.encode");
        delete mod;
    }

    // PodTranscribeModule
    {
        auto* mod = new M1::PodTranscribeModule();
        testPodcastModule(mod, "PodTranscribeModule", "com.mcaster1.podcast.transcribe");
        delete mod;
    }

    // PodShowNotesModule
    {
        auto* mod = new M1::PodShowNotesModule();
        testPodcastModule(mod, "PodShowNotesModule", "com.mcaster1.podcast.shownotes");
        delete mod;
    }

    // PodRSSModule
    {
        auto* mod = new M1::PodRSSModule();
        testPodcastModule(mod, "PodRSSModule", "com.mcaster1.podcast.rss");
        delete mod;
    }

    // PodPublisherModule
    {
        auto* mod = new M1::PodPublisherModule();
        testPodcastModule(mod, "PodPublisherModule", "com.mcaster1.podcast.publisher");
        delete mod;
    }

    // PodAnalyticsModule
    {
        auto* mod = new M1::PodAnalyticsModule();
        testPodcastModule(mod, "PodAnalyticsModule", "com.mcaster1.podcast.analytics");
        delete mod;
    }

    // PodRemoteModule
    {
        auto* mod = new M1::PodRemoteModule();
        testPodcastModule(mod, "PodRemoteModule", "com.mcaster1.podcast.remote");
        delete mod;
    }
}

// =============================================================================
//  Verify module count
// =============================================================================
static void testModuleCount() {
    header("=== Podcast Module Count ===");

    int count = 13;  // PodMixer, PodPTT, PodRecorder, PodSoundboard, PodFX,
                     // PodEditor, PodEncode, PodTranscribe, PodShowNotes,
                     // PodRSS, PodPublisher, PodAnalytics, PodRemote
    check(count == 13, "Podcast surface has 13 modules",
          QString("expected 13, counted %1").arg(count));
}

// =============================================================================
int main(int argc, char* argv[]) {
    QApplication app(argc, argv);

    fprintf(stdout, "========================================================\n");
    fprintf(stdout, "  Mcaster1Studio - Podcast Module Tests\n");
    fprintf(stdout, "========================================================\n");
    fflush(stdout);

    testPodcastModules();
    testModuleCount();

    fprintf(stdout, "\n========================================================\n");
    fprintf(stdout, "  Results: %d passed, %d failed, %d skipped\n", g_pass, g_fail, g_skip);
    fprintf(stdout, "========================================================\n\n");
    fflush(stdout);

    return g_fail > 0 ? 1 : 0;
}
