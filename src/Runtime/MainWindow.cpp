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
#include <Global/CoreContext.h>
#include <QStandardItem>
#include <QStandardItemModel>
#include <QItemSelectionModel>
#include <QPoint>
#include <Scene/Entity.h>
#include <Scene/Component.h>
#include <Widgets/CPropertyHeader.h>
#include <Widgets/CDoubleProperty.h>
#include <Widgets/CStringProperty.h>
#include <Widgets/CListProperty.h>
#include <Widgets/NewProjectDialog.h>
#include <QFileDialog>
#include <Core/Project.h>
#include <QProgressBar>
#include <Widgets/NavMeshParamsDlg.h>
#include <Widgets/AddMeshToEntityDlg.h>
#include <Core/ThreadPool.h>
#include <Scene/Asset.h>
#include <Renderer/VulkanDescriptor.h>
#include <Renderer/Texture.h>
#include <Function/AgentNav/RCScheduler.h>
#include <Function/Animation/Animation.h>
#include <QMessageBox>
#include <Widgets/AgentParam.h>
static QPointer<QPlainTextEdit> s_messageLogWidget;
static QPointer<QFile> s_logFile;

/*******************************Property***********************************************/ 
// Tag Component
CStringProperty* tagProperty;
CStringProperty* uuidProperty;
// Transform Component
CPropertyHeader* tsfheader;
CPropertyHeader* posheader;
CPropertyHeader* rotheader;
CPropertyHeader* sclheader;
CDoubleProperty* pxProperty;
CDoubleProperty* pyProperty;
CDoubleProperty* pzProperty;
CDoubleProperty* rxProperty;
CDoubleProperty* ryProperty;
CDoubleProperty* rzProperty;
CDoubleProperty* sxProperty;
CDoubleProperty* syProperty;
CDoubleProperty* szProperty;

CPropertyHeader* materialheader;
CStringProperty* meshProperty;
CStringProperty* meshuuidProperty;
CStringProperty* textureProperty;
CStringProperty* textureuuidProperty;

CPropertyHeader* skeletalMaterialheader;
CStringProperty* skeletalMeshProperty;
CStringProperty* skeletalMeshuuidProperty;
CStringProperty* skeletalMeshtextureProperty;
CStringProperty* skeletalMeshtextureuuidProperty;
CListProperty* skeletalCurrentAnimationProperty;
/*******************************Property***********************************************/

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

	qRegisterMetaType<uint64_t>("uint64_t");
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

	// init ui
	createPopMenu();
	createEntityView();
	craeteComponentView();
	craeteResourceView();
	navmeshdlg = new NavMeshParamsDlg(this, m_meshTableModel, m_meshTableSelectModel);

	setProjectActionDisable();

	// statusbar
	m_statusInfo = new QLabel(this);
	m_progressBar = new QProgressBar(this);
	m_progressBar->setMaximumWidth(500);
	connect(this, SIGNAL(signal_progressTick(int)), this, SLOT(slot_progressTick(int)));
	m_statusInfo->setText(QString::fromLocal8Bit("暂停"));
	//m_progressBar->hide();
	ui->statusbar->addWidget(m_statusInfo);
	ui->statusbar->addWidget(m_progressBar);
	//m_progressBar->setValue(50);
	agentParam = new AgentParam(this);
	ui->actAddAgent->setEnabled(false);
	ui->actAgentTarget->setEnabled(false);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::createPopMenu()
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

void MainWindow::createEntityView()
{
	m_entityTreeModel = new QStandardItemModel(this);
	m_entityTreeSelectModel = new QItemSelectionModel(m_entityTreeModel, this);

	ui->entityTreeView->setModel(m_entityTreeModel);
	ui->entityTreeView->setSelectionModel(m_entityTreeSelectModel);
	ui->entityTreeView->setHeaderHidden(true);
	ui->entityTreeView->setAlternatingRowColors(true);
	ui->entityTreeView->setContextMenuPolicy(Qt::CustomContextMenu);
	connect(ui->entityTreeView, SIGNAL(customContextMenuRequested(const QPoint&)), this, SLOT(slot_treeviewEntity_customcontextmenu(const QPoint&)));
	m_treeviewEntityRoot = new QStandardItem(QString::fromLocal8Bit("场景根节点"));
	QIcon icon;
	icon.addFile(":/images/root.png");
	m_treeviewEntityRoot->setIcon(icon);
	m_entityTreeModel->appendRow(m_treeviewEntityRoot);

	

	connect(m_entityTreeSelectModel, SIGNAL(currentChanged(const QModelIndex&, const QModelIndex&)), this, SLOT(slot_on_entityTreeSelectModel_currentChanged(const QModelIndex&, const QModelIndex&)));
	connect(ui->componentTreeWidget,
		SIGNAL(tagChanged()),
		this,
		SLOT(slot_tagPropertyChanged())
	);
}

