#include "androidexbuildmanager.h"

#include <coreplugin/actionmanager/actioncontainer.h>
#include <coreplugin/actionmanager/actionmanager.h>
#include <coreplugin/icore.h>

#include <projectexplorer/projectexplorer.h>
#include <projectexplorer/projectexplorerconstants.h>
#include <projectexplorer/project.h>
#include <projectexplorer/projectmanager.h>
#include <projectexplorer/projectnodes.h>
#include <projectexplorer/projecttree.h>
#include <projectexplorer/buildmanager.h>
#include <projectexplorer/target.h>
#include <projectexplorer/kit.h>
#include <projectexplorer/kitaspects.h>
#include <projectexplorer/buildsteplist.h>
#include <projectexplorer/makestep.h>

#include <solutions/tasking/tasktree.h>

#include "androidconstants.h"
#include "androidexconstants.h"
#include "androidexbuildstep.h"
#include "androidbuildapkstep.h"

#include <qmakeprojectmanager/qmakeprojectmanagerconstants.h>

#include <QAction>
#include <QMenu>
#include <QMessageBox>
#include <QScopedPointer>

namespace AndroidEx {
    class BuildConfiguration;
    using BuildConfigurationMap = QHash<ProjectExplorer::BuildConfiguration*, BuildConfiguration*>;
}

class AndroidEx::BuildConfiguration
{
public:
    BuildConfiguration(ProjectExplorer::BuildConfiguration *config);

    ProjectExplorer::BuildConfiguration *m_buildConfig;
    ProjectExplorer::BuildStepList m_buildSteps;

    static BuildConfigurationMap m_map;
};

namespace AndroidEx {

BuildConfigurationMap BuildConfiguration::m_map;

const char EX_BUILDSTEPS_BUILD[] = "AndroidEx.BuildSteps.Build";
BuildConfiguration::BuildConfiguration(ProjectExplorer::BuildConfiguration *config)
    : m_buildConfig(config)
    , m_buildSteps{config, EX_BUILDSTEPS_BUILD}
{}

bool isSupport(ProjectExplorer::Project *project) {
    if (!project) {
        return false;
    }

    ProjectExplorer::Target *target = project->activeTarget();
    ProjectExplorer::Kit *kit = target->kit();
    bool isAndroidKit = ProjectExplorer::DeviceTypeKitAspect::deviceTypeId(kit) == Android::Constants::ANDROID_DEVICE_TYPE;

    ProjectExplorer::BuildConfiguration *buildConfig = target->activeBuildConfiguration();
    bool isRelease = buildConfig->buildType() == ProjectExplorer::BuildConfiguration::Release;

    QString buildDir = buildConfig->buildDirectory().path();
    return isAndroidKit && isRelease;
}

BuildConfiguration* getBuildConfiguration(ProjectExplorer::Project *project) {
    if (!project || !isSupport(project)) {
        return nullptr;
    }

    ProjectExplorer::Target *target = project->activeTarget();
    if (!target) {
        return nullptr;
    }

    ProjectExplorer::BuildConfiguration *config = target->activeBuildConfiguration();
    if (!config) {
        return nullptr;
    }

    BuildConfiguration *exConfig = BuildConfiguration::m_map.value(config);
    if (!exConfig) {
        exConfig = new BuildConfiguration(config);
        BuildConfiguration::m_map[config] = exConfig;
    }

    return exConfig;
}

void removeBuildConfiguration(ProjectExplorer::BuildConfiguration *config) {
    if (!config) {
        return;
    }

    BuildConfiguration::m_map.remove(config);
}

void removeBuildConfiguration(ProjectExplorer::Target *target) {
    if (!target) {
        return;
    }

    QList<ProjectExplorer::BuildConfiguration*> configs = target->buildConfigurations();
    for (QList<ProjectExplorer::BuildConfiguration*>::Iterator confIter = configs.begin();
            confIter != configs.end(); confIter++) {
        removeBuildConfiguration(*confIter);
    }
}

void removeBuildConfiguration(ProjectExplorer::Project *project) {
    if (!project) {
        return;
    }

    using namespace ProjectExplorer;

    QList<Target*> targets = project->targets();
    for (QList<Target*>::Iterator targetIter = targets.begin();
            targetIter != targets.end(); targetIter++) {
        removeBuildConfiguration(*targetIter);
    }
}

}


static AndroidExBuildManager *m_instance = nullptr;

AndroidExBuildManager *AndroidExBuildManager::instance()
{
    return m_instance;
}

AndroidExBuildManager::AndroidExBuildManager(QObject *parent)
    : QObject{parent}
{
    QTC_CHECK(!m_instance);
    m_instance = this;
}

