#include "appconfig.h"

#include "passwordpromtdialog.h"

#include <QSettings>
#include <QFontDatabase>
#include <QDir>

#define DEFAULT_PROJECT_PATH_ON_WS "projects"
#define DEFAULT_TEMPLATE_PATH_ON_WS "templates"

#define EDITOR_STYLE "editor/style"
#define EDITOR_FONT_SIZE "editor/font/size"
#define EDITOR_FONT_STYLE "editor/font/style"
#define EDITOR_SAVE_ON_ACTION "editor/saveOnAction"
#define EDITOR_TABS_TO_SPACES "editor/tabsToSpaces"
#define EDITOR_TAB_WIDTH "editor/tabWidth"
#define EDITOR_FORMATTER_NAME "editor/formatter"
#define BUILD_DEFAULT_PROJECT_PATH "build/defaultprojectpath"
#define BUILD_TEMPLATE_PATH "build/templatepath"
#define BUILD_TEMPLATE_URL "build/templateurl"
#define BUILD_ADDITIONAL_PATHS "build/additional_path"
#define NETWORK_PROXY_TYPE "/network/proxy/type"
#define NETWORK_PROXY_HOST "/network/proxy/host"
#define NETWORK_PROXY_PORT "/network/proxy/port"
#define NETWORK_PROXY_CREDENTIALS "/network/proxy/credentials"
#define NETWORK_PROXY_USERNAME "/network/proxy/username"
#define PROJECTTMPLATES_AUTOUPDATE "behavior/projecttmplates/autoupdate"
#define LOGGER_FONT_STYLE "behavior/logger/font/face"
#define LOGGER_FONT_SIZE "behavior/logger/font/size"
#define USE_DEVELOP_MODE "behavior/use_develop"

#if defined(Q_OS_WIN)
# define PREFERRED_FIXED_FONT "Consolas"
#elif defined(Q_OS_LINUX)
# define PREFERRED_FIXED_FONT "Monospace"
#elif defined(Q_OS_MAC)
# define PREFERRED_FIXED_FONT "Monaco"
#else
# define PREFERRED_FIXED_FONT "Courier New"
#endif

static bool isFixedPitch(const QFont & font) {
    const QFontInfo fi(font);
    // qDebug() << fi.family() << fi.fixedPitch();
    return fi.fixedPitch();
}

const QFont AppConfig::systemMonoFont() {
    QFont font(PREFERRED_FIXED_FONT);
    if (isFixedPitch(font))
        return font;
    font.setStyleHint(QFont::Monospace);
    if (isFixedPitch(font))
        return font;
    font.setStyleHint(QFont::TypeWriter);
    if (isFixedPitch(font))
        return font;
    font.setFamily("courier");
    if (isFixedPitch(font))
        return font;
    // qDebug() << font << "fallback";
    return font;
}

struct AppConfigData {
    struct NetworkProxy {
        AppConfig::NetworkProxyType type;
        bool useCredentials;
        QString host;
        QString port;
        QString username;
        QString password;
    } networkProxy;

    QStringList buildAdditionalPaths;
    QString editorStyle;
    QString editorFontStyle;
    QString editorFormatterStyle;
    QString loggerFontStyle;
    QString builDefaultProjectPath;
    QString builTemplatePath;
    QString builTemplateUrl;
    int loggerFontSize;
    int editorFontSize;
    int editorTabWidth;
    bool editorSaveOnAction;
    bool editorTabsToSpaces;
    bool autoUpdateProjectTmplates;
    bool useDevelopMode;
};

AppConfigData* appData()
{
    static AppConfigData appData;
    return &appData;
}

AppConfig& AppConfig::mutableInstance()
{
    static AppConfig appConfig;
    return appConfig;
}

const QString AppConfig::filterTextWithVariables(const QString &text) const
{
    QString result(text);
    for(auto k: filterTextMap.keys())
        result.replace(QString("{{%1}}").arg(k), filterTextMap.value(k)());
    return result;
}

