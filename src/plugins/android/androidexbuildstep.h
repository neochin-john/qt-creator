#pragma once

#include <projectexplorer/makestep.h>
#include <projectexplorer/processstep.h>
#include <utils/futuresynchronizer.h>
#include <utils/commandline.h>

namespace ProjectExplorer {
class BuildStepList;
}

namespace Utils {
class FilePath;
}

namespace Android {
namespace Internal {

class AndroidBuildApkStep;
class AndroidConfigGetter
{
public:
    AndroidConfigGetter(ProjectExplorer::BuildStep *step);
    bool isRelease();

    Utils::FilePath androidBuildDirectory();
    QString quotedAndroidBuildDirectory();

    Utils::FilePath adbPath();
    Utils::FilePath zipAlignPath();
    Utils::FilePath apkSignerPath();

    Utils::FilePath unsignedApkPath();
    Utils::FilePath signedApkPath();
    Utils::FilePath apkPath();
    bool signApk();

    QString packageName();
    QString deviceSN();

protected:
    QString buildToolsVersion();
    AndroidBuildApkStep* androidBuildApkStep();

private:
    ProjectExplorer::BuildStep *m_step;
};


class AndroidExMakeLibStep : public ProjectExplorer::MakeStep, public AndroidConfigGetter
{
    Q_OBJECT

public:
    AndroidExMakeLibStep(ProjectExplorer::BuildStepList *bsl, Utils::Id id);
};

class AndroidExApkAlignStep : public ProjectExplorer::AbstractProcessStep, public AndroidConfigGetter
{
    Q_OBJECT

public:
    AndroidExApkAlignStep(ProjectExplorer::BuildStepList *bsl, Utils::Id id);
};

class AndroidExApkSignStep : public ProjectExplorer::AbstractProcessStep, public AndroidConfigGetter
{
    Q_OBJECT

public:
    AndroidExApkSignStep(ProjectExplorer::BuildStepList *bsl, Utils::Id id);
};

class AndroidExAppUninstallStep : public ProjectExplorer::AbstractProcessStep, public AndroidConfigGetter
{
    Q_OBJECT

public:
    AndroidExAppUninstallStep(ProjectExplorer::BuildStepList *bsl, Utils::Id id);
};

class AndroidExApkReinstallStep : public ProjectExplorer::BuildStep, public AndroidConfigGetter
{
    Q_OBJECT

public:
    AndroidExApkReinstallStep(ProjectExplorer::BuildStepList *bsl, Utils::Id id);

    bool init() override;

private:
    Tasking::GroupItem runRecipe() override;

    void runImpl(QPromise<void> &promise);
    QString m_adbCommand;
    QStringList m_arguments;
    Utils::FutureSynchronizer m_synchronizer;
};

class AndroidExApkBuildStep : public ProjectExplorer::AbstractProcessStep
{
    Q_OBJECT

public:
    AndroidExApkBuildStep(ProjectExplorer::BuildStepList *bsl, Utils::Id id);
};

class AndroidExAssetsMakeConfigStep;
class AndroidExAssetsMakeStep : public ProjectExplorer::MakeStep
{
    Q_OBJECT

public:
    AndroidExAssetsMakeStep(ProjectExplorer::BuildStepList *bsl, Utils::Id id);
    void configParams(AndroidExAssetsMakeConfigStep *configStep);
};


class AndroidExAssetsMakeConfigStep : public ProjectExplorer::MakeStep
{
    Q_OBJECT

public:
    AndroidExAssetsMakeConfigStep(ProjectExplorer::BuildStepList *bsl, Utils::Id id);

    bool init() override;

private:
    friend class AndroidExAssetsMakeStep;
    Tasking::GroupItem runRecipe() override;
};


class AndroidExAssetsMakeConfigStepFactory : public ProjectExplorer::BuildStepFactory
{
public:
    AndroidExAssetsMakeConfigStepFactory();
};


class AndroidExAssetsCmdConfigStep;
class AndroidExAssetsCmdStep : public ProjectExplorer::AbstractProcessStep
{
    Q_OBJECT

public:
    AndroidExAssetsCmdStep(ProjectExplorer::BuildStepList *bsl, Utils::Id id);
    void configParams(AndroidExAssetsCmdConfigStep *configStep);

private:
    Utils::FilePathAspect m_command{this};
    Utils::StringAspect m_arguments{this};
    Utils::FilePathAspect m_workingDirectory{this};
};


class AndroidExAssetsCmdConfigStep : public ProjectExplorer::AbstractProcessStep
{
    Q_OBJECT

public:
    AndroidExAssetsCmdConfigStep(ProjectExplorer::BuildStepList *bsl, Utils::Id id);

private:
    friend class AndroidExAssetsCmdStep;
    Tasking::GroupItem runRecipe() override;

    Utils::FilePathAspect m_command{this};
    Utils::StringAspect m_arguments{this};
    Utils::FilePathAspect m_workingDirectory{this};
};


class AndroidExAssetsCmdConfigStepFactory : public ProjectExplorer::BuildStepFactory
{
public:
    AndroidExAssetsCmdConfigStepFactory();
};


class AndroidExAssetsCopyConfigStep;
class AndroidExAssetsCopyStep : public ProjectExplorer::BuildStep
{
    Q_OBJECT

public:
    AndroidExAssetsCopyStep(ProjectExplorer::BuildStepList *bsl, Utils::Id id);
    void configParams(AndroidExAssetsCopyConfigStep *configStep);

    bool init() override;

private:
    Tasking::GroupItem runRecipe() override;

    Utils::FilePathAspect m_sourceAspect{this};
    Utils::FilePathAspect m_targetAspect{this};
};


class AndroidExAssetsCopyConfigStep : public ProjectExplorer::BuildStep
{
    Q_OBJECT

public:
    AndroidExAssetsCopyConfigStep(ProjectExplorer::BuildStepList *bsl, Utils::Id id);

    bool init() override;

private:
    friend class AndroidExAssetsCopyStep;
    Tasking::GroupItem runRecipe() override;

    Utils::FilePathAspect m_sourceAspect{this};
    Utils::FilePathAspect m_targetAspect{this};
};

class AndroidExAssetsCopyConfigStepFactory : public ProjectExplorer::BuildStepFactory
{
public:
    AndroidExAssetsCopyConfigStepFactory();
};

class AndroidExAssetsCleanConfigStep;
class AndroidExAssetsCleanStep : public ProjectExplorer::BuildStep
{
    Q_OBJECT

public:
    AndroidExAssetsCleanStep(ProjectExplorer::BuildStepList *bsl, Utils::Id id);
    void configParams(AndroidExAssetsCleanConfigStep *configStep);

    bool init() override;

private:
    Tasking::GroupItem runRecipe() override;

    Utils::FilePathAspect m_targetAspect{this};
};


class AndroidExAssetsCleanConfigStep : public ProjectExplorer::BuildStep
{
    Q_OBJECT

public:
    AndroidExAssetsCleanConfigStep(ProjectExplorer::BuildStepList *bsl, Utils::Id id);

    bool init() override;

private:
    friend class AndroidExAssetsCleanStep;
    Tasking::GroupItem runRecipe() override;

    Utils::FilePathAspect m_targetAspect{this};
};


class AndroidExAssetsCleanConfigStepFactory : public ProjectExplorer::BuildStepFactory
{
public:
    AndroidExAssetsCleanConfigStepFactory();
};


} // namespace Internal
} // namespace Android
