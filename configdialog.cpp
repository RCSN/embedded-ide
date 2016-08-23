#include "configdialog.h"
#include "ui_configdialog.h"

#include <QSettings>
#include <QDir>
#include <QFile>
#include <QDomDocument>
#include <QFileDialog>
#include <QInputDialog>
#include <QStringListModel>
#include <QProcess>
#include <QProcessEnvironment>

#include <QDebug>

#include "qsvsh/qsvcolordef.h"
#include "qsvsh/qsvcolordeffactory.h"
#include "qsvsh/qsvlangdef.h"
#include "qsvsh/qsvsyntaxhighlighter.h"

class QsvCLangDef : public QsvLangDef {
public:
    QsvCLangDef() : QsvLangDef( ":/qsvsh/qtsourceview/data/langs/cpp.lang" ) {}
    virtual ~QsvCLangDef();
};

QsvCLangDef::~QsvCLangDef()
{
}

void adjustPath()
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
    QStringList additional = QSettings().value("build/additional_path").toStringList()
#ifdef Q_OS_WIN
            .replaceInStrings("/", "\\")
#endif
    ;
    pathList = QStringList(additional + pathList).toSet().toList();
    path = pathList.join(path_separator);
    qputenv("PATH", path.toLocal8Bit());
}

static QString getBundledStyles(QComboBox *cb, const QString& defaultName) {
    QDir d(":/qsvsh/qtsourceview/data/colors/");
    QString defaultStyle;
    foreach(QString name, d.entryList(QStringList("*.xml"))) {
        QDomDocument doc("mydocument");
        QFile file(d.filePath(name));
        if (file.open(QIODevice::ReadOnly) && doc.setContent(&file)) {
            QDomNodeList itemDatas = doc.elementsByTagName("itemDatas");
            if (!itemDatas.isEmpty()) {
                QDomNamedNodeMap attr = itemDatas.at(0).attributes();
                QString name = attr.namedItem("name").toAttr().value();
                QString desc = attr.namedItem("description").toAttr().value();
                QString style = name + ": " + desc;
                cb->addItem(style, file.fileName());
                if (defaultName == name)
                    defaultStyle = style;
            }
        }
    }
    return defaultStyle;
}

static QString readBundle(const QString& path) {
    QFile f(path);
    if (f.open(QFile::ReadOnly))
        return QString(f.readAll());
    return QString();
}

QString defaultProjectPath()
{
    return QDir::home().absoluteFilePath(".embedded-ide/projects");
}

ConfigDialog::ConfigDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::ConfigDialog),
    set(new QSettings(this)),
    defColors(0l),
    langCpp(new QsvCLangDef()),
    syntax(0l)
{
    ui->setupUi(this);
    ui->additionalPathList->setModel(new QStringListModel(this));

    load();

    connect(ui->fontComboBox, SIGNAL(activated(int)), this, SLOT(refreshEditor()));
    connect(ui->fontSpinBox, SIGNAL(valueChanged(int)), this, SLOT(refreshEditor()));
    connect(ui->colorStyleComboBox, SIGNAL(activated(int)), this, SLOT(refreshEditor()));

    refreshEditor();
}

ConfigDialog::~ConfigDialog()
{
    delete ui;
    delete langCpp;
    if (defColors)
        delete defColors;
}

void ConfigDialog::load()
{
    ui->plainTextEdit->setPlainText(readBundle(":/help/reference-code-c.txt"));
    QString style = getBundledStyles(ui->colorStyleComboBox, set->value("editor/colorstyle", "Kate").toString());
    if (!style.isEmpty())
        ui->colorStyleComboBox->setCurrentIndex(ui->colorStyleComboBox->findText(style));

    ui->fontSpinBox->setValue(set->value("editor/font/size", 10).toInt());
    ui->fontComboBox->setCurrentFont(QFont(set->value("editor/font/style", "DejaVu Sans Mono").toString()));

    int replaceTabs = set->value("editor/replaceTabs", 0).toInt();
    ui->groupReplaceTabs->setChecked(replaceTabs != 0);
    if (replaceTabs != 0)
        ui->spinReplaceTabs->setValue(replaceTabs);

    ui->projectPath->setText(set->value("build/defaultprojectpath", defaultProjectPath()).toString());

    QStringList additionalPaths = set->value("build/additional_path").toStringList();
    QStringListModel *model = qobject_cast<QStringListModel*>(ui->additionalPathList->model());
    model->setStringList(additionalPaths);
}

void ConfigDialog::save()
{
    set->setValue("editor/colorstyle", ui->colorStyleComboBox->currentText().split(':').at(0));
    set->setValue("editor/font/size", ui->fontSpinBox->value());
    set->setValue("editor/font/style", ui->fontComboBox->currentFont().family());
    set->setValue("editor/replaceTabs", ui->groupReplaceTabs->isChecked()? ui->spinReplaceTabs->value() : 0);
    set->setValue("build/defaultprojectpath", ui->projectPath->text());

    QStringListModel *model = qobject_cast<QStringListModel*>(ui->additionalPathList->model());
    QStringList additionalPaths = model->stringList();
    set->setValue("build/additional_path", additionalPaths);

    adjustPath();
}

void ConfigDialog::on_buttonBox_accepted()
{
    save();
}

void ConfigDialog::refreshEditor()
{
    if (defColors)
        delete defColors;
    if (syntax)
        syntax->deleteLater();
    QString currentStyle = ui->colorStyleComboBox->itemData(ui->colorStyleComboBox->currentIndex()).toString();
    defColors = new QsvColorDefFactory( currentStyle );
    syntax    = new QsvSyntaxHighlighter( ui->plainTextEdit, defColors, langCpp );
    QPalette p = ui->plainTextEdit->palette();
    p.setColor(QPalette::Base, defColors->getColorDef("dsWidgetBackground").getBackground());
    ui->plainTextEdit->setPalette(p);
    QFont font(ui->fontComboBox->currentFont());
    font.setPointSize(ui->fontSpinBox->value());
    ui->plainTextEdit->setFont(font);
}

void ConfigDialog::on_toolButton_clicked()
{
    QString path = QFileDialog::getExistingDirectory(this, tr("Select directory"), QDir::homePath());
    if (!path.isEmpty()) {
        ui->projectPath->setText(path);
    }
}

void ConfigDialog::on_tbPathAdd_clicked()
{
    QString path = QDir::homePath();
    QString dir = QFileDialog::getExistingDirectory(this, tr("Select file"), path);
    if (!dir.isEmpty()) {
        QStringListModel *model = qobject_cast<QStringListModel*>(ui->additionalPathList->model());
        QStringList list = model->stringList();
        list.append(dir);
        model->setStringList(list);
    }
}

void ConfigDialog::on_tbPathRm_clicked()
{
    foreach(QModelIndex idx, ui->additionalPathList->selectionModel()->selectedIndexes()) {
        ui->additionalPathList->model()->removeRow(idx.row());
    }
}