void MainWindow::craeteComponentView()
{
	ui->componentTreeWidget->init();
	// Tag Component
	tagProperty = new CStringProperty("tagProperty", QString::fromLocal8Bit("标签"), QString::fromLocal8Bit("entity"));
	uuidProperty = new CStringProperty(QString::fromLocal8Bit("uuidProperty").toUtf8(), QString::fromLocal8Bit("UUID"), QString::fromLocal8Bit("%1").arg(113123123124));
	uuidProperty->setDisabled(true);
	// Transform Component
	tsfheader = new CPropertyHeader("tsfheader", QString::fromLocal8Bit("转换组件"));
	posheader = new CPropertyHeader(tsfheader, "tsfHeader", QString::fromLocal8Bit("位置"));
	rotheader = new CPropertyHeader(tsfheader, "rotheader", QString::fromLocal8Bit("旋转"));
	sclheader = new CPropertyHeader(tsfheader, "sclheader", QString::fromLocal8Bit("缩放"));
	pxProperty = new CDoubleProperty(posheader, "pxProperty", "x:", 0, 0, -1000.0, 1000.0);
	pyProperty = new CDoubleProperty(posheader, "pyProperty", "y:", 0, 0, -1000.0, 1000.0);
	pzProperty = new CDoubleProperty(posheader, "pzProperty", "z:", 0, 0, -1000.0, 1000.0);

	rxProperty = new CDoubleProperty(rotheader, "rxProperty", "x:", 0, 0, -1000.0, 1000.0);
	ryProperty = new CDoubleProperty(rotheader, "ryProperty", "y:", 0, 0, -1000.0, 1000.0);
	rzProperty = new CDoubleProperty(rotheader, "rzProperty", "z:", 0, 0, -1000.0, 1000.0);

	sxProperty = new CDoubleProperty(sclheader, "sxProperty", "x:", 0, 0, -1000.0, 1000.0);
	syProperty = new CDoubleProperty(sclheader, "syProperty", "y:", 0, 0, -1000.0, 1000.0);
	szProperty = new CDoubleProperty(sclheader, "szProperty", "z:", 0, 0, -1000.0, 1000.0);

	materialheader = new CPropertyHeader("material", QString::fromLocal8Bit("材质组件"));
	meshProperty = new CStringProperty(materialheader, "meshProperty", QString::fromLocal8Bit("网格名称"), QString::fromLocal8Bit(""));
	meshuuidProperty = new CStringProperty(materialheader, "meshuuidProperty", QString::fromLocal8Bit("网格id"), QString::fromLocal8Bit(""));
	textureProperty = new CStringProperty(materialheader, "textureProperty", QString::fromLocal8Bit("贴图名称"), QString::fromLocal8Bit(""));
	textureuuidProperty = new CStringProperty(materialheader, "textureuuidProperty", QString::fromLocal8Bit("贴图id"), QString::fromLocal8Bit(""));
	meshProperty->setDisabled(true);
	meshuuidProperty->setDisabled(true);
	textureProperty->setDisabled(true);
	textureuuidProperty->setDisabled(true);


	skeletalMaterialheader = new CPropertyHeader("skeletalMaterial", QString::fromLocal8Bit("骨骼动画"));
	skeletalMeshProperty = new CStringProperty(skeletalMaterialheader, "skeletalMeshProperty", QString::fromLocal8Bit("骨骼模型名称"), QString::fromLocal8Bit(""));
	skeletalMeshuuidProperty = new CStringProperty(skeletalMaterialheader, "skeletalMeshuuidProperty", QString::fromLocal8Bit("骨骼模型id"), QString::fromLocal8Bit(""));
	skeletalMeshtextureProperty = new CStringProperty(skeletalMaterialheader, "skeletalMeshtextureProperty", QString::fromLocal8Bit("贴图名称"), QString::fromLocal8Bit(""));
	skeletalMeshtextureuuidProperty = new CStringProperty(skeletalMaterialheader, "skeletalMeshtextureuuidProperty", QString::fromLocal8Bit("贴图id"), QString::fromLocal8Bit(""));
	skeletalCurrentAnimationProperty = new CListProperty(skeletalMaterialheader, "skeletalCurrentAnimationProperty", QString::fromLocal8Bit("当前动画"), { CListDataItem("empty")}, 0);
	skeletalMeshProperty->setDisabled(true);
	skeletalMeshuuidProperty->setDisabled(true);
	skeletalMeshtextureProperty->setDisabled(true);
	skeletalMeshtextureuuidProperty->setDisabled(true);
	
	//ui->componentTreeWidget->adjustToContents();
}
void MainWindow::craeteResourceView()
{
	// mesh table init
	m_meshTableModel = new QStandardItemModel(50, 15, this);
	m_meshTableSelectModel = new QItemSelectionModel(m_meshTableModel, this);
	ui->modelTableView->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
	ui->modelTableView->setModel(m_meshTableModel);
	ui->modelTableView->setSelectionModel(m_meshTableSelectModel);
	ui->modelTableView->horizontalHeader()->hide();
	ui->modelTableView->verticalHeader()->hide();

	// skeletal mesh table init
	m_skeletalmeshTableModel = new QStandardItemModel(50, 15, this);
	m_skeletalmeshTableSelectModel = new QItemSelectionModel(m_skeletalmeshTableModel, this);
	ui->skeletalMeshTableView->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
	ui->skeletalMeshTableView->setModel(m_skeletalmeshTableModel);
	ui->skeletalMeshTableView->setSelectionModel(m_skeletalmeshTableSelectModel);
	ui->skeletalMeshTableView->horizontalHeader()->hide();
	ui->skeletalMeshTableView->verticalHeader()->hide();

	// texture table init
	m_textureTableModel = new QStandardItemModel(50, 15, this);
	m_textureTableSelectModel = new QItemSelectionModel(m_textureTableModel, this);
	ui->textureTableView->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
	ui->textureTableView->setModel(m_textureTableModel);
	ui->textureTableView->setSelectionModel(m_textureTableSelectModel);
	ui->textureTableView->horizontalHeader()->hide();
	ui->textureTableView->verticalHeader()->hide();

	// connect
	connect(this, SIGNAL(signal_importResource2Table(QString, uint64_t, int)), this, SLOT(slot_importResource2Table(QString, uint64_t, int)));
	connect(m_meshTableSelectModel, SIGNAL(currentChanged(const QModelIndex&, const QModelIndex&)), this, SLOT(slot_on_meshTableSelectModel_currentChanged(const QModelIndex&, const QModelIndex&)));
	connect(m_textureTableSelectModel, SIGNAL(currentChanged(const QModelIndex&, const QModelIndex&)), this, SLOT(slot_on_textureTableSelectModel_currentChanged(const QModelIndex&, const QModelIndex&)));
}

