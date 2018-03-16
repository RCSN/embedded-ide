#ifndef CPPTEXTEDITOR_H
#define CPPTEXTEDITOR_H

#include "codetexteditor.h"

class ICodeModelProvider;

class CPPTextEditor : public CodeTextEditor
{
public:
    explicit CPPTextEditor(QWidget *parent = nullptr);
    virtual ~CPPTextEditor();

    static IDocumentEditorCreator *creator();

private slots:
    void findDeclaration();
    void findDefinition();

protected:
    QMenu *createContextualMenu() override;
    void triggerAutocompletion() override;
    QsciLexer *lexerFromFile(const QString &name) override;

private:
    ICodeModelProvider *provider;
};

#endif // CPPTEXTEDITOR_H