#pragma once
#include "IModule.h"
#include "IPlugin.h"
#include <QSettings>

class QWidget;
class QLabel;

namespace Sample {

/// SampleModule — minimal working Mcaster1Studio plugin.
///
/// Demonstrates:
///   - Implementing M1::IModule
///   - createWidget() returning a QWidget
///   - C ABI factory exports (mcaster1_plugin_info, mcaster1_create_module)
///
/// Build as a DLL and drop into Mcaster1Studio's plugins/modules/ folder.
class SampleModule : public M1::IModule {
    Q_OBJECT

public:
    explicit SampleModule(QObject* parent = nullptr);
    ~SampleModule() override = default;

    // ── IModule identity ──────────────────────────────────────────────────
    QString  moduleId()      const override { return "com.example.sample"; }
    QString  displayName()   const override { return "Sample Module"; }
    QSize    preferredSize() const override { return {300, 200}; }

    // ── IModule lifecycle ─────────────────────────────────────────────────
    QWidget* createWidget(QWidget* parent) override;
    void     initialize()                  override;
    void     shutdown()                    override;
    void     saveState(QSettings& s)       override;
    void     loadState(QSettings& s)       override;

private:
    QLabel* m_label = nullptr;
};

} // namespace Sample