void MainWindow::clearAllComponentProperty()
{
	ui->componentTreeWidget->remove(tagProperty);
	ui->componentTreeWidget->remove(uuidProperty);
	ui->componentTreeWidget->remove(tsfheader );
	ui->componentTreeWidget->remove(posheader );
	ui->componentTreeWidget->remove(rotheader );
	ui->componentTreeWidget->remove(sclheader );
	ui->componentTreeWidget->remove(pxProperty);
	ui->componentTreeWidget->remove(pyProperty);
	ui->componentTreeWidget->remove(pzProperty);

	ui->componentTreeWidget->remove(rxProperty);
	ui->componentTreeWidget->remove(ryProperty);
	ui->componentTreeWidget->remove(rzProperty);

	ui->componentTreeWidget->remove(sxProperty);
	ui->componentTreeWidget->remove(syProperty);
	ui->componentTreeWidget->remove(szProperty);

	ui->componentTreeWidget->remove(materialheader);
	ui->componentTreeWidget->remove(meshProperty);
	ui->componentTreeWidget->remove(meshuuidProperty);
	ui->componentTreeWidget->remove(textureProperty);
	ui->componentTreeWidget->remove(textureuuidProperty);

	ui->componentTreeWidget->remove(skeletalMaterialheader);
	ui->componentTreeWidget->remove(skeletalMeshProperty);
	ui->componentTreeWidget->remove(skeletalMeshuuidProperty);
	ui->componentTreeWidget->remove(skeletalMeshtextureProperty);
	ui->componentTreeWidget->remove(skeletalMeshtextureuuidProperty);
	ui->componentTreeWidget->remove(skeletalCurrentAnimationProperty);
}

#define AllProjectEnableui(isenable) ui->actImportModel->setEnabled(isenable);\
ui->actImportSkeletalMesh->setEnabled(isenable);\
ui->actSaveProject->setEnabled(isenable);\
ui->actNavmeshParam->setEnabled(isenable);\
ui->actImportTexture->setEnabled(isenable);\
ui->actNavmeshParam->setEnabled(isenable);\
ui->actAgentParam->setEnabled(isenable);\

