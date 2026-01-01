#pragma once
#include <QDialog>
#include <QStringList>

class QLineEdit;
class QListWidget;
class QDialogButtonBox;

/// CustomSurfaceDialog — lets the user name a new surface and pick its modules.
///
/// The dialog shows a checkable list of all known built-in modules.
/// surfaceName() and selectedModuleIds() return the user's choices after accept().
class CustomSurfaceDialog : public QDialog {
    Q_OBJECT

public:
    explicit CustomSurfaceDialog(QWidget* parent = nullptr);

    QString     surfaceName()       const;
    QStringList selectedModuleIds() const;

private:
    QLineEdit*        m_nameEdit   = nullptr;
    QListWidget*      m_moduleList = nullptr;
    QDialogButtonBox* m_buttons    = nullptr;
};
