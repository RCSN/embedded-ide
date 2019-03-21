#include "etags.h"

#include <QFile>
#include <QRegularExpression>
#include <QRegularExpressionMatchIterator>

#include <QtDebug>

static bool findNextEntry(QIODevice *f)
{
    bool founded = false;
    while(!f->atEnd() && !founded) {
        char c;
        qint64 r = f->read(&c, 1);
        if (r == -1)
            break;
        if (r == 1 && c == 0x0C) {
            r = f->read(&c, 1);
            if (c == '\r')
                r = f->read(&c, 1);
            founded = (r == 1) && (c == '\n');
        }
        if (r == 0)
            break;
    }
    return founded;
}

#define PARSE_ERROR qDebug() << __FILE__ << __LINE__;

static QString processFileName(const QString& line, int *len)
{
    QStringList parts = line.split(',');
    if (parts.count() == 2) {
        QString file = parts.at(0);
        bool ok;
        *len = parts.at(1).toInt(&ok, 10);
        if (ok) {
            return file;
        } else
            PARSE_ERROR
    } else
        PARSE_ERROR
    *len = -1;
    return QString();
}

static void parseDefs(const QString& in, const QString& file, ETags::TagMap *map)
{
    QRegularExpression re(R"(([^\x7f]+)\x7f([^\x01]+)\x01(\d+),(\d+)\r?\n)");
    QRegularExpressionMatchIterator it = re.globalMatch(in);
    while(it.hasNext()) {
        QRegularExpressionMatch m = it.next();
        ETags::Tag ctx;
        QString tag = m.captured(2);
        ctx.decl = m.captured(1);
        ctx.file = file;
        ctx.line = m.captured(3).toInt() - 1;
        ctx.offset = m.captured(4).toInt();
        map->insert(tag, ctx);
    }
}

bool ETags::parse(QIODevice *in)
{
    map.clear();
    while (findNextEntry(in)) {
        QByteArray line = in->readLine();
        int len = 0;
        QString file = processFileName(line, &len);
        if (!file.isEmpty() && len != -1) {
            QString defs = in->read(len);
            parseDefs(defs, file, &map);
        }
    }
    qDebug() << "tag entries found" << map.count();
    return true;
}