void MainWindow::setProjectActionEnable()
{
	AllProjectEnableui(true)
}

void MainWindow::setProjectActionDisable()
{
	AllProjectEnableui(false)
}

void MainWindow::importResource2Table(QString filename, uint64_t uuid, int type)
{
	emit signal_importResource2Table(filename, uuid, type);
}

void MainWindow::setStatus(const QString& text)
{
	m_statusInfo->setText(text);
}

void MainWindow::progressBegin(int tasknum)
{
	/*m_progressBar->show();
	m_progressTaskNum = tasknum;
	m_progressBar->setMaximum(tasknum);
	m_progressBar->setValue(0);*/
	emit signal_progressTick(tasknum);
}

void MainWindow::progressTick()
{
	emit signal_progressTick(0);
}

void MainWindow::progressEnd()
{
	setStatus("");
	emit signal_progressTick(-1);
}

void MainWindow::addEntity(uint64_t uuid)
{
	auto entity = GLOBAL_SCENE->getEntityByUUID(uuid);
	auto name = entity.getComponent<GU::TagComponent>().Tag;
	QStandardItem* item = new QStandardItem(name.c_str());
	item->setData(uuid);
	m_entityMap[uuid] = item;
	QIcon icon;
	icon.addFile(":/images/entity.png");
	item->setIcon(icon);
	item->setEditable(false);
	m_treeviewEntityRoot->appendRow(item);
}

void MainWindow::removeEntity(uint64_t uuid)
{
	for (size_t i = 0; i < m_treeviewEntityRoot->rowCount(); i++)
	{
		auto tvitem = m_treeviewEntityRoot->child(i);
		if (tvitem->data().toULongLong() == uuid)
		{
			m_treeviewEntityRoot->removeRow(i);
		}
	}
}


void MainWindow::on_actShowViewDock_triggered()
{
	if (ui->actShowViewDock->isChecked())
		ui->dockEntity->show();
	else
		ui->dockEntity->hide();
}

void MainWindow::on_actShowInfoDock_triggered()
{
	if (ui->actShowInfoDock->isChecked())
		ui->dockInfo->show();
	else
		ui->dockInfo->hide();
}
void MainWindow::on_actNewProject_triggered()
{
	NewProjectDialog* newProjectDlg = new NewProjectDialog(this);

	auto rnt = newProjectDlg->exec();

	if (rnt == QDialog::Accepted)
	{
		std::string projectfile = newProjectDlg->m_projectPath.toStdString();
		GLOBAL_SAVE_PROJECT(projectfile);
		setProjectActionEnable();
	}
}
void MainWindow::on_actOpenProject_triggered()
{
	QString qfilename = QFileDialog::getOpenFileName(this, QString::fromLocal8Bit("打开工程"), QDir::currentPath(), QString::fromLocal8Bit("工程文件(*.gu)"));
	if (qfilename.isEmpty()) return;

	std::string filename = qfilename.toStdString();
	GLOBAL_OPEN_PROJECT(filename);
	setProjectActionEnable();
}
void MainWindow::on_actSaveProject_triggered()
{
	if (!GLOBAL_PROJECT_FILE_PATH.empty())
	{
		GLOBAL_SAVE_PROJECT(GLOBAL_PROJECT_FILE_PATH);
	}
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
	/*ui->dockEntity->hide();
	ui->dockInfo->hide();
	ui->dockComponent->hide();
	ui->actShowInfoDock->setChecked(false);
	ui->actShowViewDock->setChecked(false);*/
	GU::g_CoreContext.g_isStop = false;
	GU::g_CoreContext.g_isPlay = true;
}

void MainWindow::on_actPause_triggered()
{
	if (ui->actPause->isVisible()) ui->actPause->setVisible(false);
	if (!ui->actPlay->isVisible()) ui->actPlay->setVisible(true);
	GU::g_CoreContext.g_isPlay = false;
	GU::g_CoreContext.g_isStop = false;
}

void MainWindow::on_actStop_triggered()
{
	if (ui->actPause->isVisible()) ui->actPause->setVisible(false);
	if (!ui->actPlay->isVisible()) ui->actPlay->setVisible(true);
	ui->dockEntity->show();
	ui->dockInfo->show();
	ui->actShowInfoDock->setChecked(true);
	ui->actShowViewDock->setChecked(true);
	GU::g_CoreContext.g_isPlay = false;
	GU::g_CoreContext.g_isStop = true;
}

