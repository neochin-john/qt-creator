#include "androidexbuildstep.h"
#include "androidconstants.h"
#include "androidexconstants.h"
#include "androidmanager.h"
#include "androidconfigurations.h"
#include "androidbuildapkstep.h"
#include "androiddevice.h"
#include "androidtr.h"

#include <projectexplorer/projectexplorerconstants.h>
#include <projectexplorer/processparameters.h>
#include <projectexplorer/target.h>
#include <projectexplorer/buildsteplist.h>
#include <projectexplorer/kitaspects.h>

#include <solutions/tasking/tasktree.h>

#include <utils/layoutbuilder.h>
#include <utils/pathchooser.h>
#include <utils/environment.h>
#include <utils/commandline.h>
#include <utils/filestreamer.h>
#include <utils/async.h>
#include <utils/process.h>

#include <qtsupport/qtkitaspect.h>

#include <QComboBox>
#include <QPushButton>

namespace Android {
namespace Internal {


AndroidConfigGetter::AndroidConfigGetter(ProjectExplorer::BuildStep *step)
    : m_step{step}
{

}

bool AndroidConfigGetter::isRelease()
{
    ProjectExplorer::Target *target = m_step->target();
    ProjectExplorer::BuildConfiguration *buildConfiguration = target->activeBuildConfiguration();
    return buildConfiguration->buildType() == ProjectExplorer::BuildConfiguration::Release;
}

Utils::FilePath AndroidConfigGetter::androidBuildDirectory()
{
    return AndroidManager::androidBuildDirectory(m_step->target());
}

QString AndroidConfigGetter::quotedAndroidBuildDirectory()
{
    QString buildPath = androidBuildDirectory().toString();
    if (Utils::HostOsInfo::isWindowsHost()) {
        buildPath = QDir::toNativeSeparators(buildPath);
    }
    return Utils::ProcessArgs::quoteArg(buildPath);
}

Utils::FilePath AndroidConfigGetter::adbPath()
{
    Android::AndroidConfig config = Android::AndroidConfigurations::currentConfig();
    Utils::FilePath sdkPath = config.sdkLocation();
    return sdkPath.pathAppended("/platform-tools/adb").withExecutableSuffix();
}

Utils::FilePath AndroidConfigGetter::zipAlignPath()
{
    Android::AndroidConfig config = Android::AndroidConfigurations::currentConfig();
    Utils::FilePath sdkPath = config.sdkLocation();
    return sdkPath.pathAppended("/build-tools/" + buildToolsVersion() +"/zipalign").withExecutableSuffix();
}

Utils::FilePath AndroidConfigGetter::apkSignerPath()
{

    Android::AndroidConfig config = Android::AndroidConfigurations::currentConfig();
    Utils::FilePath sdkPath = config.sdkLocation();
    Utils::FilePath apkSignerPath = sdkPath.pathAppended("/build-tools/" + buildToolsVersion() +"/apksigner");
    if (Utils::HostOsInfo::isWindowsHost()) {
        apkSignerPath = apkSignerPath.stringAppended(".bat");
    }

    return apkSignerPath;
}

Utils::FilePath AndroidConfigGetter::unsignedApkPath()
{
    Utils::FilePath pkgDir = androidBuildDirectory().pathAppended("/build/outputs/apk");
    return isRelease() ? pkgDir.pathAppended("/release/android-build-release-unsigned.apk")
                       : pkgDir.pathAppended("/debug/android-build-debug-unsigned.apk");
}

Utils::FilePath AndroidConfigGetter::signedApkPath()
{
    Utils::FilePath pkgDir = androidBuildDirectory().pathAppended("/build/outputs/apk");
    return isRelease() ? pkgDir.pathAppended("/release/android-build-release-signed.apk")
                       : pkgDir.pathAppended("/debug/android-build-debug-signed.apk");
}

Utils::FilePath AndroidConfigGetter::apkPath()
{
    return signApk() ? signedApkPath() : unsignedApkPath();
}

bool AndroidConfigGetter::signApk()
{
    AndroidBuildApkStep *buildApkStep = androidBuildApkStep();
    return buildApkStep->signPackage();
}

QString AndroidConfigGetter::packageName()
{
    return AndroidManager::packageName(m_step->target());
}

QString AndroidConfigGetter::deviceSN()
{
    /* 有时AndroidManager::deviceSerialNumber得到的并不是当前设备，以下方法可以。
     * 但是部署运行还是不能正确找到设备。项目整体重新构建运行即可，然后deviceSerialNumber
     * 也可用了。*/
    /*
    const auto dev = static_cast<const Android::Internal::AndroidDevice *>(ProjectExplorer::DeviceKitAspect::device(m_step->target()->kit()).data());
    if (!dev) {
        return "";
    }

    auto info = AndroidDevice::androidDeviceInfoFromIDevice(dev);
    if (!info.isValid()) {
        return "";
    }

    return info.serialNumber;
    */

    return AndroidManager::deviceSerialNumber(m_step->target());
}

QString AndroidConfigGetter::buildToolsVersion()
{
    AndroidBuildApkStep *buildApkStep = androidBuildApkStep();
    QString ver = buildApkStep->buildToolsVersion().toString();
    if (ver != "") {
        return ver;
    }

    QtSupport::QtVersion *qt = QtSupport::QtKitAspect::qtVersion(m_step->target()->kit());
    const int minApiSupported = AndroidManager::defaultMinimumSDK(qt);
    const QList<QVersionNumber> versions = Utils::transform(
            AndroidConfigurations::sdkManager()->filteredBuildTools(minApiSupported),
            [](const BuildTools *pkg) {
        return pkg->revision();
    });
    return versions.last().toString();
}

AndroidBuildApkStep *AndroidConfigGetter::androidBuildApkStep()
{
    ProjectExplorer::Target *target = m_step->target();
    ProjectExplorer::BuildConfiguration *buildConfiguration = target->activeBuildConfiguration();
    BuildStepList *bsl = buildConfiguration->buildSteps();
    return bsl->firstOfType<Android::Internal::AndroidBuildApkStep>();
}

AndroidExApkBuildStep::AndroidExApkBuildStep(ProjectExplorer::BuildStepList *bsl, Utils::Id id)
    : ProjectExplorer::AbstractProcessStep(bsl, id)
{
    setLowPriority();
    setWorkingDirectoryProvider([this]() {
        ProjectExplorer::Target *target = this->target();
        return AndroidManager::androidBuildDirectory(target);
    });
    setCommandLineProvider([this]() {
        ProjectExplorer::Target *target = this->target();
        Utils::FilePath buildDir = AndroidManager::androidBuildDirectory(target);

    #if defined(Q_OS_WIN32)
        Utils::FilePath gradlePath = buildDir.pathAppended("gradlew.bat");
    #else
        Utils::FilePath gradlePath = buildDir.pathAppended("gradlew");
    #endif
        Utils::CommandLine cmd(gradlePath);
        cmd.addArg("assembleRelease");
        return cmd;
    });
}


AndroidExMakeLibStep::AndroidExMakeLibStep(ProjectExplorer::BuildStepList *bsl, Utils::Id id)
    : ProjectExplorer::MakeStep{bsl, id}
    , AndroidConfigGetter{this}
{
    setCommandLineProvider([this]() {
        const QString innerQuoted = quotedAndroidBuildDirectory();
        const QString outerQuoted = Utils::ProcessArgs::quoteArg("INSTALL_ROOT=" + innerQuoted);

        Utils::FilePath makePath = makeExecutable();
        Utils::CommandLine cmd(makePath);
        cmd.addArgs(QString("%1 install").arg(outerQuoted), Utils::CommandLine::Raw);
        return cmd;
    });
}


AndroidExApkAlignStep::AndroidExApkAlignStep(ProjectExplorer::BuildStepList *bsl, Utils::Id id)
    : ProjectExplorer::AbstractProcessStep{bsl, id}
    , AndroidConfigGetter{this}
{
    setCommandLineProvider([this]() {
        Utils::FilePath zipAlign = zipAlignPath();
        QString unsignedApk = unsignedApkPath().toString();
        QString signedApk = signedApkPath().toString();
        Utils::CommandLine cmd(zipAlign);
        cmd.addArgs(QString("-v -f 4 %1 %2").arg(unsignedApk, signedApk), Utils::CommandLine::Raw);
        return cmd;
    });
}

AndroidExApkSignStep::AndroidExApkSignStep(ProjectExplorer::BuildStepList *bsl, Utils::Id id)
    : ProjectExplorer::AbstractProcessStep{bsl, id}
    , AndroidConfigGetter{this}
{
    setCommandLineProvider([this]() {
        Utils::FilePath apkSigner = apkSignerPath();
        QString signedApk = signedApkPath().toString();
        AndroidBuildApkStep *buildApkStep = androidBuildApkStep();
        Utils::CommandLine cmd(apkSigner);
        cmd.addArgs(QString("sign --ks %1 --ks-pass pass:%2 --ks-key-alias %3 %4")
            .arg(buildApkStep->m_keystorePath.toString(), buildApkStep->m_keystorePasswd,
                 buildApkStep->m_certificateAlias, signedApk),
            Utils::CommandLine::Raw);
        return cmd;
    });
}

AndroidExAppUninstallStep::AndroidExAppUninstallStep(ProjectExplorer::BuildStepList *bsl, Utils::Id id)
    : ProjectExplorer::AbstractProcessStep{bsl, id}
    , AndroidConfigGetter{this}
{
    setCommandLineProvider([this]() {
        Utils::FilePath adb = adbPath();
        QString pkgName = packageName();
        Utils::CommandLine cmd(adb);
        cmd.addArgs(QString("uninstall %1").arg(pkgName), Utils::CommandLine::Raw);
        return cmd;
    });
}

AndroidExApkReinstallStep::AndroidExApkReinstallStep(ProjectExplorer::BuildStepList *bsl, Utils::Id id)
    : ProjectExplorer::BuildStep{bsl, id}
    , AndroidConfigGetter{this}
{
}

bool AndroidExApkReinstallStep::init()
{
    Utils::FilePath adb = adbPath();
    Utils::FilePath apk = apkPath();
    QString device = deviceSN();

    m_adbCommand = adb.toString();

    m_arguments.clear();

    if (device != "") {
        m_arguments << "-s" << device;
    }

    m_arguments << "install" << "-r" << apk.toString();

    return adb.exists() && apk.exists();
}

Tasking::GroupItem AndroidExApkReinstallStep::runRecipe()
{
    using namespace Tasking;
    using namespace Utils;
    const auto onSetup = [this](Async<void> &async) {
        async.setConcurrentCallData(&AndroidExApkReinstallStep::runImpl, this);
        async.setFutureSynchronizer(&m_synchronizer);
    };
    return AsyncTask<void>(onSetup);
}

void AndroidExApkReinstallStep::runImpl(QPromise<void> &promise)
{
    QProcess process;
    process.start(m_adbCommand, m_arguments, QIODevice::ReadOnly);
    connect(&process, &QProcess::readyReadStandardOutput, this, [this, &process]() {
        QByteArray data = process.readAllStandardOutput();
        emit addOutput(QString::fromUtf8(data), OutputFormat::Stdout);
    });
    connect(&process, &QProcess::readyReadStandardError, this, [this, &process]() {
        QByteArray data = process.readAllStandardError();
        emit addOutput(QString::fromUtf8(data), OutputFormat::Stderr);
    });

    QString cmdStr = QString("\"%1\" %2").arg(m_adbCommand, m_arguments.join(" "));
    emit addOutput(Tr::tr("Starting: %1").arg(cmdStr), OutputFormat::NormalMessage);
    process.start();
    while (!process.waitForFinished(3000)) {
        if (process.state() == QProcess::NotRunning) {
            break;
        }

        if (promise.isCanceled()) {
            process.kill();
            process.waitForFinished();
        }
    }

    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
        emit addOutput(Tr::tr("The process \"%1\" exited normally").arg(cmdStr),
                       OutputFormat::ErrorMessage);
        promise.future().cancel();
    }
}

AndroidExAssetsMakeStep::AndroidExAssetsMakeStep(ProjectExplorer::BuildStepList *bsl, Utils::Id id)
    : ProjectExplorer::MakeStep{bsl, id}
{

}

void AndroidExAssetsMakeStep::configParams(AndroidExAssetsMakeConfigStep *configStep)
{
    m_makeCommandAspect.setValue(configStep->m_makeCommandAspect.value());
    m_buildTargetsAspect.setValue(configStep->m_buildTargetsAspect.value());
    m_userArgumentsAspect.setValue(configStep->m_userArgumentsAspect.value());
    m_jobCountAspect.setValue(configStep->m_jobCountAspect.value());
    m_disabledForSubdirsAspect.setValue(configStep->m_disabledForSubdirsAspect.value());
}

AndroidExAssetsMakeConfigStep::AndroidExAssetsMakeConfigStep(ProjectExplorer::BuildStepList *bsl, Utils::Id id)
    : ProjectExplorer::MakeStep(bsl, id)
{
    setWidgetExpandedByDefault(true);
    setDisplayName(tr("AndroidEx Make Assets"));
    setUserArguments("make_assets");
}

bool AndroidExAssetsMakeConfigStep::init()
{
    return true;
}

Tasking::GroupItem AndroidExAssetsMakeConfigStep::runRecipe()
{
    const auto onSetup = [this]() {
        emit addOutput(Tr::tr("AndroidEx (fake) make assets (make)"), OutputFormat::NormalMessage);
    };
    return Tasking::Group{Tasking::onGroupSetup(onSetup)};
}

AndroidExAssetsMakeConfigStepFactory::AndroidExAssetsMakeConfigStepFactory()
{
    registerStep<AndroidExAssetsMakeConfigStep>(AndroidEx::Constants::ASSETS_MAKE_CONFIG_STEP_ID);
    setSupportedDeviceType(Android::Constants::ANDROID_DEVICE_TYPE);
    setSupportedStepList(ProjectExplorer::Constants::BUILDSTEPS_BUILD);
    setDisplayName(QObject::tr("AndroidEx Assets Make"));
    setRepeatable(false);
    setFlags(ProjectExplorer::BuildStep::UniqueStep);
}

AndroidExAssetsCmdStep::AndroidExAssetsCmdStep(ProjectExplorer::BuildStepList *bsl, Utils::Id id)
    : ProjectExplorer::AbstractProcessStep{bsl, id}
{
    setWorkingDirectoryProvider([this] {
        const Utils::FilePath workingDir = m_workingDirectory();
        if (workingDir.isEmpty())
            return Utils::FilePath::fromString(fallbackWorkingDirectory());
        return workingDir;
    });

    setCommandLineProvider([this] {
        return Utils::CommandLine{m_command(), m_arguments(), Utils::CommandLine::Raw};
    });

    addMacroExpander();
}

void AndroidExAssetsCmdStep::configParams(AndroidExAssetsCmdConfigStep *configStep)
{
    m_command.setValue(configStep->m_command.value());
    m_arguments.setValue(configStep->m_arguments.value());
    m_workingDirectory.setValue(configStep->m_workingDirectory.value());
}

const char PROCESS_COMMAND_KEY[] = "AndroidEx.AssetsCmd.Command";
const char PROCESS_WORKINGDIRECTORY_KEY[] = "AndroidEx.AssetsCmd.WorkingDirectory";
const char PROCESS_ARGUMENTS_KEY[] = "AndroidEx.AssetsCmd.Arguments";
AndroidExAssetsCmdConfigStep::AndroidExAssetsCmdConfigStep(ProjectExplorer::BuildStepList *bsl, Utils::Id id)
    : ProjectExplorer::AbstractProcessStep{bsl, id}
{
    m_command.setSettingsKey(PROCESS_COMMAND_KEY);
    m_command.setLabelText(Tr::tr("Command:"));
    m_command.setExpectedKind(Utils::PathChooser::Command);
    m_command.setHistoryCompleter("PE.ProcessStepCommand.History");

    m_arguments.setSettingsKey(PROCESS_ARGUMENTS_KEY);
    m_arguments.setDisplayStyle(Utils::StringAspect::LineEditDisplay);
    m_arguments.setLabelText(Tr::tr("Arguments:"));

    m_workingDirectory.setSettingsKey(PROCESS_WORKINGDIRECTORY_KEY);
    m_workingDirectory.setValue(QString(ProjectExplorer::Constants::DEFAULT_WORKING_DIR));
    m_workingDirectory.setLabelText(Tr::tr("Working directory:"));
    m_workingDirectory.setExpectedKind(Utils::PathChooser::Directory);

    setCommandLineProvider([this] {
        Utils::CommandLine cmd(m_command());
        cmd.addArgs(m_arguments(), Utils::CommandLine::Raw);
        return cmd;
    });
    setSummaryUpdater([this] {
        QString display = displayName();
        if (display.isEmpty())
            display = Tr::tr("AndroidEx Assets Cmd");

        ProcessParameters param;
        setupProcessParameters(&param);
        return param.summary(display);
    });

    addMacroExpander();
}

Tasking::GroupItem AndroidExAssetsCmdConfigStep::runRecipe()
{
    const auto onSetup = [this]() {
        emit addOutput(Tr::tr("AndroidEx (fake) make assets (cmd)"), OutputFormat::NormalMessage);
    };
    return Tasking::Group{Tasking::onGroupSetup(onSetup)};
}

AndroidExAssetsCmdConfigStepFactory::AndroidExAssetsCmdConfigStepFactory()
{
    registerStep<AndroidExAssetsCmdConfigStep>(AndroidEx::Constants::ASSETS_CMD_CONFIG_STEP_ID);
    setSupportedDeviceType(Android::Constants::ANDROID_DEVICE_TYPE);
    setSupportedStepList(ProjectExplorer::Constants::BUILDSTEPS_BUILD);
    setDisplayName(QObject::tr("AndroidEx Assets Command Line"));
    setRepeatable(false);
    setFlags(ProjectExplorer::BuildStep::UniqueStep);
}

AndroidExAssetsCopyStep::AndroidExAssetsCopyStep(ProjectExplorer::BuildStepList *bsl, Utils::Id id)
    : ProjectExplorer::BuildStep{bsl, id}
{
    addMacroExpander();
}

void AndroidExAssetsCopyStep::configParams(AndroidExAssetsCopyConfigStep *configStep)
{
    m_sourceAspect.setValue(configStep->m_sourceAspect.value());
    m_targetAspect.setValue(configStep->m_targetAspect.value());
}

bool AndroidExAssetsCopyStep::init()
{
    return m_sourceAspect().exists();
}

Tasking::GroupItem AndroidExAssetsCopyStep::runRecipe()
{
    const auto onSetup = [this](Utils::FileStreamer &streamer) {
        Utils::FilePath src = m_sourceAspect();
        Utils::FilePath tgt = m_targetAspect();

        QTC_ASSERT(src.isFile(), return Tasking::SetupResult::StopWithError);
        streamer.setSource(src);
        streamer.setDestination(tgt);
        return Tasking::SetupResult::Continue;
    };
    const auto onDone = [this](const Utils::FileStreamer &) {
        emit addOutput(Tr::tr("Copying finished."), OutputFormat::NormalMessage);
    };
    const auto onError = [this](const Utils::FileStreamer &) {
        emit addOutput(Tr::tr("Copying failed."), OutputFormat::ErrorMessage);
    };
    return Utils::FileStreamerTask(onSetup, onDone, onError);
}

const char SOURCE_KEY[] = "AndroidEx.AssetsCopy.Source";
const char TARGET_KEY[] = "AndroidEx.AssetsCopy.Target";
AndroidExAssetsCopyConfigStep::AndroidExAssetsCopyConfigStep(ProjectExplorer::BuildStepList *bsl, Utils::Id id)
    : ProjectExplorer::BuildStep{bsl, id}
{
    m_sourceAspect.setSettingsKey(SOURCE_KEY);
    m_sourceAspect.setLabelText(Tr::tr("Source:"));
    m_sourceAspect.setExpectedKind(Utils::PathChooser::Directory);

    m_targetAspect.setSettingsKey(TARGET_KEY);
    m_targetAspect.setLabelText(Tr::tr("Target:"));
    m_targetAspect.setExpectedKind(Utils::PathChooser::Directory);

    setSummaryUpdater([] {
        return QString("<b>" + Tr::tr("AndroidEx Assets Copy Dir") + "</b>");
    });

    addMacroExpander();
}

bool AndroidExAssetsCopyConfigStep::init()
{
    return true;
}

Tasking::GroupItem AndroidExAssetsCopyConfigStep::runRecipe()
{
    const auto onSetup = [this]() {
        emit addOutput(Tr::tr("AndroidEx (fake) make assets (copy)"), OutputFormat::NormalMessage);
    };
    return Tasking::Group{Tasking::onGroupSetup(onSetup)};
}

AndroidExAssetsCopyConfigStepFactory::AndroidExAssetsCopyConfigStepFactory()
{
    registerStep<AndroidExAssetsCopyConfigStep>(AndroidEx::Constants::ASSETS_COPY_CONFIG_STEP_ID);
    setSupportedDeviceType(Android::Constants::ANDROID_DEVICE_TYPE);
    setSupportedStepList(ProjectExplorer::Constants::BUILDSTEPS_BUILD);
    setDisplayName(QObject::tr("AndroidEx Assets Copy Dir"));
    setRepeatable(false);
    setFlags(ProjectExplorer::BuildStep::UniqueStep);
}

AndroidExAssetsCleanStep::AndroidExAssetsCleanStep(ProjectExplorer::BuildStepList *bsl, Utils::Id id)
    : ProjectExplorer::BuildStep{bsl, id}
{
    addMacroExpander();
}

void AndroidExAssetsCleanStep::configParams(AndroidExAssetsCleanConfigStep *configStep)
{
    m_targetAspect.setValue(configStep->m_targetAspect.value());
}

bool AndroidExAssetsCleanStep::init()
{
    return true;
}

Tasking::GroupItem AndroidExAssetsCleanStep::runRecipe()
{
    using namespace Tasking;
    const auto onSetup = [this]() {
        Utils::FilePath tgt = m_targetAspect();
        emit addOutput(Tr::tr("AndroidEx remove assets dir %1").arg(tgt.toString()),
                       OutputFormat::NormalMessage);
        tgt.removeRecursively();
        return Tasking::SetupResult::Continue;
    };
    return Group{onGroupSetup(onSetup)};
}

const char CLEAN_TARGET_KEY[] = "AndroidEx.AssetsClean.Target";
AndroidExAssetsCleanConfigStep::AndroidExAssetsCleanConfigStep(ProjectExplorer::BuildStepList *bsl, Utils::Id id)
    : ProjectExplorer::BuildStep{bsl, id}
{
    m_targetAspect.setSettingsKey(CLEAN_TARGET_KEY);
    m_targetAspect.setLabelText(Tr::tr("Target:"));
    m_targetAspect.setExpectedKind(Utils::PathChooser::Directory);

    setSummaryUpdater([] {
        return QString("<b>" + Tr::tr("AndroidEx Assets Remove Dir") + "</b>");
    });

    addMacroExpander();
}

bool AndroidExAssetsCleanConfigStep::init()
{
    return true;
}

Tasking::GroupItem AndroidExAssetsCleanConfigStep::runRecipe()
{
    const auto onSetup = [this]() {
        emit addOutput(Tr::tr("AndroidEx (fake) clean assets dir"), OutputFormat::NormalMessage);
    };
    return Tasking::Group{Tasking::onGroupSetup(onSetup)};
}

AndroidExAssetsCleanConfigStepFactory::AndroidExAssetsCleanConfigStepFactory()
{
    registerStep<AndroidExAssetsCleanConfigStep>(AndroidEx::Constants::ASSETS_CLEAN_CONFIG_STEP_ID);
    setSupportedDeviceType(Android::Constants::ANDROID_DEVICE_TYPE);
    setSupportedStepList(ProjectExplorer::Constants::BUILDSTEPS_BUILD);
    setDisplayName(QObject::tr("AndroidEx Assets Remove Dir"));
    setRepeatable(false);
    setFlags(ProjectExplorer::BuildStep::UniqueStep);
}


} // namespace Internal
} // namespace Android
