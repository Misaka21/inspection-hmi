// src/main.cpp
//
// Application entry point for the Inspection HMI.
//
// Responsibilities:
//   - Initialize Qt application with proper OpenGL surface format for VTK
//   - Apply dark theme stylesheet
//   - Create GatewayClient
//   - Create MainWindow (Engineer mode) and OperatorWindow (Operator mode)
//   - Wire up mode switching signals
//   - Connect gateway client signals to both windows
//   - Enter Qt event loop

#include "core/GatewayClient.h"
#include "ui/MainWindow.h"
#include "ui/operator/OperatorWindow.h"
#include "ui/operator/ControlPanel.h"
#include "ui/operator/ResultPanel.h"
#include "ui/operator/NavMapWidget.h"
#include "scene/QVTKWidget.h"

#include <QApplication>
#include <QSurfaceFormat>

int main(int argc, char* argv[])
{
    // VTK CRITICAL: Must set default surface format BEFORE creating QApplication.
    // QVTKWidget requires a core OpenGL 3.2+ profile with depth/stencil.
    QSurfaceFormat::setDefaultFormat(QVTKWidget::defaultFormat());

    QApplication app(argc, argv);
    app.setApplicationName("Inspection HMI");
    app.setApplicationVersion("1.0");
    app.setOrganizationName("InspectionSystem");

    // -----------------------------------------------------------------------
    // Dark palette — ensures ALL widgets default to dark background.
    // Without this, QScrollArea viewports, QGroupBox interiors, and other
    // container widgets fall back to the platform's white QPalette::Window.
    // -----------------------------------------------------------------------
    QPalette darkPalette;
    darkPalette.setColor(QPalette::Window,          QColor(0x2b, 0x2b, 0x2b));
    darkPalette.setColor(QPalette::WindowText,      QColor(0xe0, 0xe0, 0xe0));
    darkPalette.setColor(QPalette::Base,            QColor(0x1e, 0x1e, 0x1e));
    darkPalette.setColor(QPalette::AlternateBase,   QColor(0x2b, 0x2b, 0x2b));
    darkPalette.setColor(QPalette::ToolTipBase,     QColor(0x3c, 0x3c, 0x3c));
    darkPalette.setColor(QPalette::ToolTipText,     QColor(0xe0, 0xe0, 0xe0));
    darkPalette.setColor(QPalette::Text,            QColor(0xe0, 0xe0, 0xe0));
    darkPalette.setColor(QPalette::Button,          QColor(0x4a, 0x4a, 0x4a));
    darkPalette.setColor(QPalette::ButtonText,      QColor(0xe0, 0xe0, 0xe0));
    darkPalette.setColor(QPalette::BrightText,      Qt::white);
    darkPalette.setColor(QPalette::Link,            QColor(0x21, 0x96, 0xF3));
    darkPalette.setColor(QPalette::Highlight,       QColor(0x21, 0x96, 0xF3));
    darkPalette.setColor(QPalette::HighlightedText, Qt::white);
    darkPalette.setColor(QPalette::PlaceholderText, QColor(0x80, 0x80, 0x80));
    // Disabled state
    darkPalette.setColor(QPalette::Disabled, QPalette::WindowText, QColor(0x66, 0x66, 0x66));
    darkPalette.setColor(QPalette::Disabled, QPalette::Text,       QColor(0x66, 0x66, 0x66));
    darkPalette.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(0x66, 0x66, 0x66));
    app.setPalette(darkPalette);

    // -----------------------------------------------------------------------
    // Dark theme stylesheet — fine-tunes borders, padding, hover effects
    // on top of the dark palette.
    // -----------------------------------------------------------------------
    app.setStyleSheet(R"(
        QToolBar {
            background-color: #3c3c3c;
            border: none;
            spacing: 6px;
            padding: 4px;
        }
        QDockWidget::title {
            background-color: #3c3c3c;
            padding: 6px;
        }
        QPushButton {
            background-color: #4a4a4a;
            border: 1px solid #5a5a5a;
            border-radius: 4px;
            padding: 6px 16px;
            min-height: 24px;
        }
        QPushButton:hover {
            background-color: #5a5a5a;
        }
        QPushButton:pressed {
            background-color: #3a3a3a;
        }
        QPushButton:disabled {
            background-color: #3a3a3a;
            color: #666666;
        }
        QLineEdit, QSpinBox, QDoubleSpinBox, QComboBox {
            background-color: #3c3c3c;
            border: 1px solid #5a5a5a;
            border-radius: 3px;
            padding: 4px;
        }
        QComboBox::drop-down {
            border: none;
        }
        QTreeWidget, QListWidget, QPlainTextEdit {
            background-color: #1e1e1e;
            border: 1px solid #3c3c3c;
        }
        QTabWidget::pane {
            border: 1px solid #3c3c3c;
            background-color: #2b2b2b;
        }
        QTabBar::tab {
            background-color: #3c3c3c;
            color: #b0b0b0;
            padding: 8px 16px;
            border: 1px solid #4a4a4a;
            border-bottom: none;
        }
        QTabBar::tab:selected {
            background-color: #2b2b2b;
            color: #e0e0e0;
            border-bottom: 2px solid #2196F3;
        }
        QProgressBar {
            background-color: #3c3c3c;
            border: 1px solid #5a5a5a;
            border-radius: 3px;
            text-align: center;
        }
        QProgressBar::chunk {
            background-color: #2196F3;
        }
        QGroupBox {
            border: 1px solid #4a4a4a;
            border-radius: 4px;
            margin-top: 12px;
            padding-top: 16px;
            background-color: #2f2f2f;
        }
        QGroupBox::title {
            subcontrol-origin: margin;
            left: 10px;
            padding: 0 6px;
            color: #c0c0c0;
        }
        QScrollArea {
            border: none;
            background-color: transparent;
        }
        QSplitter::handle {
            background-color: #4a4a4a;
        }
        QCheckBox::indicator {
            width: 16px;
            height: 16px;
            border: 1px solid #5a5a5a;
            border-radius: 3px;
            background-color: #3c3c3c;
        }
        QCheckBox::indicator:checked {
            background-color: #2196F3;
            border-color: #2196F3;
        }
        QHeaderView::section {
            background-color: #3c3c3c;
            border: 1px solid #4a4a4a;
            padding: 4px;
        }
    )");

    // -----------------------------------------------------------------------
    // Gateway client
    // -----------------------------------------------------------------------
    // Initially disconnected; user must connect via UI.
    hmi::GatewayClient client;

    // -----------------------------------------------------------------------
    // Engineer mode window (MainWindow)
    // -----------------------------------------------------------------------
    MainWindow engineerWindow;
    engineerWindow.setGatewayClient(&client);
    engineerWindow.setWindowTitle(QStringLiteral("检测系统 HMI - 工程师模式"));
    engineerWindow.resize(1600, 900);

    // -----------------------------------------------------------------------
    // Operator mode window (OperatorWindow)
    // -----------------------------------------------------------------------
    OperatorWindow operatorWindow;
    operatorWindow.setWindowTitle(QStringLiteral("检测系统 HMI - 操作员模式"));
    operatorWindow.resize(800, 1024);

    // -----------------------------------------------------------------------
    // Connect gateway signals to operator window
    // -----------------------------------------------------------------------
    // System state updates are streamed continuously when subscribed.
    QObject::connect(&client, &hmi::GatewayClient::systemStateReceived,
                     &operatorWindow, &OperatorWindow::updateTaskStatus);

    // Inspection events (captures, defects, etc.) are pushed to the result panel.
    QObject::connect(&client, &hmi::GatewayClient::inspectionEventReceived,
                     &operatorWindow, &OperatorWindow::addEvent);

    // Navigation map updates (used when switching tasks or maps).
    QObject::connect(&client, &hmi::GatewayClient::navMapReceived,
                     [&operatorWindow](hmi::Result result, hmi::NavMapInfo mapInfo) {
                         if (result.ok() && !mapInfo.image.media.mediaId.isEmpty()) {
                             // Convert thumbnail JPEG to QImage
                             QImage img;
                             if (!mapInfo.image.thumbnailJpeg.isEmpty()) {
                                 img.loadFromData(mapInfo.image.thumbnailJpeg, "JPEG");
                             }
                             operatorWindow.navMap()->setNavMap(mapInfo, img);
                         }
                     });

    // -----------------------------------------------------------------------
    // Mode switching
    // -----------------------------------------------------------------------
    // Switch from Engineer → Operator mode.
    QObject::connect(&engineerWindow, &MainWindow::switchToOperatorMode, [&]() {
        engineerWindow.hide();
        operatorWindow.show();
    });

    // Switch from Operator → Engineer mode.
    QObject::connect(&operatorWindow, &OperatorWindow::switchToEngineerMode, [&]() {
        operatorWindow.hide();
        engineerWindow.show();
    });

    // -----------------------------------------------------------------------
    // Operator control panel to gateway
    // -----------------------------------------------------------------------
    QObject::connect(operatorWindow.controlPanel(), &ControlPanel::startRequested,
                     &client, &hmi::GatewayClient::startInspection);
    QObject::connect(operatorWindow.controlPanel(), &ControlPanel::pauseRequested,
                     &client, &hmi::GatewayClient::pauseInspection);
    QObject::connect(operatorWindow.controlPanel(), &ControlPanel::resumeRequested,
                     &client, &hmi::GatewayClient::resumeInspection);
    QObject::connect(operatorWindow.controlPanel(), &ControlPanel::stopRequested,
                     &client, &hmi::GatewayClient::stopInspection);

    // -----------------------------------------------------------------------
    // Result panel download requests
    // -----------------------------------------------------------------------
    QObject::connect(operatorWindow.resultPanel(), &ResultPanel::downloadImageRequested,
                     &client, &hmi::GatewayClient::downloadMedia);
    QObject::connect(&client, &hmi::GatewayClient::mediaDownloaded,
                     operatorWindow.resultPanel(), &ResultPanel::setFullImage);

    // -----------------------------------------------------------------------
    // Show engineer window by default
    // -----------------------------------------------------------------------
    engineerWindow.show();

    // -----------------------------------------------------------------------
    // Enter Qt event loop
    // -----------------------------------------------------------------------
    return app.exec();
}