void MainWindow::on_actCreateEntity_triggered()
{
	auto entity =  GLOBAL_SCENE->createEntity();
}

void MainWindow::on_actCopyEntity_triggered()
{
	auto sitem = m_entityTreeModel->itemFromIndex(m_entityTreeSelectModel->currentIndex());
	uint64_t slduuid = sitem->data().toULongLong();

	auto entity = GLOBAL_SCENE->duplicateEntity(GLOBAL_SCENE->getEntityByUUID(slduuid));
	auto uuid = entity.getComponent<GU::IDComponent>().ID;
	auto name = entity.getComponent<GU::TagComponent>().Tag;
	QStandardItem* item = new QStandardItem(name.c_str());
	item->setData((UINT64)uuid);
	m_entityMap[uuid] = item;
	QIcon icon;
	icon.addFile(":/images/entity.png");
	item->setIcon(icon);
	item->setEditable(false);
	m_treeviewEntityRoot->appendRow(item);
}

void MainWindow::on_actDeleteEntity_triggered()
{
	auto item = m_entityTreeModel->itemFromIndex(m_entityTreeSelectModel->currentIndex());
	uint64_t uuid = item->data().toULongLong();
	GLOBAL_SCENE->destroyEntity(GLOBAL_SCENE->getEntityByUUID(uuid));
}

void MainWindow::on_actNavmeshParam_triggered()
{
	navmeshdlg->show();
}

void MainWindow::on_actImportModel_triggered()
{
	QString qfilename = QFileDialog::getOpenFileName(this, QString::fromLocal8Bit("打开模型"), QDir::currentPath(), QString::fromLocal8Bit("obj模型(*.obj);;fbx模型(*.fbx)"));

	if (!qfilename.isEmpty())
	{
		std::filesystem::path filename = qfilename.toStdString();
		if (!std::filesystem::exists(GLOBAL_MODEL_PATH / filename.filename()))
		{
			std::filesystem::copy(filename, GLOBAL_MODEL_PATH);
			
		}
		GLOBAL_THREAD_POOL->enqueue([=]() {
			GLOBAL_ASSET->insertMesh(filename.filename());
#if 0 // test progress bar
			GLOBAL_MAINWINDOW->progressBegin(5);
			for (size_t i = 0; i < 5; i++)
			{
				std::this_thread::sleep_for(std::chrono::seconds(3));
				GLOBAL_MAINWINDOW->progressTick();
			}
			GLOBAL_MAINWINDOW->progressEnd();
#endif
		});
	}
}

void MainWindow::on_actAddModelToEntity_triggered()
{
	auto addMeshToEntityDlg = new AddMeshToEntityDlg(m_meshTableModel, m_meshTableSelectModel, m_textureTableModel, m_textureTableSelectModel, this);
	auto rnt = addMeshToEntityDlg->exec();
	if (rnt == QDialog::Accepted)
	{
		auto entityitem = m_entityTreeModel->itemFromIndex(m_entityTreeSelectModel->currentIndex());
		auto modelitem = m_meshTableModel->itemFromIndex(m_meshTableSelectModel->currentIndex());
		auto textureitem = m_textureTableModel->itemFromIndex(m_textureTableSelectModel->currentIndex());
		if (modelitem == nullptr || textureitem == nullptr || modelitem->data().isNull() || textureitem->data().isNull())
		{
			QMessageBox msgBox;
			msgBox.setIcon(QMessageBox::Icon::Critical);
			msgBox.setText(QString::fromLocal8Bit("未选择模型或者贴图"));
			msgBox.exec();
			return;
		}

		uint64_t uuid = entityitem->data().toULongLong();
		auto entity = GLOBAL_SCENE->getEntityByUUID(uuid);
		
		auto& materialComponent = entity.addComponent<GU::MaterialComponent>(modelitem->data().toULongLong(), textureitem->data().toULongLong());
	}
	slot_on_entityTreeSelectModel_currentChanged(m_entityTreeSelectModel->currentIndex(), m_entityTreeSelectModel->currentIndex());
}

