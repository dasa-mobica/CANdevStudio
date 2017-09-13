
#include "mainwindow.h"
#include "log.hpp"
#include "modelvisitor.h" // apply_model_visitor
#include "ui_mainwindow.h"

#include <QtCore/QFile>
#include <QtCore/QMimeData>
#include <QtGui/QDrag>
#include <QtGui/QWindow>
#include <QtWidgets/QFileDialog>
#include <QtWidgets/QMdiArea>
#include <QtWidgets/QMdiSubWindow>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QToolButton>
#include <QtWidgets/QVBoxLayout>
#include <QRadioButton>

#include <cassert> // assert
#include <iostream>

#include <candevice/candevicemodel.h>
#include <canrawsender/canrawsendermodel.h>
#include <canrawview/canrawviewmodel.h>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , ui(std::make_unique<Ui::MainWindow>())
{
    ui->setupUi(this);
    ui->centralWidget->layout()->setContentsMargins(0, 0, 0, 0);

    auto modelRegistry = std::make_shared<QtNodes::DataModelRegistry>();
    modelRegistry->registerModel<CanDeviceModel>();
    modelRegistry->registerModel<CanRawSenderModel>();
    modelRegistry->registerModel<CanRawViewModel>();

    canRawSender = std::make_shared<CanRawSender>();
    graphScene = std::make_shared<QtNodes::FlowScene>(modelRegistry);

    connect(graphScene.get(), &QtNodes::FlowScene::nodeCreated, this, &MainWindow::nodeCreatedCallback);
    connect(graphScene.get(), &QtNodes::FlowScene::nodeDeleted, this, &MainWindow::nodeDeletedCallback);
    connect(graphScene.get(), &QtNodes::FlowScene::nodeDoubleClicked, this, &MainWindow::nodeDoubleClickedCallback);

    setupMdiArea();
    connectToolbarSignals();
    connectMenuSignals();
}

MainWindow::~MainWindow() { delete graphView; }

void MainWindow::closeEvent(QCloseEvent* e)
{
    QMessageBox::StandardButton userReply;
    userReply = QMessageBox::question(
        this, "Exit", "Are you sure you want to quit CANdevStudio?", QMessageBox::Yes | QMessageBox::No);
    if (userReply == QMessageBox::Yes) {
        QApplication::quit();
    } else {
        e->ignore();
    }
}

void MainWindow::nodeCreatedCallback(QtNodes::Node& node)
{
    auto dataModel = node.nodeDataModel();

    assert(nullptr != dataModel);

    apply_model_visitor(*dataModel,
        [this, dataModel](CanRawViewModel& m) {
            auto rawView = &m.canRawView;
            ui->mdiArea->addSubWindow(rawView);
            connect(ui->actionstart, &QAction::triggered, rawView, &CanRawView::startSimulation);
            connect(ui->actionstop, &QAction::triggered, rawView, &CanRawView::stopSimulation);
            connect(rawView, &CanRawView::dockUndock, this, [this, rawView] { handleDock(rawView, ui->mdiArea); });
        },
        [this, dataModel](CanRawSenderModel& m) {
            QWidget* crsWidget = m.canRawSender.getMainWidget();
            auto& rawSender = m.canRawSender;
            ui->mdiArea->addSubWindow(crsWidget);
            connect(
                &rawSender, &CanRawSender::dockUndock, this, [this, crsWidget] { handleDock(crsWidget, ui->mdiArea); });
            connect(ui->actionstart, &QAction::triggered, &rawSender, &CanRawSender::startSimulation);
            connect(ui->actionstop, &QAction::triggered, &rawSender, &CanRawSender::stopSimulation);
        },
        [this](CanDeviceModel&) {});
}

void handleWidgetDeletion(QWidget* widget)
{
    assert(nullptr != widget);
    if (widget->parentWidget()) {

        widget->parentWidget()->close();
    } // else path not needed
}

void MainWindow::nodeDeletedCallback(QtNodes::Node& node)
{
    auto dataModel = node.nodeDataModel();

    assert(nullptr != dataModel);

    apply_model_visitor(*dataModel, [this, dataModel](CanRawViewModel& m) { handleWidgetDeletion(&m.canRawView); },
        [this, dataModel](CanRawSenderModel& m) { handleWidgetDeletion(m.canRawSender.getMainWidget()); },
        [this](CanDeviceModel&) {});
}

void handleWidgetShowing(QWidget* widget)
{
    assert(nullptr != widget);
    if (widget->parentWidget()) {

        widget->parentWidget()->show();
    } else {
        widget->show();
    }
}

void MainWindow::nodeDoubleClickedCallback(QtNodes::Node& node)
{
    auto dataModel = node.nodeDataModel();

    assert(nullptr != dataModel);

    apply_model_visitor(*dataModel, [this, dataModel](CanRawViewModel& m) { handleWidgetShowing(&m.canRawView); },
        [this, dataModel](CanRawSenderModel& m) { handleWidgetShowing(m.canRawSender.getMainWidget()); },
        [this](CanDeviceModel&) {});
}