const QHash<QString, QString> AppConfig::getVariableMap() const
{
    QHash<QString, QString> h;
    for(auto k: filterTextMap.keys())
        h.insert(k, filterTextMap[k]());
    return h;
}

const QStringList &AppConfig::buildAdditionalPaths() const
{
    return appData()->buildAdditionalPaths;
}

const QString& AppConfig::editorStyle() const
{
    return appData()->editorStyle;
}

int AppConfig::editorFontSize() const
{
    return appData()->editorFontSize;
}

const QString& AppConfig::editorFontStyle() const
{
    return appData()->editorFontStyle;
}

int AppConfig::loggerFontSize() const
{
    return appData()->loggerFontSize;
}

const QString &AppConfig::loggerFontStyle() const
{
    return appData()->loggerFontStyle;
}

bool AppConfig::editorSaveOnAction() const
{
    return appData()->editorSaveOnAction;
}

bool AppConfig::editorTabsToSpaces() const
{
    return appData()->editorTabsToSpaces;
}

int AppConfig::editorTabWidth() const
{
    return appData()->editorTabWidth;
}

QString AppConfig::editorFormatterStyle() const
{
    return appData()->editorFormatterStyle;
}

const QString& AppConfig::buildDefaultProjectPath() const
{
    return appData()->builDefaultProjectPath;
}

const QString &AppConfig::buildTemplatePath() const
{
    return appData()->builTemplatePath;
}

const QString &AppConfig::buildTemplateUrl() const
{
    return appData()->builTemplateUrl;
}

QString AppConfig::defaultApplicationResources() const
{
    return QDir::home().absoluteFilePath("embedded-ide-workspace");
}

QString AppConfig::defaultProjectPath()
{
    return QDir(defaultApplicationResources()).absoluteFilePath(DEFAULT_PROJECT_PATH_ON_WS);
}

QString AppConfig::defaultTemplatePath()
{
    return QDir(defaultApplicationResources()).absoluteFilePath(DEFAULT_TEMPLATE_PATH_ON_WS);
}

QString AppConfig::defaultTemplateUrl()
{
    return "https://api.github.com/repos/ciaa/EmbeddedIDE-templates/contents";
}

const QString& AppConfig::networkProxyHost() const
{
    return appData()->networkProxy.host;
}

QString AppConfig::networkProxyPort() const
{
    return appData()->networkProxy.port;
}

bool AppConfig::networkProxyUseCredentials() const
{
    return appData()->networkProxy.useCredentials;
}

AppConfig::NetworkProxyType AppConfig::networkProxyType() const
{
    return appData()->networkProxy.type;
}

const QString& AppConfig::networkProxyUsername() const
{
    return appData()->networkProxy.username;
}

const QString& AppConfig::networkProxyPassword() const
{
    return appData()->networkProxy.password;
}

bool AppConfig::projectTmplatesAutoUpdate() const
{
    return appData()->autoUpdateProjectTmplates;
}

bool AppConfig::useDevelopMode() const
{
    return appData()->useDevelopMode;
}

AppConfig::AppConfig()
{
    load();
}

