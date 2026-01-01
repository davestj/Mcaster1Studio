#include "SurfaceWindow.h"
#include "SurfaceWidget.h"
#include <QCloseEvent>
#include <QKeyEvent>
#include <QGuiApplication>
#include <QScreen>

// ─── Constructor ─────────────────────────────────────────────────────────────
SurfaceWindow::SurfaceWindow(SurfaceWidget* surface, QWidget* parent)
    : QMainWindow(parent)
    , m_surface(surface)
{
    setAttribute(Qt::WA_DeleteOnClose, false);  // MainWindow manages lifetime
    setMinimumSize(640, 480);

    // Take ownership of the surface widget
    setCentralWidget(surface);
    surface->show();

    // Window title
    const QString name = surface->customName().isEmpty()
        ? surface->objectName()
        : surface->customName();
    setWindowTitle(QString("Mcaster1Studio \u2014 %1").arg(name));

    // Reasonable default size
    resize(1280, 800);
}

SurfaceWindow::~SurfaceWindow() {
    // Don't delete the surface — MainWindow will reclaim it
    if (m_surface && centralWidget() == m_surface) {
        takeCentralWidget();
    }
}

// ─── Move to screen ──────────────────────────────────────────────────────────
void SurfaceWindow::moveToScreen(int screenIndex) {
    const auto screens = QGuiApplication::screens();
    if (screenIndex < 0 || screenIndex >= screens.size()) return;
    const QRect geo = screens[screenIndex]->availableGeometry();
    move(geo.topLeft());
}

void SurfaceWindow::maximizeOnScreen(int screenIndex) {
    const auto screens = QGuiApplication::screens();
    if (screenIndex < 0 || screenIndex >= screens.size()) return;
    const QRect geo = screens[screenIndex]->availableGeometry();
    setGeometry(geo);
    showMaximized();
}

int SurfaceWindow::currentScreenIndex() const {
    const QScreen* scr = screen();
    if (!scr) return 0;
    return QGuiApplication::screens().indexOf(scr);
}

// ─── Close = dock back (non-destructive) ─────────────────────────────────────
void SurfaceWindow::closeEvent(QCloseEvent* event) {
    event->ignore();  // Don't actually close
    emit dockBackRequested(m_surface);
}

// ─── F11 fullscreen toggle ───────────────────────────────────────────────────
void SurfaceWindow::keyPressEvent(QKeyEvent* event) {
    if (event->key() == Qt::Key_F11) {
        toggleFullscreen();
        return;
    }
    QMainWindow::keyPressEvent(event);
}

void SurfaceWindow::toggleFullscreen() {
    m_isFullscreen = !m_isFullscreen;
    if (m_isFullscreen)
        showFullScreen();
    else
        showNormal();
}