void MainWindow::handleDock(QWidget* component, QMdiArea* mdi)
{
    // check if component is already displayed by mdi area
    if (mdi->subWindowList().contains(static_cast<QMdiSubWindow*>(component->parentWidget()))) {
        // undock
        auto parent = component->parentWidget();
        mdi->removeSubWindow(component); // removeSubwWndow only removes widget, not window

        component->show();
        parent->close();
    } else {
        // dock
        mdi->addSubWindow(component)->show();
    }
}

void MainWindow::connectToolbarSignals()
{
    connect(ui->actionstart, &QAction::triggered, ui->actionstop, &QAction::setDisabled);
    connect(ui->actionstart, &QAction::triggered, ui->actionstart, &QAction::setEnabled);
    connect(ui->actionstop, &QAction::triggered, ui->actionstop, &QAction::setEnabled);
    connect(ui->actionstop, &QAction::triggered, ui->actionstart, &QAction::setDisabled);
}

void MainWindow::handleSaveAction()
{
    QString fileName = QFileDialog::getSaveFileName(
        nullptr, "Project Configuration", QDir::homePath(), "CANdevStudio Files (*.cds)");

    if (!fileName.isEmpty()) {
        if (!fileName.endsWith(".cds", Qt::CaseInsensitive))
            fileName += ".cds";

        QFile file(fileName);
        if (file.open(QIODevice::WriteOnly)) {
            file.write(graphScene->saveToMemory()); // FIXME
        }
    } else {
        cds_error("File name empty");
    }
}

void MainWindow::handleLoadAction()
{
    graphScene->clearScene();

    QString fileName
        = QFileDialog::getOpenFileName(nullptr, "Project Configuration", QDir::homePath(), "CANdevStudio (*.cds)");

    if (!QFileInfo::exists(fileName)) {
        cds_error("File does not exist");
        return;
    }

    QFile file(fileName);

    if (!file.open(QIODevice::ReadOnly)) {
        cds_error("Could not open file");
        return;
    }

    QByteArray wholeFile = file.readAll();

    // TODO check if file is correct, nodeeditor library does not provide it and will crash if incorrect file is
    // supplied

    graphScene->loadFromMemory(wholeFile); // FIXME
}

void MainWindow::connectMenuSignals()
{
    QActionGroup* ViewModes = new QActionGroup(this);
    ViewModes->addAction(ui->actionTabView);
    ViewModes->addAction(ui->actionSubWindowView);
    connect(ui->actionAbout, &QAction::triggered, this, [this] { QMessageBox::about(this, "About", "<about body>"); });
    connect(ui->actionExit, &QAction::triggered, this, &MainWindow::close);
    connect(ui->actionLoad, &QAction::triggered, this, &MainWindow::handleLoadAction);
    connect(ui->actionSave, &QAction::triggered, this, &MainWindow::handleSaveAction);
    connect(ui->actionTile, &QAction::triggered, ui->mdiArea, &QMdiArea::tileSubWindows);
    connect(ui->actionCascade, &QAction::triggered, ui->mdiArea, &QMdiArea::cascadeSubWindows);
    connect(ui->actionTabView, &QAction::triggered, this, [this] { ui->mdiArea->setViewMode(QMdiArea::TabbedView); });
    connect(ui->actionTabView, &QAction::toggled, ui->actionTile, &QAction::setDisabled);
    connect(ui->actionTabView, &QAction::toggled, ui->actionCascade, &QAction::setDisabled);
    connect(ui->actionSubWindowView, &QAction::triggered, this,
        [this] { ui->mdiArea->setViewMode(QMdiArea::SubWindowView); });
}

struct ModelToolButton : public QToolButton {
    ModelToolButton(QWidget* parent = nullptr)
        : QToolButton(parent)
    {
        setAcceptDrops(true);
    }

    static QString mimeDataKey() { return QStringLiteral("application/x-qtnodeeditor"); }


