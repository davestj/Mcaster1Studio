#include "CustomSurfaceDialog.h"
#include "ModuleRegistry.h"
#include <QVBoxLayout>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QDialogButtonBox>
#include <QLabel>

CustomSurfaceDialog::CustomSurfaceDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle("New Custom Surface");
    setMinimumWidth(340);

    auto* layout = new QVBoxLayout(this);
    layout->setSpacing(6);

    layout->addWidget(new QLabel("Surface Name:", this));
    m_nameEdit = new QLineEdit(this);
    m_nameEdit->setPlaceholderText("My Custom Surface");
    layout->addWidget(m_nameEdit);

    layout->addWidget(new QLabel("Select Modules:", this));
    m_moduleList = new QListWidget(this);
    m_moduleList->setSelectionMode(QAbstractItemView::NoSelection);
    for (const auto& [id, name] : M1::availableModules()) {
        auto* item = new QListWidgetItem(name, m_moduleList);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(Qt::Unchecked);
        item->setData(Qt::UserRole, id);
    }
    layout->addWidget(m_moduleList, 1);

    m_buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(m_buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(m_buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(m_buttons);
}

QString CustomSurfaceDialog::surfaceName() const {
    return m_nameEdit->text().trimmed();
}

QStringList CustomSurfaceDialog::selectedModuleIds() const {
    QStringList ids;
    for (int i = 0; i < m_moduleList->count(); ++i) {
        const auto* item = m_moduleList->item(i);
        if (item && item->checkState() == Qt::Checked)
            ids << item->data(Qt::UserRole).toString();
    }
    return ids;
}