void AndroidExBuildManager::init()
{
    initMenu();
    initProjectContextMenu();
    updateActions();

    ProjectExplorer::ProjectManager *projectManager = ProjectExplorer::ProjectManager::instance();
    connect(projectManager, &ProjectExplorer::ProjectManager::startupProjectChanged,
            this, [this](ProjectExplorer::Project *project) {
        qDebug() << "startup project changed" << project->displayName();
        this->updateActions();
    });
    connect(projectManager, &ProjectExplorer::ProjectManager::targetRemoved,
            this, [](ProjectExplorer::Target *target) {
        qDebug() << "target removed" << target->project()->displayName() << target->displayName();
        AndroidEx::removeBuildConfiguration(target);
    });
    connect(projectManager, &ProjectExplorer::ProjectManager::projectRemoved,
            this, [](ProjectExplorer::Project *project) {
        qDebug() << "project removed" << project->displayName();
        AndroidEx::removeBuildConfiguration(project);
    });

    ProjectExplorer::ProjectTree *projectTree = ProjectExplorer::ProjectTree::instance();
    connect(projectTree, &ProjectExplorer::ProjectTree::aboutToShowContextMenu,
        this, &AndroidExBuildManager::updateActions);
    connect(projectTree, &ProjectExplorer::ProjectTree::currentProjectChanged,
            this, [](ProjectExplorer::Project *project) {
        qDebug() << "current project changed" << project->displayName();
    });

    ProjectExplorer::BuildManager *buildManager = ProjectExplorer::BuildManager::instance();
    connect(buildManager, &ProjectExplorer::BuildManager::buildStateChanged,
            this, &AndroidExBuildManager::updateActions);
    connect(buildManager, &ProjectExplorer::BuildManager::buildQueueFinished,
            this, &AndroidExBuildManager::updateActions);
}


void AndroidExBuildManager::buildCpp(ProjectExplorer::Project *project)
{
    build(project, true, false, false, false);
}

void AndroidExBuildManager::buildCppAndRun(ProjectExplorer::Project *project)
{
    build(project, true, false, true, true);
}

void AndroidExBuildManager::makeAssets(ProjectExplorer::Project *project)
{
    build(project, false, true, false, false);
}

void AndroidExBuildManager::makeAssetsAndRun(ProjectExplorer::Project *project)
{
    build(project, false, true, true, true);
}

void AndroidExBuildManager::buildApkAndRun(ProjectExplorer::Project *project)
{
    build(project, false, false, true, true);
}

void AndroidExBuildManager::installAppAndRun(ProjectExplorer::Project *project)
{
    build(project, false, false, false, true);
}

void AndroidExBuildManager::uninstallApp(ProjectExplorer::Project *project)
{
    if (!project) {
        return;
    }

    using namespace ProjectExplorer;
    AndroidEx::BuildConfiguration *exConfig = AndroidEx::getBuildConfiguration(project);
    if (!exConfig) {
        qDebug() << "android ex config is null";
        return;
    }

    BuildStepList *bsl = &exConfig->m_buildSteps;
    bsl->clear();

    Android::Internal::AndroidExAppUninstallStep *appUninstallStep =
        new Android::Internal::AndroidExAppUninstallStep(bsl, AndroidEx::Constants::APP_UNINSTALL_STEP_ID);
    bsl->appendStep(appUninstallStep);

    ProjectExplorer::BuildManager::buildList(bsl);
}

void AndroidExBuildManager::updateActions()
{
    using namespace ProjectExplorer;
    if (BuildManager::isBuilding() || BuildManager::isDeploying()) {
        setMenuEnabled(false);
        setProjectContextMenuEnabled(false);
        return;
    }

    bool support = AndroidEx::isSupport(ProjectManager::startupProject());
    setMenuEnabled(support);

    support = AndroidEx::isSupport(ProjectTree::currentProject());
    setProjectContextMenuEnabled(support);
}