void AppConfig::load()
{
    QSettings s;
    this->setLoggerFontStyle(
                s.value(LOGGER_FONT_STYLE, systemMonoFont()).toString());
    this->setLoggerFontSize(
                s.value(LOGGER_FONT_SIZE, 10).toInt());
    this->setEditorStyle(
                s.value(EDITOR_STYLE, "Default").toString());
    this->setEditorFontSize(
                s.value(EDITOR_FONT_SIZE, 10).toInt());
    this->setEditorFontStyle(
                s.value(EDITOR_FONT_STYLE, systemMonoFont()).toString());
    this->setEditorSaveOnAction(
                s.value(EDITOR_SAVE_ON_ACTION, true).toBool());
    this->setEditorTabsToSpaces(
                s.value(EDITOR_TABS_TO_SPACES, true).toBool());
    this->setEditorTabWidth(
                s.value(EDITOR_TAB_WIDTH, 4).toInt());
    this->setEditorFormatterStyle(
                s.value(EDITOR_FORMATTER_NAME, "linux").toString());
    this->setBuildDefaultProjectPath(
                s.value(BUILD_DEFAULT_PROJECT_PATH, defaultProjectPath()).toString());
    this->setBuildTemplatePath(
                s.value(BUILD_TEMPLATE_PATH, defaultTemplatePath()).toString());
    this->setBuildTemplateUrl(
                s.value(BUILD_TEMPLATE_URL, defaultTemplateUrl()).toString());
    this->setBuildAdditionalPaths(
                s.value(BUILD_ADDITIONAL_PATHS).toStringList());
    this->setNetworkProxyType(
                static_cast<NetworkProxyType>(
                    s.value(NETWORK_PROXY_TYPE, false).toInt()));
    this->setNetworkProxyHost(
                s.value(NETWORK_PROXY_HOST).toString());
    this->setNetworkProxyPort(
                s.value(NETWORK_PROXY_PORT).toString());
    this->setNetworkProxyUseCredentials(
                s.value(NETWORK_PROXY_CREDENTIALS).toBool());
    this->setNetworkProxyUsername(
                s.value(NETWORK_PROXY_USERNAME).toString());
    if (this->networkProxyType() == NetworkProxyType::Custom
            && this->networkProxyUseCredentials()) {
        PasswordPromtDialog paswd(
                    PasswordPromtDialog::tr("Proxy require password"));
        if (paswd.exec() == QDialog::Accepted) {
            this->setNetworkProxyPassword(paswd.password());
        }
    }
    this->setProjectTmplatesAutoUpdate(
                s.value(PROJECTTMPLATES_AUTOUPDATE, true).toBool());
    this->setUseDevelopMode(s.value(USE_DEVELOP_MODE, false).toBool());
    emit configChanged(this);
}

void AppConfig::save()
{
    QSettings s;
    s.setValue(LOGGER_FONT_SIZE, appData()->loggerFontSize);
    s.setValue(LOGGER_FONT_STYLE, appData()->loggerFontStyle);
    s.setValue(EDITOR_STYLE, appData()->editorStyle);
    s.setValue(EDITOR_FONT_SIZE, appData()->editorFontSize);
    s.setValue(EDITOR_FONT_STYLE, appData()->editorFontStyle);
    s.setValue(EDITOR_SAVE_ON_ACTION, appData()->editorSaveOnAction);
    s.setValue(EDITOR_TABS_TO_SPACES, appData()->editorTabsToSpaces);
    s.setValue(EDITOR_TAB_WIDTH, appData()->editorTabWidth);
    s.setValue(EDITOR_FORMATTER_NAME, appData()->editorFormatterStyle);
    s.setValue(BUILD_DEFAULT_PROJECT_PATH, appData()->builDefaultProjectPath);
    s.setValue(BUILD_TEMPLATE_PATH, appData()->builTemplatePath);
    s.setValue(BUILD_TEMPLATE_URL, appData()->builTemplateUrl);
    s.setValue(BUILD_ADDITIONAL_PATHS, appData()->buildAdditionalPaths);
    s.setValue(NETWORK_PROXY_TYPE, static_cast<int>(this->networkProxyType()));
    s.setValue(NETWORK_PROXY_HOST, this->networkProxyHost());
    s.setValue(NETWORK_PROXY_PORT, this->networkProxyPort());
    s.setValue(NETWORK_PROXY_CREDENTIALS, this->networkProxyUseCredentials());
    s.setValue(NETWORK_PROXY_USERNAME, this->networkProxyUsername());
    s.setValue(PROJECTTMPLATES_AUTOUPDATE,
               this->projectTmplatesAutoUpdate());
    s.setValue(USE_DEVELOP_MODE, this->useDevelopMode());
    this->adjustPath();
    emit configChanged(this);
}

void AppConfig::addFilterTextVariable(const QString &key, std::function<QString ()> func)
{
    filterTextMap[key] = func;
}

void AppConfig::setBuildAdditionalPaths(
        const QStringList& buildAdditionalPaths) const
{
    appData()->buildAdditionalPaths = buildAdditionalPaths;
}

