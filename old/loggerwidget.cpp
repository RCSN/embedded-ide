#include "appconfig.h"
#include "loggerwidget.h"

#include <QProcess>
#include <QTextBrowser>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QToolButton>
#include <QRegularExpression>
#include <QUrlQuery>
#include <QTimer>

#include <QtDebug>

#ifdef Q_OS_WIN
#include <windows.h>

#include <tlhelp32.h>

struct ProcessInfo_t {
    long pid;
    long ppid;
    QString comm;
};

QList<ProcessInfo_t> GetRawProcessList() {
    QList<ProcessInfo_t> process_info;
    // Fetch the PID and PPIDs
    PROCESSENTRY32 process_entry;
    HANDLE snapshot_handle = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot_handle == NULL) {
        qDebug() << "CreateToolhelp32Snapshot fail";
        return process_info;
    }
    process_entry.dwSize = sizeof(PROCESSENTRY32);
    if (Process32First(snapshot_handle, &process_entry)) {
        do {
            if (process_entry.th32ProcessID != 0) {
                ProcessInfo_t pi;
                pi.pid = static_cast<long>(process_entry.th32ProcessID);
                pi.ppid = static_cast<long>(process_entry.th32ParentProcessID);
                pi.comm = QString::fromWCharArray(process_entry.szExeFile);
                process_info.append(pi);
            }
        } while (Process32Next(snapshot_handle, &process_entry));
    } else
        qDebug() << "Process32First fail";
    CloseHandle(snapshot_handle);
    return process_info;
}

#elif defined(Q_OS_UNIX)
#include <signal.h>
#endif

struct LoggerWidget::priv_t {
    QProcess *proc;
    QTextBrowser *view;

    void decode(const QString &text, QColor color);

    void addText(const QString &text, QColor color) {
        QTextCursor c = view->textCursor();
        c.movePosition(QTextCursor::End);
        c.insertHtml(QString("<span style=\"color: %2\">%1</span>")
                     .arg(text)
                     .arg(color.name()));
        view->setTextCursor(c);
    }
};

static QString mkUrl(const QString& p, const QString& x, const QString& y) {
    return QString("file:%1?x=%2&y=%3").arg(p).arg(x).arg(y.toInt() - 1);
}

static QString consoleMarkError(const QString& s) {
    QString str(s);
    QRegularExpression re(R"(^(.+?):(\d+):(\d+):(.+?):(.+?)$)");
    re.setPatternOptions(QRegularExpression::MultilineOption);
    QRegularExpressionMatchIterator it = re.globalMatch(s);
    while(it.hasNext()) {
        QRegularExpressionMatch m = it.next();
        QString text = m.captured(0);
        QString path = m.captured(1);
        QString line = m.captured(2);
        QString col = m.captured(3);
        QString url = mkUrl(path, line, col);
        str.replace(text, QString("<a href=\"%1\">%2</a>").arg(url).arg(text));
    }
    return str;
}

static QString consoleToHtml(const QString& s) {
    return consoleMarkError(QString(s)
            .replace("\t", "&nbsp;")
            .replace(" ", "&nbsp;"))
            .replace("\r\n", "<br>")
            .replace("\n", "<br>");
}

