#include "appconfig.h"
#include "processmanager.h"
#include "projectmanager.h"

#include <QFileInfo>
#include <QFileSystemModel>
#include <QHeaderView>
#include <QListView>
#include <QProcess>
#include <QPushButton>
#include <QRegularExpression>
#include <QStandardItemModel>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTreeView>

#include <QtDebug>

class ProjectManager::Priv_t {
public:
    QStringList targets;
    QHash<QString, QString> allTargets;
    QRegularExpression targetFilter{ R"(^(?!Makefile)[a-zA-Z0-9_\\-]+$)", QRegularExpression::MultilineOption };
    QListView *targetView = nullptr;
    ProcessManager *pman;
    QFileInfo makeFile;
};

static QHash<QString, QString> findAllTargets(const QString& text)
{
    QHash<QString, QString> map;
    QRegularExpression re(R"(^([a-zA-Z0-9 \t\\\/_\.\:\-]*?):(?!=)\s*([^#\r\n]*?)\s*$)",
                          QRegularExpression::MultilineOption);
    QRegularExpressionMatchIterator it = re.globalMatch(text);
    while(it.hasNext()) {
        QRegularExpressionMatch me = it.next();
        map.insert(me.captured(1), me.captured(2));
    }
    return map;
}

const QString DISCOVER_PROC = "makeDiscover";
const QString PATCH_PROC = "patch";
const QString DIFF_PROC = "diff";

ProjectManager::ProjectManager(QListView *view, ProcessManager *pman, QObject *parent) :
    QObject(parent),
    priv(new Priv_t)
{
    priv->targetView = view;
    priv->targetView->setModel(new QStandardItemModel(priv->targetView));
    priv->pman = pman;
    priv->pman->setTerminationHandler(DISCOVER_PROC, [this](QProcess *make, int code, QProcess::ExitStatus status) {
        Q_UNUSED(code);
        if (status == QProcess::NormalExit) {
            auto out = QString(make->readAllStandardOutput());
            priv->allTargets = findAllTargets(out);
            priv->targets = priv->allTargets.keys().filter(priv->targetFilter);
            priv->targets.sort();
            auto targetModel = qobject_cast<QStandardItemModel*>(priv->targetView->model());
            if (targetModel) {
                for(auto& t: priv->targets) {
                    auto item = new QStandardItem;
                    auto button = new QPushButton;
                    auto name = QString(t).replace('_', ' ');
                    targetModel->appendRow(item);
                    button->setIcon(QIcon(":/images/actions/run-build.svg"));
                    button->setIconSize(QSize(16, 16));
                    button->setText(name);
                    button->setStyleSheet("text-align: left; padding: 4px;");
                    priv->targetView->setIndexWidget(item->index(), button);
                    item->setSizeHint(button->sizeHint());
                    connect(button, &QPushButton::clicked, [t, this](){ emit targetTriggered(t); });
                }
            }
        }
    });
    priv->pman->setTerminationHandler(PATCH_PROC, [this](QProcess *p, int code, QProcess::ExitStatus status) {
        Q_UNUSED(p);
        Q_UNUSED(code);
        qDebug() << "Finish project creation on " << p->workingDirectory() << " with code" << code << status;
        if (status == QProcess::NormalExit) {
            auto path = QDir(p->workingDirectory()).absoluteFilePath("Makefile");
            qDebug() << "Open" << path;
            openProject(path);
        }
    });
}

ProjectManager::~ProjectManager()
{
    delete priv;
}

QString ProjectManager::projectName() const
{
    return priv->makeFile.absoluteDir().dirName();
}

QString ProjectManager::projectPath() const
{
    return priv->makeFile.canonicalPath();
}

QString ProjectManager::projectFile() const
{
    return priv->makeFile.canonicalFilePath();
}

bool ProjectManager::isProjectOpen() const
{
    return priv->makeFile.exists();
}

void ProjectManager::createProject(const QString& projectFilePath, const QString& templateFile)
{
    AppConfig::ensureExist(projectFilePath);
    priv->pman->setStartupHandler(PATCH_PROC, [templateFile](QProcess *p) {
        p->write(templateFile.toLocal8Bit());
        p->closeWriteChannel();
        p->deleteLater();
    });
    priv->pman->start(PATCH_PROC, "patch", { "-p1" }, {}, projectFilePath);
}

void ProjectManager::exportCurrentProjectTo(const QString &patchFile)
{
    auto tmpDir = QDir(QDir(QStandardPaths::writableLocation(QStandardPaths::TempLocation)).filePath(QString("%1-empty").arg(projectName())));
    if (!tmpDir.exists())
        QDir::root().mkpath(tmpDir.absolutePath());
    priv->pman->setTerminationHandler(DIFF_PROC, [this, tmpDir, patchFile](QProcess *p, int code, QProcess::ExitStatus status) {
        Q_UNUSED(code);
        QDir::root().rmpath(tmpDir.absolutePath());
        if (status == QProcess::NormalExit) {
            QFile f(patchFile);
            if (f.open(QFile::WriteOnly)) {
                f.write(p->readAllStandardOutput());
                f.close();
            }
            if (f.error() == QFile::NoError)
                emit exportFinish(tr("Export sucessfull"));
            else
                emit exportFinish(f.errorString());
        } else
            emit exportFinish(p->errorString());
        p->deleteLater();;
    });
    priv->pman->start(DIFF_PROC, "diff", { "-N", "-u", "-r", tmpDir.absolutePath(), "." }, {}, projectPath());
}

void ProjectManager::openProject(const QString &makefile)
{
    auto doOpenProject = [makefile, this]() {
        priv->pman->processFor(DISCOVER_PROC)->setWorkingDirectory(QFileInfo(makefile).absolutePath());
        priv->pman->start(DISCOVER_PROC, "make", { "-B", "-p", "-r", "-n", "-f", makefile }, { { "LC_ALL", "C" } });
        priv->makeFile = QFileInfo(makefile);
        emit projectOpened(makefile);
    };

    // FIXME: Unnecesary if force to close project after open other
    if (isProjectOpen()) {
        auto con = connect(this, &ProjectManager::projectClosed, doOpenProject);
        connect(this, &ProjectManager::projectClosed, [con]() { disconnect(con); });
        closeProject();
    } else
        doOpenProject();
}

void ProjectManager::closeProject()
{
    priv->allTargets.clear();
    priv->targets.clear();

    if (priv->targetView->model())
        priv->targetView->model()->deleteLater();
    priv->targetView->setModel(new QStandardItemModel(priv->targetView));

    if (priv->pman->isRunning(DISCOVER_PROC))
        priv->pman->terminate(DISCOVER_PROC, true, 1000);
    priv->makeFile = QFileInfo();
    emit projectClosed();
}

void ProjectManager::reloadProject()
{
    auto project = projectFile();
    closeProject();
    openProject(project);
}