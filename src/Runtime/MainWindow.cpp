﻿#include "MainWindow.h"
#include "ui_MainWindow.h"
#include <QMdiArea>
#include <QLoggingCategory>
#include <QPlainTextEdit>
#include <QPointer>
#include <QMenu>
#include <QToolButton>
#include <Widgets/AboutDialog.h>
#include <QLabel>
#include <QDateTime>
static QPointer<QPlainTextEdit> s_messageLogWidget;
static QPointer<QFile> s_logFile;
static void messageHandler(QtMsgType msgType, const QMessageLogContext& logContext, const QString& msg)
{

	QString txt;
	switch (msgType) {
	case QtDebugMsg:
		txt = QString("[Debug]: %1").arg(msg);
		break;
	case QtWarningMsg:
		txt = QString("[Warning]: %1").arg(msg);
		break;
	case QtCriticalMsg:
		txt = QString("[Critical]: %1").arg(msg);
		break;
	case QtFatalMsg:
		txt = QString("[Fatal]: %1").arg(msg);
		abort();
	}

	if (!s_messageLogWidget.isNull())
		s_messageLogWidget->appendPlainText(txt);
	QTextStream ts(s_logFile);
	ts << txt << endl;
}
MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow),
    m_vulkanWindow(new GU::VulkanWindow())
{
    ui->setupUi(this);

	// Debug log
	s_messageLogWidget = ui->debugLog;
	s_messageLogWidget->setReadOnly(true);
	QDateTime currDateTime = QDateTime::currentDateTime();
	s_logFile = new QFile(QString("./logs/%1-log.txt").arg(currDateTime.toString("yyyy-MM-dd-hh-mm-ss-zzz")));
	s_logFile->open(QIODevice::WriteOnly | QIODevice::Append);
	s_logFile->resize(0);
	qInstallMessageHandler(messageHandler);

	// vulkanWindow
    QWidget* vulkanWindowWrapper = QWidget::createWindowContainer(m_vulkanWindow);
	QLoggingCategory::setFilterRules(QStringLiteral("qt.vulkan=true"));
	inst = new QVulkanInstance();
	inst->setLayers(QByteArrayList()
		<< "VK_LAYER_GOOGLE_threading"
		<< "VK_LAYER_LUNARG_parameter_validation"
		<< "VK_LAYER_LUNARG_object_tracker"
		<< "VK_LAYER_LUNARG_core_validation"
		<< "VK_LAYER_LUNARG_image"
		<< "VK_LAYER_LUNARG_swapchain"
		<< "VK_LAYER_GOOGLE_unique_objects");

	if (!inst->create())
		qFatal("Failed to create Vulkan instance: %d", inst->errorCode());
	m_vulkanWindow->setVulkanInstance(inst);
	setCentralWidget(vulkanWindowWrapper);

	// pop Menu
	creatorPopMenu();

	// statusbar
	m_mousePosition = new QLabel(this);
	m_mousePosition->setText(QString::fromLocal8Bit("正在加载程序..."));
	ui->statusbar->addWidget(m_mousePosition);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::creatorPopMenu()
{
	QMenu* menu = new QMenu(this);
	menu->addAction(ui->actShowViewDock);
	menu->addAction(ui->actShowInfoDock);
	QToolButton* toolBtn = new QToolButton(this);
	toolBtn->setPopupMode(QToolButton::InstantPopup);
	toolBtn->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
	toolBtn->setDefaultAction(ui->actSelPopMenu);
	toolBtn->setMenu(menu);
	ui->toolBar->addSeparator();
	ui->toolBar->addWidget(toolBtn);

	ui->toolBar->addSeparator();
	ui->toolBar->addAction(ui->actPlay);
	ui->toolBar->addAction(ui->actPause);
	ui->actPause->setVisible(false);
	ui->toolBar->addAction(ui->actStop);
	ui->toolBar->addSeparator();
	ui->toolBar->addAction(ui->actAbout);
}

void MainWindow::on_actShowViewDock_triggered()
{
	if (ui->actShowViewDock->isChecked())
		ui->dockView->show();
	else
		ui->dockView->hide();
}

void MainWindow::on_actShowInfoDock_triggered()
{
	if (ui->actShowInfoDock->isChecked())
		ui->dockInfo->show();
	else
		ui->dockInfo->hide();
}

void MainWindow::on_actAbout_triggered()
{
	AboutDialog* aboutDialog = new AboutDialog(this);
	aboutDialog->exec();
}

void MainWindow::on_actPlay_triggered()
{
	if (ui->actPlay->isVisible()) ui->actPlay->setVisible(false);
	if (!ui->actPause->isVisible()) ui->actPause->setVisible(true);
}

void MainWindow::on_actPause_triggered()
{
	if (ui->actPause->isVisible()) ui->actPause->setVisible(false);
	if (!ui->actPlay->isVisible()) ui->actPlay->setVisible(true);
}