void AndroidExBuildManager::initMenu()
{
    Core::ActionContainer *menu = Core::ActionManager::createMenu(AndroidEx::Constants::MENU_ID);
    menu->menu()->setTitle(tr("AndroidEx"));
    m_menu = menu->menu();

    m_buildCppAction = new QAction(tr("Build C++"), this);
    Core::Command *buildCppCmd = Core::ActionManager::registerAction(m_buildCppAction,
        AndroidEx::Constants::ACTION_BUILD_CPP_ID, Core::Context(Core::Constants::C_GLOBAL));
    // cmd->setDefaultKeySequence(QKeySequence(tr("Ctrl+Alt+Meta+A")));
    connect(m_buildCppAction, &QAction::triggered, this, [this]() {
        buildCpp(ProjectExplorer::ProjectManager::startupProject());
    });
    menu->addAction(buildCppCmd);

    m_buildCppAndRunAction = new QAction(tr("Build C++ && Run"), this);
    Core::Command *buildCppAndRunCmd = Core::ActionManager::registerAction(m_buildCppAndRunAction,
        AndroidEx::Constants::ACTION_BUILD_CPP_AND_RUN_ID, Core::Context(Core::Constants::C_GLOBAL));
    connect(m_buildCppAndRunAction, &QAction::triggered, this, [this]() {
        buildCppAndRun(ProjectExplorer::ProjectManager::startupProject());
    });
    menu->addAction(buildCppAndRunCmd);

    m_makeAssetsAction = new QAction(tr("Make Assets"), this);
    Core::Command *makeAssetsCmd = Core::ActionManager::registerAction(m_makeAssetsAction,
        AndroidEx::Constants::ACTION_MAKE_ASSETS_ID, Core::Context(Core::Constants::C_GLOBAL));
    connect(m_makeAssetsAction, &QAction::triggered, this, [this]() {
        makeAssets(ProjectExplorer::ProjectManager::startupProject());
    });
    menu->addAction(makeAssetsCmd);

    m_makeAssetsAndRunAction = new QAction(tr("Make Assets && Run"), this);
    Core::Command *makeAssetsAndRunCmd = Core::ActionManager::registerAction(m_makeAssetsAndRunAction,
        AndroidEx::Constants::ACTION_MAKE_ASSETS_AND_RUN_ID, Core::Context(Core::Constants::C_GLOBAL));
    connect(m_makeAssetsAndRunAction, &QAction::triggered, this, [this]() {
        makeAssets(ProjectExplorer::ProjectManager::startupProject());
    });
    menu->addAction(makeAssetsAndRunCmd);

    m_buildApkAndRunAction = new QAction(tr("Build Apk && Run"), this);
    Core::Command *buildApkAndRunCmd = Core::ActionManager::registerAction(m_buildApkAndRunAction,
        AndroidEx::Constants::ACTION_BUILD_APK_AND_RUN_ID, Core::Context(Core::Constants::C_GLOBAL));
    connect(m_buildApkAndRunAction, &QAction::triggered, this, [this]() {
        buildApkAndRun(ProjectExplorer::ProjectManager::startupProject());
    });
    menu->addAction(buildApkAndRunCmd);

    m_installAppAndRunAction = new QAction(tr("Install && Run"), this);
    Core::Command *installAppAndRunCmd = Core::ActionManager::registerAction(m_installAppAndRunAction,
        AndroidEx::Constants::ACTION_INSTALL_APP_AND_RUN_ID, Core::Context(Core::Constants::C_GLOBAL));
    connect(m_installAppAndRunAction, &QAction::triggered, this, [this]() {
        installAppAndRun(ProjectExplorer::ProjectManager::startupProject());
    });
    menu->addAction(installAppAndRunCmd);

    m_uninstallAppAction = new QAction(tr("Uninstall App"), this);
    Core::Command *uninstallAppCmd = Core::ActionManager::registerAction(m_uninstallAppAction,
        AndroidEx::Constants::ACTION_UNINSTALL_APP_ID, Core::Context(Core::Constants::C_GLOBAL));
    connect(m_uninstallAppAction, &QAction::triggered, this, [this]() {
        uninstallApp(ProjectExplorer::ProjectManager::startupProject());
    });
    menu->addAction(uninstallAppCmd);

    Core::ActionContainer *menuBar = Core::ActionManager::actionContainer(Core::Constants::MENU_BAR);
    menuBar->addMenu(Core::ActionManager::actionContainer(Core::Constants::M_HELP), menu);
}