    void mousePressEvent(QMouseEvent* event) override
    {
        cds_debug("mousePressEvent");

//        QPoint hotSpot = event->pos();

//        QMimeData* mimeData = new QMimeData;
//        mimeData->setData(ModelToolButton::mimeDataKey(), text().toUtf8());

//        qreal dpr = window()->windowHandle()->devicePixelRatio();
//        QPixmap pixmap(this->size() * dpr);
//        pixmap.setDevicePixelRatio(dpr);
//        this->render(&pixmap);

//        QDrag* drag = new QDrag(this);
//        drag->setMimeData(mimeData);
//        drag->setPixmap(pixmap);
//        drag->setHotSpot(hotSpot);
        //Qt::DropAction action = drag->exec(Qt::CopyAction);

//        if (drag->exec(Qt::CopyAction | Qt::MoveAction, Qt::CopyAction) == Qt::MoveAction) {
//            //child->close();
//        } else {
//            //child->show();
//            //child->setPixmap(pixmap);
//        }

//        if (Qt::IgnoreAction == action) {
//            cds_debug("Drag and drop ignored. Act as if clicked");
//            std::cout << "dupa 1" << std::endl;
//        } else {
//            event->accept();
//        }

        QToolButton::mousePressEvent(event);
    }

};
struct ModelButtonGroup : public QButtonGroup {
    ModelButtonGroup(QWidget* parent = nullptr)
        : QButtonGroup(parent){}

};

struct FlowViewWrapper : public QtNodes::FlowView {
    FlowViewWrapper(QtNodes::FlowScene* scene)
        : QtNodes::FlowView(scene)
        , _scene(scene)
    {
        setAcceptDrops(true);
    }

    void dropEvent(QDropEvent* event) override
    {
        cds_debug("DropEvent");
std::cout << "dupa 12" << std::endl;
        const QMimeData* mime = event->mimeData();
        QByteArray data = mime->data(ModelToolButton::mimeDataKey());

        if (data.size() > 0) {
            QString modelName = data;
            cds_debug("Drop data: {}", modelName.toStdString());
            addNode(modelName, event->pos());
            event->acceptProposedAction();
        } else {
            cds_warn("Accepted drop does not contain {} data", ModelToolButton::mimeDataKey().toStdString());
        }
    }

    void dragMoveEvent(QDragMoveEvent* event) override
    {
        QtNodes::FlowView::dragMoveEvent(event);
std::cout << "dupa 13" << std::endl;
        const QMimeData* mime = event->mimeData();
        QByteArray data = mime->data(ModelToolButton::mimeDataKey());

        if (data.size() > 0) {
            event->acceptProposedAction();
        }
    }

    /*
    void mousePressEvent(QMouseEvent* event) override
    {
        std::cout << "hahahah" << std::endl;
    }
    */

    void addNode(const QString& modelName, const QPoint& pos)
    {
        cds_debug("Adding node: {}", modelName.toStdString());
        auto type = _scene->registry().create(modelName);
std::cout << "dupa 14" << std::endl;
        if (type) {
            auto& node = _scene->createNode(std::move(type));
            node.nodeGraphicsObject().setPos(mapToScene(pos));
        } else {
            cds_warn("Failed to create {} node. Does it exist in the registry?", modelName.toStdString());
        }
    }

private:
    QtNodes::FlowScene* _scene;
};

void MainWindow::setupMdiArea()
{
    graphView = new QWidget();
    graphView->setAcceptDrops(true);
    auto flowView = new FlowViewWrapper(graphScene.get());
    auto layout = new QHBoxLayout();

//    auto button1 = new QToolButton();
//    auto button2 = new QToolButton();
//    auto button3 = new QToolButton();
    auto buttonN = new ModelToolButton();
    auto button1 = new ModelToolButton();
    auto button2 = new ModelToolButton();
    auto button3 = new ModelToolButton();

    buttonN->setToolTip("Arrow");
    buttonN->setAcceptDrops(false);
    buttonN->setIcon(QIcon("../resources/icons/external-link.svg"));
    buttonN->setCheckable(true);
    buttonN->setChecked(true);

    button1->setText("CanDeviceModel");
    button1->setToolTip("CanDeviceModel");
    button1->setIcon(QIcon("../resources/icons/settings.svg"));
    button1->setCheckable(true);


    button2->setText("CanRawSenderModel");
    button2->setToolTip("CanRawSenderModel");
    button2->setIcon(QIcon("../resources/icons/email.svg"));
    button2->setCheckable(true);

    button3->setText("CanRawViewModel");
    button3->setToolTip("CanRawViewModel ");
    button3->setIcon(QIcon("../resources/icons/share.svg"));
    button3->setCheckable(true);

    auto group = new ModelButtonGroup();
    group->addButton(buttonN);
    group->addButton(button1);
    group->addButton(button2);
    group->addButton(button3);
    group->setExclusive(true);

    auto toolbar = new QToolBar();
    toolbar->setOrientation(Qt::Vertical);
    toolbar->addWidget(buttonN);
    toolbar->addWidget(button1);
    toolbar->addWidget(button2);
    toolbar->addWidget(button3);

    layout->addWidget(toolbar);     // ikonki po lewej
    layout->addWidget(flowView);    // siatka
    graphView->setLayout(layout);
    graphView->setWindowTitle("Project Configuration");
    ui->mdiArea->addSubWindow(graphView);
    ui->mdiArea->setAttribute(Qt::WA_DeleteOnClose, false);
    ui->mdiArea->tileSubWindows();
}