void MainWindow::on_actAddSkeletalModelToEntity_triggered()
{
	auto addMeshToEntityDlg = new AddMeshToEntityDlg(m_skeletalmeshTableModel, m_skeletalmeshTableSelectModel, m_textureTableModel, m_textureTableSelectModel, this);
	auto rnt = addMeshToEntityDlg->exec();
	if (rnt == QDialog::Accepted)
	{
		auto entityitem = m_entityTreeModel->itemFromIndex(m_entityTreeSelectModel->currentIndex());
		auto modelitem = m_skeletalmeshTableModel->itemFromIndex(m_skeletalmeshTableSelectModel->currentIndex());
		auto textureitem = m_textureTableModel->itemFromIndex(m_textureTableSelectModel->currentIndex());
		if (modelitem == nullptr || textureitem == nullptr || modelitem->data().isNull() || textureitem->data().isNull())
		{
			QMessageBox msgBox;
			msgBox.setIcon(QMessageBox::Icon::Critical);
			msgBox.setText(QString::fromLocal8Bit("未选择模型或者贴图"));
			msgBox.exec();
			return;
		}
		uint64_t uuid = entityitem->data().toULongLong();
		auto entity = GLOBAL_SCENE->getEntityByUUID(uuid);
		auto& materialComponent = entity.addComponent<GU::SkeletalMeshComponent>(modelitem->data().toULongLong(), textureitem->data().toULongLong());
		materialComponent.currentAnimation = GLOBAL_ANIMATION->getAnimationsWithUUID(GLOBAL_ASSET->getSkeletalMeshWithUUID(modelitem->data().toULongLong())->meshs[0].animationID).begin()->first;
	}
	slot_on_entityTreeSelectModel_currentChanged(m_entityTreeSelectModel->currentIndex(), m_entityTreeSelectModel->currentIndex());
}

void MainWindow::on_actImportTexture_triggered()
{
	QString qfilename = QFileDialog::getOpenFileName(this, QString::fromLocal8Bit("打开模型"), QDir::currentPath(), QString::fromLocal8Bit("图片(*.png *.jpg);;"));
	if (!qfilename.isEmpty())
	{
		std::filesystem::path filepath = qfilename.toStdString();
		if (!std::filesystem::exists(GLOBAL_ASSET_PATH / "textures" / filepath.filename()))
		{
			std::filesystem::copy(filepath, GLOBAL_TEXTURE_PATH);

		}
		GLOBAL_THREAD_POOL->enqueue([=]() {
			GLOBAL_ASSET->insertTexture(filepath.filename());
			});
	}
}

void MainWindow::on_actImportSkeletalMesh_triggered()
{
	QString qfilename = QFileDialog::getOpenFileName(this, QString::fromLocal8Bit("打开模型"), QDir::currentPath(), QString::fromLocal8Bit("fbx模型(*.fbx)"));

	if (!qfilename.isEmpty())
	{
		std::filesystem::path filename = qfilename.toStdString();
		if (!std::filesystem::exists(GLOBAL_MODEL_PATH / filename.filename()))
		{
			std::filesystem::copy(filename, GLOBAL_MODEL_PATH);

		}
		GLOBAL_THREAD_POOL->enqueue([=]() {
			GLOBAL_ASSET->insertSkeletalMesh(filename.filename());
			});
	}
}

void MainWindow::on_actAgentParam_triggered()
{
	auto rnt = agentParam->exec();

	if (rnt == QDialog::Accepted)
	{
		ui->actAddAgent->setEnabled(true);
		ui->actAgentTarget->setEnabled(true);
	}
}

void MainWindow::on_actAgentTarget_triggered()
{
	bool isChecked = ui->actAgentTarget->isChecked();
	GLOBAL_RCSCHEDULER->isSetTarget = isChecked;
	GLOBAL_RCSCHEDULER->isSetAgent = false;
	ui->actAddAgent->setChecked(false);
}

void MainWindow::on_actAddAgent_triggered()
{
	bool isChecked = ui->actAddAgent->isChecked();
	GLOBAL_RCSCHEDULER->isSetAgent = isChecked;
	GLOBAL_RCSCHEDULER->isSetTarget = false;
	ui->actAgentTarget->setChecked(false);
}

void MainWindow::on_actSaveAgent_triggered()
{
	QFileDialog filedial;
	auto rnt = filedial.exec();
	if (rnt == QDialog::Accepted)
	{
		std::filesystem::path savepath = filedial.getSaveFileName().toStdString();
		GLOBAL_RCSCHEDULER->saveAgent(savepath);
	}
}

void MainWindow::on_actReadAgent_triggered()
{
	QFileDialog filedial;
	auto rnt = filedial.exec();
	if (rnt == QDialog::Accepted)
	{
		std::filesystem::path readPath = filedial.getOpenFileName().toStdString();
		GLOBAL_RCSCHEDULER->readAgent(readPath);
	}
}