void AndroidExBuildManager::initProjectContextMenu()
{
    Core::ActionContainer *menu = Core::ActionManager::createMenu(AndroidEx::Constants::PROJECT_CONTEXT_MENU_ID);
    menu->menu()->setTitle(tr("AndroidEx"));
    m_projectContextMenu = menu->menu();

    Core::Context projectTreeContext(ProjectExplorer::Constants::C_PROJECT_TREE);

    m_buildCppActionCM = new QAction(tr("Build C++"), this);
    Core::Command *buildCppCmd = Core::ActionManager::registerAction(m_buildCppActionCM,
        AndroidEx::Constants::ACTION_BUILD_CPP_CM_ID, projectTreeContext);
    connect(m_buildCppActionCM, &QAction::triggered, this, [this]() {
        buildCpp(ProjectExplorer::ProjectTree::currentProject());
    });
    menu->addAction(buildCppCmd);

    m_buildCppAndRunActionCM = new QAction(tr("Build C++ && Run"), this);
    Core::Command *buildCppAndRunCmd = Core::ActionManager::registerAction(m_buildCppAndRunActionCM,
        AndroidEx::Constants::ACTION_BUILD_CPP_AND_RUN_CM_ID, projectTreeContext);
    connect(m_buildCppAndRunActionCM, &QAction::triggered, this, [this]() {
        buildCppAndRun(ProjectExplorer::ProjectTree::currentProject());
    });
    menu->addAction(buildCppAndRunCmd);

    m_makeAssetsActionCM = new QAction(tr("Make Assets"), this);
    Core::Command *makeAssetsCmd = Core::ActionManager::registerAction(m_makeAssetsActionCM,
        AndroidEx::Constants::ACTION_MAKE_ASSETS_CM_ID, projectTreeContext);
    connect(m_makeAssetsActionCM, &QAction::triggered, this, [this]() {
        makeAssets(ProjectExplorer::ProjectTree::currentProject());
    });
    menu->addAction(makeAssetsCmd);

    m_makeAssetsAndRunActionCM = new QAction(tr("Make Assets && Run"), this);
    Core::Command *makeAssetsAndRunCmd = Core::ActionManager::registerAction(m_makeAssetsAndRunActionCM,
        AndroidEx::Constants::ACTION_MAKE_ASSETS_AND_RUN_CM_ID, projectTreeContext);
    connect(m_makeAssetsAndRunActionCM, &QAction::triggered, this, [this]() {
        makeAssetsAndRun(ProjectExplorer::ProjectTree::currentProject());
    });
    menu->addAction(makeAssetsAndRunCmd);

    m_buildApkAndRunActionCM = new QAction(tr("Build Apk && Run"), this);
    Core::Command *buildApkAndRunCmd = Core::ActionManager::registerAction(m_buildApkAndRunActionCM,
        AndroidEx::Constants::ACTION_BUILD_APK_AND_RUN_CM_ID, projectTreeContext);
    connect(m_buildApkAndRunActionCM, &QAction::triggered, this, [this]() {
        buildApkAndRun(ProjectExplorer::ProjectTree::currentProject());
    });
    menu->addAction(buildApkAndRunCmd);

    m_installAppAndRunActionCM = new QAction(tr("Install && Run"), this);
    Core::Command *installAppAndRunCmd = Core::ActionManager::registerAction(m_installAppAndRunActionCM,
        AndroidEx::Constants::ACTION_INSTALL_APP_AND_RUN_CM_ID, projectTreeContext);
    connect(m_installAppAndRunActionCM, &QAction::triggered, this, [this]() {
        installAppAndRun(ProjectExplorer::ProjectTree::currentProject());
    });
    menu->addAction(installAppAndRunCmd);

    m_uninstallAppActionCM = new QAction(tr("Uninstall App"), this);
    Core::Command *uninstallAppCmd = Core::ActionManager::registerAction(m_uninstallAppActionCM,
        AndroidEx::Constants::ACTION_UNINSTALL_APP_CM_ID, projectTreeContext);
    connect(m_uninstallAppActionCM, &QAction::triggered, this, [this]() {
        uninstallApp(ProjectExplorer::ProjectTree::currentProject());
    });
    menu->addAction(uninstallAppCmd);

    Core::ActionContainer *projectContextMenu = Core::ActionManager::createMenu(
        ProjectExplorer::Constants::M_PROJECTCONTEXT);
    projectContextMenu->addMenu(menu, ProjectExplorer::Constants::G_PROJECT_BUILD);
}

void AndroidExBuildManager::setMenuEnabled(bool enable)
{
    m_buildCppAction->setEnabled(enable);
    m_buildCppAndRunAction->setEnabled(enable);
    m_makeAssetsAction->setEnabled(enable);
    m_makeAssetsAndRunAction->setEnabled(enable);
    m_buildApkAndRunAction->setEnabled(enable);
    m_installAppAndRunAction->setEnabled(enable);
    m_uninstallAppAction->setEnabled(enable);
}

void AndroidExBuildManager::setProjectContextMenuEnabled(bool enable)
{
    m_buildCppActionCM->setEnabled(enable);
    m_buildCppAndRunActionCM->setEnabled(enable);
    m_makeAssetsActionCM->setEnabled(enable);
    m_makeAssetsAndRunActionCM->setEnabled(enable);
    m_buildApkAndRunActionCM->setEnabled(enable);
    m_installAppAndRunActionCM->setEnabled(enable);
    m_uninstallAppActionCM->setEnabled(enable);
}

