/**************************************************************************
**
** This file is part of Qt Creator
**
** Copyright (c) 2009 Nokia Corporation and/or its subsidiary(-ies).
**
** Contact: Nokia Corporation (qt-info@nokia.com)
**
** Commercial Usage
**
** Licensees holding valid Qt Commercial licenses may use this file in
** accordance with the Qt Commercial License Agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Nokia.
**
** GNU Lesser General Public License Usage
**
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** If you are unsure which license is appropriate for your use, please
** contact the sales department at http://qt.nokia.com/contact.
**
**************************************************************************/

#ifndef CUSTOMWIDGETS_H
#define CUSTOMWIDGETS_H

#include "customwidget.h"

#include <utils/newclasswidget.h>
#include <utils/classnamevalidatinglineedit.h>
#include <utils/filenamevalidatinglineedit.h>
#include <utils/linecolumnlabel.h>
#include <utils/pathchooser.h>
#include <utils/projectnamevalidatinglineedit.h>
#include <utils/fancylineedit.h>
#include <utils/qtcolorbutton.h>
#include <utils/submiteditorwidget.h>
#include <utils/submitfieldwidget.h>
#include <utils/pathlisteditor.h>
#include <utils/detailsbutton.h>
#include <utils/detailswidget.h>

#include <QtDesigner/QDesignerCustomWidgetCollectionInterface>
#include <QtDesigner/QDesignerContainerExtension>
#include <QtDesigner/QExtensionFactory>

#include <QtCore/qplugin.h>
#include <QtCore/QList>

QT_BEGIN_NAMESPACE
class QExtensionManager;
QT_END_NAMESPACE

// Custom Widgets

class NewClassCustomWidget :
    public QObject,
    public CustomWidget<Utils::NewClassWidget>
{
    Q_OBJECT
    Q_INTERFACES(QDesignerCustomWidgetInterface)
public:
    explicit NewClassCustomWidget(QObject *parent = 0);
};

class ClassNameValidatingLineEdit_CW :
    public QObject,
    public CustomWidget<Utils::ClassNameValidatingLineEdit>
{
    Q_OBJECT
    Q_INTERFACES(QDesignerCustomWidgetInterface)
public:
    explicit ClassNameValidatingLineEdit_CW(QObject *parent = 0);
};

class FileNameValidatingLineEdit_CW :
    public QObject,
    public CustomWidget<Utils::FileNameValidatingLineEdit>
{
    Q_OBJECT
    Q_INTERFACES(QDesignerCustomWidgetInterface)
public:
    explicit FileNameValidatingLineEdit_CW(QObject *parent = 0);
};

class ProjectNameValidatingLineEdit_CW :
    public QObject,
    public CustomWidget<Utils::ProjectNameValidatingLineEdit>
{
    Q_OBJECT
    Q_INTERFACES(QDesignerCustomWidgetInterface)
public:
    explicit ProjectNameValidatingLineEdit_CW(QObject *parent = 0);
};

class LineColumnLabel_CW :
    public QObject,
    public CustomWidget<Utils::LineColumnLabel>
{
    Q_OBJECT
    Q_INTERFACES(QDesignerCustomWidgetInterface)
public:
    explicit LineColumnLabel_CW(QObject *parent = 0);
};

class PathChooser_CW :
    public QObject,
    public CustomWidget<Utils::PathChooser>
{
    Q_OBJECT
    Q_INTERFACES(QDesignerCustomWidgetInterface)
public:
    explicit PathChooser_CW(QObject *parent = 0);
};

class FancyLineEdit_CW :
    public QObject,
    public CustomWidget<Utils::FancyLineEdit>
{
    Q_OBJECT
    Q_INTERFACES(QDesignerCustomWidgetInterface)
public:
    explicit FancyLineEdit_CW(QObject *parent = 0);

    virtual QWidget *createWidget(QWidget *parent);
};

class QtColorButton_CW :
    public QObject,
    public CustomWidget<Utils::QtColorButton>
{
    Q_OBJECT
    Q_INTERFACES(QDesignerCustomWidgetInterface)
public:
    explicit QtColorButton_CW(QObject *parent = 0);
};

class SubmitEditorWidget_CW :
    public QObject,
    public CustomWidget<Utils::SubmitEditorWidget>
{
    Q_OBJECT
    Q_INTERFACES(QDesignerCustomWidgetInterface)
public:
    explicit SubmitEditorWidget_CW(QObject *parent = 0);
};

class SubmitFieldWidget_CW :
    public QObject,
    public CustomWidget<Utils::SubmitFieldWidget>
{
    Q_OBJECT
    Q_INTERFACES(QDesignerCustomWidgetInterface)
public:
    explicit SubmitFieldWidget_CW(QObject *parent = 0);
};

class PathListEditor_CW :
    public QObject,
    public CustomWidget<Utils::PathListEditor>
{
    Q_OBJECT
    Q_INTERFACES(QDesignerCustomWidgetInterface)
public:
    explicit PathListEditor_CW(QObject *parent = 0);
};

class DetailsButton_CW :
    public QObject,
    public CustomWidget<Utils::DetailsButton>
{
    Q_OBJECT
    Q_INTERFACES(QDesignerCustomWidgetInterface)
public:
    explicit DetailsButton_CW(QObject *parent = 0);
};

// Details Widget: plugin + simple, hacky container extension that
// accepts only one page.

class DetailsWidget_CW :
    public QObject,
    public CustomWidget<Utils::DetailsWidget>
{
    Q_OBJECT
    Q_INTERFACES(QDesignerCustomWidgetInterface)
public:
    explicit DetailsWidget_CW(QObject *parent = 0);
    QString domXml() const;
    void initialize(QDesignerFormEditorInterface *core);
};

class DetailsWidgetContainerExtension: public QObject,
                                         public QDesignerContainerExtension
{
    Q_OBJECT
    Q_INTERFACES(QDesignerContainerExtension)
public:
    explicit DetailsWidgetContainerExtension(Utils::DetailsWidget *widget, QObject *parent);

    void addWidget(QWidget *widget);
    int count() const;
    int currentIndex() const;
    void insertWidget(int index, QWidget *widget);
    void remove(int index);
    void setCurrentIndex(int index);
    QWidget *widget(int index) const;

private:
    Utils::DetailsWidget *m_detailsWidget;
};

class DetailsWidgetExtensionFactory: public QExtensionFactory
{
    Q_OBJECT
public:
    explicit DetailsWidgetExtensionFactory(QExtensionManager *parent = 0);

protected:
    QObject *createExtension(QObject *object, const QString &iid, QObject *parent) const;
};

// ------------ Collection

class WidgetCollection : public QObject, public QDesignerCustomWidgetCollectionInterface
{
    Q_OBJECT
    Q_INTERFACES(QDesignerCustomWidgetCollectionInterface)
public:
    explicit WidgetCollection(QObject *parent = 0);

    virtual QList<QDesignerCustomWidgetInterface*> customWidgets() const;

private:
    QList<QDesignerCustomWidgetInterface*> m_plugins;    
};

#endif // CUSTOMWIDGETS_H