void MainWindow::slot_treeviewEntity_customcontextmenu(const QPoint& point)
{
	QMenu* menu = new QMenu(this);
	menu->addAction(ui->actCreateEntity);
	menu->addAction(ui->actCopyEntity);
	menu->addAction(ui->actDeleteEntity);
	menu->addAction(ui->actAddModelToEntity);
	menu->addAction(ui->actAddSkeletalModelToEntity);
	menu->exec(QCursor::pos());
}

void MainWindow::slot_on_entityTreeSelectModel_currentChanged(const QModelIndex& currentIndex, const QModelIndex& previousIndex)
{
	clearAllComponentProperty();
	auto item = m_entityTreeModel->itemFromIndex(currentIndex);
	if (item == nullptr)
	{
		return;
	}
	UINT64 uuid = item->data().toULongLong();
	if (uuid == 0) return;
	auto entity = GLOBAL_SCENE->getEntityByUUID(uuid);
	
	if (entity.hasComponent<GU::TagComponent>())
	{
		auto tag = entity.getComponent<GU::TagComponent>().Tag.c_str();
		tagProperty->setValue(tag);
		ui->componentTreeWidget->add(tagProperty);
	}

	if (entity.hasComponent<GU::IDComponent>())
	{
		auto uuid = entity.getComponent<GU::IDComponent>().ID;
		uuidProperty->setValue(QString("%1").arg(uuid));
		ui->componentTreeWidget->add(uuidProperty);
	}

	if (entity.hasComponent<GU::TransformComponent>())
	{
		auto translation = entity.getComponent<GU::TransformComponent>().Translation;
		auto rotation = entity.getComponent<GU::TransformComponent>().Rotation;
		auto scale = entity.getComponent<GU::TransformComponent>().Scale;

		pxProperty->setValue(translation.x);
		pyProperty->setValue(translation.y);
		pzProperty->setValue(translation.z);

		rxProperty->setValue(rotation.x * 180.0/3.14);
		ryProperty->setValue(rotation.y * 180.0/3.14);
		rzProperty->setValue(rotation.z * 180.0/3.14);

		sxProperty->setValue(scale.x);
		syProperty->setValue(scale.y);
		szProperty->setValue(scale.z);

		ui->componentTreeWidget->add(tsfheader);
		ui->componentTreeWidget->add(posheader);
		ui->componentTreeWidget->add(rotheader);
		ui->componentTreeWidget->add(sclheader);


		ui->componentTreeWidget->add(pxProperty);
		ui->componentTreeWidget->add(pyProperty);
		ui->componentTreeWidget->add(pzProperty);

		ui->componentTreeWidget->add(rxProperty);
		ui->componentTreeWidget->add(ryProperty);
		ui->componentTreeWidget->add(rzProperty);

		ui->componentTreeWidget->add(sxProperty);
		ui->componentTreeWidget->add(syProperty);
		ui->componentTreeWidget->add(szProperty);
		ui->componentTreeWidget->add(szProperty);
	}

	if (entity.hasComponent<GU::MaterialComponent>())
	{
		auto meshuuid = entity.getComponent<GU::MaterialComponent>().material.meshUUID;
		auto textureuuid = entity.getComponent<GU::MaterialComponent>().material.textureUUID;
		meshuuidProperty->setValue(QString::number(meshuuid));
		meshProperty->setValue(GLOBAL_ASSET->getMeshPathWithUUID(meshuuid).string().c_str());
		textureuuidProperty->setValue(QString::number(textureuuid));
		textureProperty->setValue(GLOBAL_ASSET->getTexturePathWithUUID(textureuuid).string().c_str());
		ui->componentTreeWidget->add(materialheader);
		ui->componentTreeWidget->add(meshProperty);
		ui->componentTreeWidget->add(meshuuidProperty);
		ui->componentTreeWidget->add(textureProperty);
		ui->componentTreeWidget->add(textureuuidProperty);
	}

	if (entity.hasComponent<GU::SkeletalMeshComponent>())
	{
		auto meshuuid = entity.getComponent<GU::SkeletalMeshComponent>().material.skeletalMeshUUID;
		auto textureuuid = entity.getComponent<GU::SkeletalMeshComponent>().material.textureUUID;
		auto currentanimation = entity.getComponent<GU::SkeletalMeshComponent>().currentAnimation;
		skeletalMeshuuidProperty->setValue(QString::number(meshuuid));
		skeletalMeshProperty->setValue(GLOBAL_ASSET->getSkeletalMeshPathWithUUID(meshuuid).string().c_str());
		skeletalMeshtextureuuidProperty->setValue(QString::number(textureuuid));
		skeletalMeshtextureProperty->setValue(GLOBAL_ASSET->getTexturePathWithUUID(textureuuid).string().c_str());
		auto animations = GLOBAL_ANIMATION->getAnimationsWithUUID(GLOBAL_ASSET->getSkeletalMeshWithUUID(meshuuid)->meshs[0].animationID);

		CListData listdata;
		int currentindex = 0;
		int i = 0;
		for (auto&& animation : animations)
		{
			listdata.push_back({ animation.first.c_str() });
			if (currentanimation == animation.first.c_str())
			{
				currentindex = i;
			}
			i++;
		}
		
		skeletalCurrentAnimationProperty->setList(listdata);
		skeletalCurrentAnimationProperty->setIndex(currentindex);
		ui->componentTreeWidget->add(skeletalMaterialheader);
		ui->componentTreeWidget->add(skeletalMeshProperty);
		ui->componentTreeWidget->add(skeletalMeshuuidProperty);
		ui->componentTreeWidget->add(skeletalMeshtextureProperty);
		ui->componentTreeWidget->add(skeletalMeshtextureuuidProperty);
		ui->componentTreeWidget->add(skeletalCurrentAnimationProperty);
	}
	

	ui->componentTreeWidget->adjustToContents();
}

