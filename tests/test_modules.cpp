/// test_modules.cpp — Tests for broadcast module construction and identity.
///
/// Tests:
///   For each of the 20 broadcast modules:
///   - Instantiate the module
///   - Verify moduleId(), displayName(), version() return non-empty strings
///   - Verify createWidget() returns non-null
///   - Verify preferredSize() and minimumModuleSize() are reasonable
///
/// Build:
///   cmake --build build --config Debug --target TestModules
///
/// Run:
///   build/bin/Debug/TestModules.exe

#include <QApplication>
#include <QWidget>
#include <cstdio>

// Broadcast modules (all in M1:: namespace)
#include "VUMeterModule.h"
#include "DeckModule.h"
#include "DeckAModule.h"
#include "DeckBModule.h"
#include "MediaLibraryModule.h"
#include "EncoderModule.h"
#include "EffectsRackModule.h"
#include "MetadataModule.h"
#include "PlaylistModule.h"
#include "PTTModule.h"
#include "PodcastModule.h"
#include "VideoModule.h"
#include "MonitorModule.h"
#include "ClockModule.h"
#include "CartWallModule.h"
#include "DatabaseModule.h"
#include "HealthModule.h"
#include "AuxDeckModule.h"

// These two are NOT in M1:: namespace
#include "QueueModule.h"
#include "CrossfaderModule.h"

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

/// Generic module identity + widget test.
/// Works for any M1::IModule subclass.
static void testModule(M1::IModule* mod, const char* label,
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

    // preferredSize >= minimumModuleSize
    snprintf(buf, sizeof(buf), "%s preferred >= minimum", label);
    check(pref.width() >= minSz.width() && pref.height() >= minSz.height(),
          buf, QString("pref=%1x%2 min=%3x%4")
                   .arg(pref.width()).arg(pref.height())
                   .arg(minSz.width()).arg(minSz.height()));

    // createWidget — needs a parent widget
    QWidget parent;
    QWidget* widget = mod->createWidget(&parent);
    snprintf(buf, sizeof(buf), "%s createWidget", label);
    check(widget != nullptr, buf);
}

// =============================================================================
//  Test all broadcast modules
// =============================================================================
static void testBroadcastModules() {
    header("=== Broadcast Module Tests ===");

    // VUMeterModule
    {
        auto* mod = new M1::VUMeterModule();
        testModule(mod, "VUMeterModule", "com.mcaster1.vumeter");
        delete mod;
    }

    // DeckModule
    {
        auto* mod = new M1::DeckModule();
        testModule(mod, "DeckModule", "com.mcaster1.deck");
        delete mod;
    }

    // DeckAModule
    {
        auto* mod = new M1::DeckAModule();
        testModule(mod, "DeckAModule", "com.mcaster1.deck.a");
        delete mod;
    }

    // DeckBModule
    {
        auto* mod = new M1::DeckBModule();
        testModule(mod, "DeckBModule", "com.mcaster1.deck.b");
        delete mod;
    }

    // MediaLibraryModule
    {
        auto* mod = new M1::MediaLibraryModule();
        testModule(mod, "MediaLibraryModule", "com.mcaster1.library");
        delete mod;
    }

    // EncoderModule
    {
        auto* mod = new M1::EncoderModule();
        testModule(mod, "EncoderModule", "com.mcaster1.encoder");
        delete mod;
    }

    // EffectsRackModule
    {
        auto* mod = new M1::EffectsRackModule();
        testModule(mod, "EffectsRackModule", "com.mcaster1.effects");
        delete mod;
    }

    // MetadataModule
    {
        auto* mod = new M1::MetadataModule();
        testModule(mod, "MetadataModule", "com.mcaster1.metadata");
        delete mod;
    }

    // PlaylistModule
    {
        auto* mod = new M1::PlaylistModule();
        testModule(mod, "PlaylistModule", "com.mcaster1.playlist");
        delete mod;
    }

    // PTTModule
    {
        auto* mod = new M1::PTTModule();
        testModule(mod, "PTTModule", "com.mcaster1.ptt");
        delete mod;
    }

    // PodcastModule
    {
        auto* mod = new M1::PodcastModule();
        testModule(mod, "PodcastModule", "com.mcaster1.podcast");
        delete mod;
    }

    // VideoModule
    {
        auto* mod = new M1::VideoModule();
        testModule(mod, "VideoModule", "com.mcaster1.video");
        delete mod;
    }

    // MonitorModule
    {
        auto* mod = new M1::MonitorModule();
        testModule(mod, "MonitorModule", "com.mcaster1.monitor");
        delete mod;
    }

    // ClockModule
    {
        auto* mod = new M1::ClockModule();
        testModule(mod, "ClockModule", "com.mcaster1.clock");
        delete mod;
    }

    // CartWallModule
    {
        auto* mod = new M1::CartWallModule();
        testModule(mod, "CartWallModule", "com.mcaster1.cartwall");
        delete mod;
    }

    // DatabaseModule
    {
        auto* mod = new M1::DatabaseModule();
        testModule(mod, "DatabaseModule", "com.mcaster1.database");
        delete mod;
    }

    // HealthModule
    {
        auto* mod = new M1::HealthModule();
        testModule(mod, "HealthModule", "com.mcaster1.health");
        delete mod;
    }

    // AuxDeckModule
    {
        auto* mod = new M1::AuxDeckModule();
        testModule(mod, "AuxDeckModule", "com.mcaster1.auxdeck");
        delete mod;
    }

    // QueueModule (global namespace, not M1::)
    {
        auto* mod = new ::QueueModule();
        testModule(mod, "QueueModule", "com.mcaster1.queue");
        delete mod;
    }

    // CrossfaderModule (global namespace, not M1::)
    {
        auto* mod = new ::CrossfaderModule();
        testModule(mod, "CrossfaderModule", "com.mcaster1.crossfader");
        delete mod;
    }
}

// =============================================================================
int main(int argc, char* argv[]) {
    QApplication app(argc, argv);

    fprintf(stdout, "========================================================\n");
    fprintf(stdout, "  Mcaster1Studio - Broadcast Module Tests\n");
    fprintf(stdout, "========================================================\n");
    fflush(stdout);

    testBroadcastModules();

    fprintf(stdout, "\n========================================================\n");
    fprintf(stdout, "  Results: %d passed, %d failed, %d skipped\n", g_pass, g_fail, g_skip);
    fprintf(stdout, "========================================================\n\n");
    fflush(stdout);

    return g_fail > 0 ? 1 : 0;
}