void AndroidExBuildManager::build(ProjectExplorer::Project *project, bool cpp, bool assets, bool apk, bool run)
{
    if (!project) {
        return;
    }

    using namespace ProjectExplorer;
    AndroidEx::BuildConfiguration *exConfig = AndroidEx::getBuildConfiguration(project);
    if (!exConfig) {
        qDebug() << "android ex config is null";
        return;
    }

    BuildConfiguration *buildConfig = exConfig->m_buildConfig;
    BuildStepList *sysBsl = buildConfig->buildSteps();
    BuildStepList *bsl = &exConfig->m_buildSteps;
    bsl->clear();

    if (cpp) {
        Android::Internal::AndroidExMakeLibStep *makeLibStep =
            new Android::Internal::AndroidExMakeLibStep(bsl, AndroidEx::Constants::MAKE_LIB_STEP_ID);
        bsl->appendStep(makeLibStep);
    }

    if (assets) {
        auto *assetsCleanConfigStep = sysBsl->firstOfType<Android::Internal::AndroidExAssetsCleanConfigStep>();
        if (assetsCleanConfigStep) {
            Android::Internal::AndroidExAssetsCleanStep *assetsCleanStep =
                new Android::Internal::AndroidExAssetsCleanStep(bsl, AndroidEx::Constants::ASSETS_CLEAN_STEP_ID);
            assetsCleanStep->configParams(assetsCleanConfigStep);
            bsl->appendStep(assetsCleanStep);
        }

        auto *assetsMakeConfigStep = sysBsl->firstOfType<Android::Internal::AndroidExAssetsMakeConfigStep>();
        auto *assetsCmdConfigStep = sysBsl->firstOfType<Android::Internal::AndroidExAssetsCmdConfigStep>();
        auto *assetsCopyConfigStep = sysBsl->firstOfType<Android::Internal::AndroidExAssetsCopyConfigStep>();
        if (assetsMakeConfigStep) {
            Android::Internal::AndroidExAssetsMakeStep *assetsMakeStep =
                new Android::Internal::AndroidExAssetsMakeStep(bsl, AndroidEx::Constants::ASSETS_MAKE_STEP_ID);
            assetsMakeStep->configParams(assetsMakeConfigStep);
            bsl->appendStep(assetsMakeStep);
        } else if (assetsCmdConfigStep) {
            Android::Internal::AndroidExAssetsCmdStep *assetsCmdStep =
                new Android::Internal::AndroidExAssetsCmdStep(bsl, AndroidEx::Constants::ASSETS_CMD_STEP_ID);
            assetsCmdStep->configParams(assetsCmdConfigStep);
            bsl->appendStep(assetsCmdStep);
        } else if (assetsCopyConfigStep){
            Android::Internal::AndroidExAssetsCopyStep *assetsCopyStep =
                new Android::Internal::AndroidExAssetsCopyStep(bsl, AndroidEx::Constants::ASSETS_COPY_STEP_ID);
            assetsCopyStep->configParams(assetsCopyConfigStep);
            bsl->appendStep(assetsCopyStep);
        }
    }

    if (apk) {
        Android::Internal::AndroidExApkBuildStep *apkBuildStep =
            new Android::Internal::AndroidExApkBuildStep(bsl, AndroidEx::Constants::APK_BUILD_STEP_ID);
        bsl->appendStep(apkBuildStep);

        Android::Internal::AndroidBuildApkStep *buildApkStep = sysBsl->firstOfType<Android::Internal::AndroidBuildApkStep>();
        if (buildApkStep->signPackage()) {
            Android::Internal::AndroidExApkAlignStep *apkAlignStep =
                new Android::Internal::AndroidExApkAlignStep(bsl, AndroidEx::Constants::APK_ALIGN_STEP_ID);
            bsl->appendStep(apkAlignStep);

            Android::Internal::AndroidExApkSignStep *apkSignStep =
                new Android::Internal::AndroidExApkSignStep(bsl, AndroidEx::Constants::APK_SIGN_STEP_ID);
            bsl->appendStep(apkSignStep);
        }
    }

    if (run) {
        Android::Internal::AndroidExApkReinstallStep *apkReinstallStep =
            new Android::Internal::AndroidExApkReinstallStep(bsl, AndroidEx::Constants::APK_REINSTALL_STEP_ID);
        bsl->appendStep(apkReinstallStep);
    }

    ProjectExplorer::BuildManager::buildList(bsl);

    if (run) {
        ProjectExplorer::ProjectExplorerPlugin::runProject(project, ProjectExplorer::Constants::NORMAL_RUN_MODE, true);
    }
}
