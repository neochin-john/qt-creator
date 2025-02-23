#ifndef ANDROIDEXBUILDMANAGER_H
#define ANDROIDEXBUILDMANAGER_H

#include <QObject>

class QAction;
class QMenu;

namespace ProjectExplorer {
class Node;
class Project;
}

class AndroidExBuildManager : public QObject
{
    Q_OBJECT
public:
    static AndroidExBuildManager* instance();
    explicit AndroidExBuildManager(QObject *parent = nullptr);

    void init();

    void buildCpp(ProjectExplorer::Project *project);
    void buildCppAndRun(ProjectExplorer::Project *project);
    void makeAssets(ProjectExplorer::Project *project);
    void makeAssetsAndRun(ProjectExplorer::Project *project);
    void buildApkAndRun(ProjectExplorer::Project *project);
    void installAppAndRun(ProjectExplorer::Project *project);
    void uninstallApp(ProjectExplorer::Project *project);

signals:
    void enableChanged(bool enabled);

public slots:
    void updateActions();

private:
    void initMenu();
    void initProjectContextMenu();
    void setMenuEnabled(bool enable);
    void setProjectContextMenuEnabled(bool enable);
    void build(ProjectExplorer::Project *project, bool cpp, bool assets, bool apk, bool run);

private:
    QMenu *m_menu;
    QAction *m_buildCppAction;
    QAction *m_buildCppAndRunAction;
    QAction *m_makeAssetsAction;
    QAction *m_makeAssetsAndRunAction;
    QAction *m_buildApkAndRunAction;
    QAction *m_installAppAndRunAction;
    QAction *m_uninstallAppAction;

    QMenu *m_projectContextMenu;
    QAction *m_buildCppActionCM;
    QAction *m_buildCppAndRunActionCM;
    QAction *m_makeAssetsActionCM;
    QAction *m_makeAssetsAndRunActionCM;
    QAction *m_buildApkAndRunActionCM;
    QAction *m_installAppAndRunActionCM;
    QAction *m_uninstallAppActionCM;
};

#endif // ANDROIDEXBUILDMANAGER_H
