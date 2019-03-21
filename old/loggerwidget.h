#ifndef LOGGERWIDGET_H
#define LOGGERWIDGET_H

#include <QWidget>
#include <QProcess>

class LoggerWidget : public QWidget
{
    Q_OBJECT
public:
    explicit LoggerWidget(QWidget *parent = 0);
    virtual ~LoggerWidget();

signals:
    void openEditorIn(const QString& path, int line, int column);
    void processFinished(int exitCode, QProcess::ExitStatus exitStatus);

public slots:
    bool startProcess(const QString& cmd, const QStringList& args);
    bool startProcess(const QString& command);
    LoggerWidget& setWorkingDir(const QString &dir);
    LoggerWidget& addEnv(const QStringList &extraEnv);
    LoggerWidget& setEnv(const QStringList &env);
    void clearText();
    void addText(const QString& text, const QColor &color);

private:
    struct priv_t;
    priv_t *d_ptr;
};

#endif // LOGGERWIDGET_H
