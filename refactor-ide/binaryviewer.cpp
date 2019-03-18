#include "binaryviewer.h"

BinaryViewer::BinaryViewer(QWidget *parent) : QHexView(parent)
{
}

bool BinaryViewer::load(const QString &path)
{
    try {
        setData(new DataStorageFile(path));
        setPath(path);
        return true;
    } catch(std::runtime_error) {
        return false;
    }
}

QPoint BinaryViewer::cursor() const
{
    return { cursorPos(), 0 };
}

void BinaryViewer::setCursor(const QPoint &pos)
{
    setCursorPos(pos.x());
}


class BinaryViewerCreator: public IDocumentEditorCreator
{
public:
    ~BinaryViewerCreator() override;
    bool canHandleMime(const QMimeType &mime) const override {
        return mime.inherits("application/octet-stream");
    }

    IDocumentEditor *create(QWidget *parent = nullptr) const override {
        return new BinaryViewer(parent);
    }
};

IDocumentEditorCreator *BinaryViewer::creator()
{
    return IDocumentEditorCreator::staticCreator<BinaryViewerCreator>();
}

BinaryViewerCreator::~BinaryViewerCreator()
= default;