LoggerWidget::LoggerWidget(QWidget *parent) :
    QWidget(parent), d_ptr(new LoggerWidget::priv_t)
{
    auto hlayout = new QHBoxLayout();
    auto vlayout = new QVBoxLayout();
    auto clearConsole = new QToolButton(this);
    auto killProc = new QToolButton(this);
    auto view = new QTextBrowser(this);
    auto proc = new QProcess(this);
    QSize iconSize(32, 32);

    clearConsole->setIcon(QIcon("://images/actions/edit-clear.svg"));
    killProc->setIcon(QIcon("://images/actions/media-playback-stop.svg"));
    clearConsole->setIconSize(iconSize);
    killProc->setIconSize(iconSize);
    killProc->setEnabled(false);
    view->setWordWrapMode(QTextOption::NoWrap);

    connect(clearConsole, &QToolButton::clicked, [this]() {
        clearText();
    });
    connect(killProc, &QToolButton::clicked, [this]() {
#ifdef Q_OS_UNIX
        QProcess *psProc = new QProcess(this);
        connect(psProc, static_cast<void (QProcess::*)(int, QProcess::ExitStatus)>(&QProcess::finished),
                [this, psProc](int){
            QList<int> childPid;
            QString out = psProc->readAllStandardOutput();
            QRegularExpression re(R"((\d+)\s+(\d+)\s+(.*))");
            auto mi = re.globalMatch(out);
            while(mi.hasNext()) {
                auto m = mi.next();
                int pid = m.captured(1).toInt();
                int ppid = m.captured(2).toInt();
                QString comm = m.captured(3);
                // qDebug() << "pid" << pid << "ppid" << ppid << comm;
                if (ppid == d_ptr->proc->processId()) {
                    qDebug() << comm << "parent for" << d_ptr->proc->processId();
                    childPid.append(pid);
                } else if (childPid.contains(ppid)) {
                    qDebug() << comm << "child of child";
                    childPid.append(pid);
                }
            }
            qDebug() << "child proc" << childPid;
            d_ptr->proc->terminate();
            for(auto a: childPid)
                kill(a, SIGQUIT);
            QTimer::singleShot(1500, [this, childPid](){
                if (d_ptr->proc->state() == QProcess::Running) {
                    qDebug() << "killing owner";
                    d_ptr->proc->kill();
                    qDebug() << "killing child";
                    for(auto a: childPid)
                        kill(a, SIGKILL);
                }
            });
            psProc->deleteLater();
        });
        psProc->start("ps -axo pid,ppid,comm");
#elif defined(Q_OS_WIN)
        auto list = GetRawProcessList();
        QList<int> childPid;
        for(const auto& pinfo: list) {
            if (pinfo.ppid == d_ptr->proc->processId()) {
                qDebug() << pinfo.comm << "parent for" << d_ptr->proc->processId();
                childPid.append(pinfo.pid);
            } else if (childPid.contains(pinfo.ppid)) {
                qDebug() << pinfo.comm << "child of child";
                childPid.append(pinfo.pid);
            }
        }
        qDebug() << "child proc" << childPid;
        d_ptr->proc->terminate();
        for(auto pid: childPid) {
            auto h = OpenProcess(PROCESS_ALL_ACCESS, false, static_cast<DWORD>(pid));
            TerminateProcess(h, 1);
            CloseHandle(h);
        }
        QTimer::singleShot(1500, [this, childPid](){
            if (d_ptr->proc->state() == QProcess::Running) {
                qDebug() << "killing owner";
                d_ptr->proc->kill();
            }
        });
#else
        d_ptr->proc->terminate();
#endif
#if 0
        QTimer::singleShot(1500, [this](){
            if (d_ptr->proc->state() != QProcess::NotRunning) {
                d_ptr->addText("Killing process...", Qt::red);
                d_ptr->proc->kill();
            }
        });
#endif
    });

    view->setOpenExternalLinks(false);
    view->setOpenLinks(false);
    connect(view, &QTextBrowser::anchorClicked, [this](const QUrl& url) {
        QUrlQuery q(url.query());
        int row = q.queryItemValue("x").toInt();
        int col = q.queryItemValue("y").toInt();
        emit openEditorIn(url.toLocalFile(), col, row);
    });

    hlayout->setContentsMargins(1, 1, 1, 1);
    hlayout->setSpacing(1);
    hlayout->addWidget(view);
    vlayout->addWidget(clearConsole);
    vlayout->addWidget(killProc);
    vlayout->addStretch(100);
    hlayout->addLayout(vlayout);
    setLayout(hlayout);

    LoggerWidget::priv_t *d = d_ptr;
    connect(proc, &QProcess::readyReadStandardOutput, [this] () {
        QByteArray data = d_ptr->proc->readAllStandardOutput();
        d_ptr->decode(QString::fromLocal8Bit(data), Qt::blue);
    });
    connect(proc, &QProcess::readyReadStandardError, [this] () {
        QByteArray data = d_ptr->proc->readAllStandardError();
        d_ptr->decode(QString::fromLocal8Bit(data), Qt::red);
    });
    connect(proc, static_cast<void (QProcess::*)(int, QProcess::ExitStatus)>(&QProcess::finished),
            [this](int exitCode, QProcess::ExitStatus exitStatus) {
        emit processFinished(exitCode, exitStatus);
    });
    connect(proc,
 #if (QT_VERSION >= QT_VERSION_CHECK(5, 6, 0))
        &QProcess::errorOccurred,
#else
        static_cast<void (QProcess::*)(QProcess::ProcessError)>(&QProcess::error),
#endif
                [this](QProcess::ProcessError error) {
        Q_UNUSED(error);
        d_ptr->addText(tr("Process ERROR: %1").arg(d_ptr->proc->errorString()), Qt::darkRed);
    });
    connect(proc, &QProcess::stateChanged, [this, killProc](QProcess::ProcessState state) {
        killProc->setEnabled(state != QProcess::NotRunning);
    });
    auto doConf = [view]() {
        QFont loggerFont(AppConfig::mutableInstance().loggerFontStyle());
        loggerFont.setPixelSize(AppConfig::mutableInstance().loggerFontSize());
        view->document()->setDefaultFont(loggerFont);
    };
    connect(&AppConfig::mutableInstance(), &AppConfig::configChanged, doConf);
    doConf();
    d->proc = proc;
    d->view = view;
}

LoggerWidget::~LoggerWidget()
{
    // FIXME: Prevent non terminated process to emit signals in destructor
    d_ptr->proc->disconnect();
    delete d_ptr;
}

LoggerWidget &LoggerWidget::setWorkingDir(const QString& dir)
{
    d_ptr->proc->setWorkingDirectory(dir);
    return *this;
}

LoggerWidget &LoggerWidget::addEnv(const QStringList &extraEnv)
{
    return setEnv(d_ptr->proc->environment() + extraEnv);
}

LoggerWidget &LoggerWidget::setEnv(const QStringList &env)
{
    d_ptr->proc->setEnvironment(env);
    return *this;
}

void LoggerWidget::clearText()
{
    d_ptr->view->clear();
}

void LoggerWidget::addText(const QString &text, const QColor& color)
{
    d_ptr->addText(consoleToHtml(text), color);
}

bool LoggerWidget::startProcess(const QString &cmd, const QStringList &args)
{
    LoggerWidget::priv_t *d = d_ptr;
    if (d->proc->state() != QProcess::NotRunning)
        return false;
    d->view->clear();
    d->proc->start(cmd, args);
    return true;
}

bool LoggerWidget::startProcess(const QString &command)
{
    LoggerWidget::priv_t *d = d_ptr;
    if (d->proc->state() != QProcess::NotRunning)
        return false;
    d->view->clear();
    d->proc->start(command);
    return true;
}

void LoggerWidget::priv_t::decode(const QString &text, QColor color)
{
    addText(consoleToHtml(text.toHtmlEscaped()), color);
}