void AppConfig::setEditorStyle(const QString &editorStyle)
{
    appData()->editorStyle = editorStyle;
}

void AppConfig::setLoggerFontSize(int loggerFontSize)
{
    appData()->loggerFontSize = loggerFontSize;
}

void AppConfig::setLoggerFontStyle(const QString &loggerFontStyle)
{
    appData()->loggerFontStyle = loggerFontStyle;
}

void AppConfig::setEditorFontSize(int editorFontSize)
{
    appData()->editorFontSize = editorFontSize;
}

void AppConfig::setEditorFontStyle(const QString &editorFontStyle)
{
    appData()->editorFontStyle = editorFontStyle;
}

void AppConfig::setEditorSaveOnAction(bool editorSaveOnAction)
{
    appData()->editorSaveOnAction = editorSaveOnAction;
}

void AppConfig::setEditorTabsToSpaces(bool editorTabsToSpaces)
{
    appData()->editorTabsToSpaces = editorTabsToSpaces;
}

void AppConfig::setEditorTabWidth(int editorTabWidth)
{
    appData()->editorTabWidth = editorTabWidth;
}

void AppConfig::setEditorFormatterStyle(const QString &style)
{
    appData()->editorFormatterStyle = style;
}

void AppConfig::setWorkspacePath(const QString &workspacePath)
{
    QDir wSpace(workspacePath);
    setBuildDefaultProjectPath(wSpace.absoluteFilePath(DEFAULT_PROJECT_PATH_ON_WS));
    setBuildTemplatePath(wSpace.absoluteFilePath(DEFAULT_TEMPLATE_PATH_ON_WS));
}

void AppConfig::setBuildDefaultProjectPath(const QString &builDefaultProjectPath)
{
    appData()->builDefaultProjectPath = builDefaultProjectPath;
    addFilterTextVariable("projectPath", [this]{ return buildDefaultProjectPath(); });
}

void AppConfig::setBuildTemplatePath(const QString &builTemplatePath)
{
    appData()->builTemplatePath = builTemplatePath;
    addFilterTextVariable("templatePath", [this]{ return buildTemplatePath(); });
}

void AppConfig::setBuildTemplateUrl(const QString &builTemplateUrl)
{
    appData()->builTemplateUrl = builTemplateUrl;
}

void AppConfig::setNetworkProxyHost(const QString& host)
{
    appData()->networkProxy.host = host;
}

void AppConfig::setNetworkProxyPort(QString port)
{
    appData()->networkProxy.port = port;
}

void AppConfig::setNetworkProxyUseCredentials(bool useCredentials)
{
    appData()->networkProxy.useCredentials = useCredentials;
}

void AppConfig::setNetworkProxyType(NetworkProxyType type)
{
    appData()->networkProxy.type = type;
}

void AppConfig::setNetworkProxyUsername(const QString& username)
{
    appData()->networkProxy.username = username;
}

void AppConfig::setNetworkProxyPassword(const QString& password)
{
    appData()->networkProxy.password = password;
}

void AppConfig::setProjectTmplatesAutoUpdate(bool automatic)
{
    appData()->autoUpdateProjectTmplates = automatic;
}

void AppConfig::setUseDevelopMode(bool use)
{
    appData()->useDevelopMode = use;
}

void AppConfig::adjustPath()
{
    adjustPath(mutableInstance().buildAdditionalPaths());
}

void AppConfig::adjustPath(const QStringList& paths)
{
    const QChar path_separator
        #ifdef Q_OS_WIN
            (';')
        #else
            (':')
        #endif
            ;
    QString path = qgetenv("PATH");
    QStringList pathList = path.split(path_separator);
#ifdef Q_OS_WIN
    QStringList additional = QStringList(paths).replaceInStrings("/", R"(\)");
#else
    QStringList additional = QStringList(paths);
#endif
    pathList = additional << pathList << QCoreApplication::applicationDirPath();
    path = pathList.join(path_separator);
    qputenv("PATH", path.toLocal8Bit());
}