void MainWindow::slot_on_meshTableSelectModel_currentChanged(const QModelIndex& current, const QModelIndex& previous)
{

	auto item = m_meshTableModel->itemFromIndex(current);
	if (item->text().isEmpty())
	{
		setStatus("");
	}
	else
	{
		setStatus(QString::fromLocal8Bit("当前选择模型: %1 uuid: %2").arg(item->text()).arg(item->data().toString()));
	}
}

void MainWindow::slot_on_textureTableSelectModel_currentChanged(const QModelIndex& current, const QModelIndex& previous)
{

	auto item = m_textureTableModel->itemFromIndex(current);
	if (item->text().isEmpty())
	{
		setStatus("");
	}
	else
	{
		setStatus(QString::fromLocal8Bit("当前选择贴图: %1 uuid: %2").arg(item->text()).arg(item->data().toString()));
	}
}

void MainWindow::slot_importResource2Table(QString filename, uint64_t uuid, int type)
{
	QStandardItem* item = new QStandardItem(filename);
	item->setEditable(false);
	item->setIcon(QIcon(":/images/model.png"));
	item->setData(uuid);

	::GU::Asset::AssetType assettype = (::GU::Asset::AssetType)type;

	switch (assettype)
	{
	case GU::Asset::AssetType::Mesh:
		m_meshTableModel->setItem(m_numMeshInTable / 15, m_numMeshInTable % 15, item);
		ui->modelTableView->update();
		m_numMeshInTable++;
		break;
	case GU::Asset::AssetType::SkeletalMesh:
		m_skeletalmeshTableModel->setItem(m_numSkeletalMeshInTable / 15, m_numSkeletalMeshInTable % 15, item);
		ui->skeletalMeshTableView->update();
		m_numSkeletalMeshInTable++;
		break;
	case GU::Asset::AssetType::Texture:
		m_textureTableModel->setItem(m_numTextureInTable / 15, m_numTextureInTable % 15, item);
		ui->textureTableView->update();
		m_numTextureInTable++;
		break;
	default:
		break;
	}
}
void MainWindow::slot_progressTick(int max)
{
	if (max != 0 && max != -1)
	{
		m_progressTaskNum = max;
		m_progressBar->setMaximum(max);
		m_progressBar->setValue(0);
		m_progressBar->show();
	}

	m_currentTaskNo++;

	if (m_currentTaskNo > m_progressTaskNum || max == -1)
	{
		m_currentTaskNo = 0;
		m_progressTaskNum = 0;
		m_progressBar->setMaximum(0);
		m_progressBar->hide();
	}
	else
	{
		m_progressBar->setValue(m_currentTaskNo);
	}
}
void MainWindow::slot_tagPropertyChanged()
{
	auto item = m_entityTreeModel->itemFromIndex(m_entityTreeSelectModel->currentIndex());
	std::string uuidstring = item->data().toString().toStdString();
	uint64_t uuid;
	sscanf(uuidstring.c_str(), "%llu", &uuid);
	if (uuid == 0) return;
	auto entity = GLOBAL_SCENE->getEntityByUUID(uuid);
	auto tag = entity.getComponent<GU::TagComponent>().Tag;
	item->setText(tag.c_str());
}