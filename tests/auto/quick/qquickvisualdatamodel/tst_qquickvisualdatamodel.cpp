/****************************************************************************
**
** Copyright (C) 2012 Nokia Corporation and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/
**
** This file is part of the test suite of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** GNU Lesser General Public License Usage
** This file may be used under the terms of the GNU Lesser General Public
** License version 2.1 as published by the Free Software Foundation and
** appearing in the file LICENSE.LGPL included in the packaging of this
** file. Please review the following information to ensure the GNU Lesser
** General Public License version 2.1 requirements will be met:
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Nokia gives you certain additional
** rights. These rights are described in the Nokia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU General
** Public License version 3.0 as published by the Free Software Foundation
** and appearing in the file LICENSE.GPL included in the packaging of this
** file. Please review the following information to ensure the GNU General
** Public License version 3.0 requirements will be met:
** http://www.gnu.org/copyleft/gpl.html.
**
** Other Usage
** Alternatively, this file may be used in accordance with the terms and
** conditions contained in a signed written agreement between you and Nokia.
**
**
**
**
**
**
** $QT_END_LICENSE$
**
****************************************************************************/
#include "../../shared/util.h"
#include "../shared/visualtestutil.h"

#include <qtest.h>
#include <QtTest/QSignalSpy>
#include <QStandardItemModel>
#include <QtQml/qqmlengine.h>
#include <QtQml/qqmlcomponent.h>
#include <QtQml/qqmlcontext.h>
#include <QtQml/qqmlexpression.h>
#include <QtQuick/qquickview.h>
#include <private/qquicklistview_p.h>
#include <QtQuick/private/qquicktext_p.h>
#include <QtQuick/private/qquickvisualdatamodel_p.h>
#include <private/qqmlvaluetype_p.h>
#include <private/qquickchangeset_p.h>
#include <private/qqmlengine_p.h>
#include <math.h>

using namespace QQuickVisualTestUtil;

template <typename T, int N> int lengthOf(const T (&)[N]) { return N; }

static void initStandardTreeModel(QStandardItemModel *model)
{
    QStandardItem *item;
    item = new QStandardItem(QLatin1String("Row 1 Item"));
    model->insertRow(0, item);

    item = new QStandardItem(QLatin1String("Row 2 Item"));
    item->setCheckable(true);
    model->insertRow(1, item);

    QStandardItem *childItem = new QStandardItem(QLatin1String("Row 2 Child Item"));
    item->setChild(0, childItem);

    item = new QStandardItem(QLatin1String("Row 3 Item"));
    item->setIcon(QIcon());
    model->insertRow(2, item);
}

class SingleRoleModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(QStringList values WRITE setList)
public:
    SingleRoleModel(const QByteArray &role = "name", QObject *parent = 0)
        : QAbstractListModel(parent)
    {
        QHash<int, QByteArray> roles;
        roles.insert(Qt::DisplayRole , role);
        setRoleNames(roles);
        list << "one" << "two" << "three" << "four";
    }

    void emitMove(int sourceFirst, int sourceLast, int destinationChild) {
        emit beginMoveRows(QModelIndex(), sourceFirst, sourceLast, QModelIndex(), destinationChild);
        emit endMoveRows();
    }

    QStringList list;

    void setList(const QStringList &l) { list = l; }

public slots:
    void set(int idx, QString string) {
        list[idx] = string;
        emit dataChanged(index(idx,0), index(idx,0));
    }

protected:
    int rowCount(const QModelIndex & /* parent */ = QModelIndex()) const {
        return list.count();
    }
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const {
        if (role == Qt::DisplayRole)
            return list.at(index.row());
        return QVariant();
    }
};

class StandardItem : public QObject, public QStandardItem
{
    Q_OBJECT
    Q_PROPERTY(QString text WRITE setText)

public:
    void writeText(const QString &text) { setText(text); }
};

class StandardItemModel : public QStandardItemModel
{
    Q_OBJECT
    Q_PROPERTY(QQmlListProperty<StandardItem> items READ items CONSTANT)
    Q_CLASSINFO("DefaultProperty", "items")
public:
    QQmlListProperty<StandardItem> items() { return QQmlListProperty<StandardItem>(this, 0, append); }

    static void append(QQmlListProperty<StandardItem> *property, StandardItem *item)
    {
        static_cast<QStandardItemModel *>(property->object)->appendRow(item);
    }
};

class DataObject : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QString name READ name WRITE setName NOTIFY nameChanged)
    Q_PROPERTY(QString color READ color WRITE setColor NOTIFY colorChanged)

public:
    DataObject(QObject *parent=0) : QObject(parent) {}
    DataObject(const QString &name, const QString &color, QObject *parent=0)
        : QObject(parent), m_name(name), m_color(color) { }


    QString name() const { return m_name; }
    void setName(const QString &name) {
        if (name != m_name) {
            m_name = name;
            emit nameChanged();
        }
    }

    QString color() const { return m_color; }
    void setColor(const QString &color) {
        if (color != m_color) {
            m_color = color;
            emit colorChanged();
        }
    }

signals:
    void nameChanged();
    void colorChanged();

private:
    QString m_name;
    QString m_color;
};

QML_DECLARE_TYPE(SingleRoleModel)
QML_DECLARE_TYPE(StandardItem)
QML_DECLARE_TYPE(StandardItemModel)
QML_DECLARE_TYPE(DataObject)

class tst_qquickvisualdatamodel : public QQmlDataTest
{
    Q_OBJECT
public:
    tst_qquickvisualdatamodel();

private slots:
    void initTestCase();
    void cleanupTestCase();
    void rootIndex();
    void updateLayout_data();
    void updateLayout();
    void childChanged_data();
    void childChanged();
    void objectListModel();
    void singleRole();
    void modelProperties();
    void noDelegate_data();
    void noDelegate();
    void itemsDestroyed_data();
    void itemsDestroyed();
    void packagesDestroyed();
    void qaimRowsMoved();
    void qaimRowsMoved_data();
    void remove_data();
    void remove();
    void move_data();
    void move();
    void groups_data();
    void groups();
    void invalidGroups();
    void get();
    void onChanged_data();
    void onChanged();
    void create();
    void incompleteModel();
    void insert_data();
    void insert();
    void resolve_data();
    void resolve();
    void warnings_data();
    void warnings();

private:
    template <int N> void groups_verify(
            const SingleRoleModel &model,
            QQuickItem *contentItem,
            const int (&mIndex)[N],
            const int (&iIndex)[N],
            const int (&vIndex)[N],
            const int (&sIndex)[N],
            const bool (&vMember)[N],
            const bool (&sMember)[N]);

    template <int N> void get_verify(
            const SingleRoleModel &model,
            QQuickVisualDataModel *visualModel,
            QQuickVisualDataGroup *visibleItems,
            QQuickVisualDataGroup *selectedItems,
            const int (&mIndex)[N],
            const int (&iIndex)[N],
            const int (&vIndex)[N],
            const int (&sIndex)[N],
            const bool (&vMember)[N],
            const bool (&sMember)[N]);

    bool failed;
    QQmlEngine engine;
};

Q_DECLARE_METATYPE(QQuickChangeSet)

template <typename T> static T evaluate(QObject *scope, const QString &expression)
{
    QQmlExpression expr(qmlContext(scope), scope, expression);
    T result = expr.evaluate().value<T>();
    if (expr.hasError())
        qWarning() << expr.error().toString();
    return result;
}

template <> void evaluate<void>(QObject *scope, const QString &expression)
{
    QQmlExpression expr(qmlContext(scope), scope, expression);
    expr.evaluate();
    if (expr.hasError())
        qWarning() << expr.error().toString();
}

void tst_qquickvisualdatamodel::initTestCase()
{
    QQmlDataTest::initTestCase();
    qRegisterMetaType<QQuickChangeSet>();

    qmlRegisterType<SingleRoleModel>("tst_qquickvisualdatamodel", 1, 0, "SingleRoleModel");
    qmlRegisterType<StandardItem>("tst_qquickvisualdatamodel", 1, 0, "StandardItem");
    qmlRegisterType<StandardItemModel>("tst_qquickvisualdatamodel", 1, 0, "StandardItemModel");
    qmlRegisterType<DataObject>("tst_qquickvisualdatamodel", 1, 0, "DataObject");
}

void tst_qquickvisualdatamodel::cleanupTestCase()
{

}

tst_qquickvisualdatamodel::tst_qquickvisualdatamodel()
{
}

void tst_qquickvisualdatamodel::rootIndex()
{
    QQmlEngine engine;
    QQmlComponent c(&engine, testFileUrl("visualdatamodel.qml"));

    QStandardItemModel model;
    initStandardTreeModel(&model);

    engine.rootContext()->setContextProperty("myModel", &model);

    QQuickVisualDataModel *obj = qobject_cast<QQuickVisualDataModel*>(c.create());
    QVERIFY(obj != 0);

    QMetaObject::invokeMethod(obj, "setRoot");
    QVERIFY(qvariant_cast<QModelIndex>(obj->rootIndex()) == model.index(0,0));

    QMetaObject::invokeMethod(obj, "setRootToParent");
    QVERIFY(qvariant_cast<QModelIndex>(obj->rootIndex()) == QModelIndex());

    QMetaObject::invokeMethod(obj, "setRoot");
    QVERIFY(qvariant_cast<QModelIndex>(obj->rootIndex()) == model.index(0,0));
    model.clear(); // will emit modelReset()
    QVERIFY(qvariant_cast<QModelIndex>(obj->rootIndex()) == QModelIndex());

    delete obj;
}

void tst_qquickvisualdatamodel::updateLayout_data()
{
    QTest::addColumn<QUrl>("source");

    QTest::newRow("item delegate") << testFileUrl("datalist.qml");
    QTest::newRow("package delegate") << testFileUrl("datalist-package.qml");
}

void tst_qquickvisualdatamodel::updateLayout()
{
    QFETCH(QUrl, source);

    QQuickView view;

    QStandardItemModel model;
    initStandardTreeModel(&model);

    view.rootContext()->setContextProperty("myModel", &model);

    view.setSource(source);

    QQuickListView *listview = qobject_cast<QQuickListView*>(view.rootObject());
    QVERIFY(listview != 0);

    QQuickItem *contentItem = listview->contentItem();
    QVERIFY(contentItem != 0);

    QQuickText *name = findItem<QQuickText>(contentItem, "display", 0);
    QVERIFY(name);
    QCOMPARE(name->text(), QString("Row 1 Item"));
    name = findItem<QQuickText>(contentItem, "display", 1);
    QVERIFY(name);
    QCOMPARE(name->text(), QString("Row 2 Item"));
    name = findItem<QQuickText>(contentItem, "display", 2);
    QVERIFY(name);
    QCOMPARE(name->text(), QString("Row 3 Item"));

    model.invisibleRootItem()->sortChildren(0, Qt::DescendingOrder);

    name = findItem<QQuickText>(contentItem, "display", 0);
    QVERIFY(name);
    QCOMPARE(name->text(), QString("Row 3 Item"));
    name = findItem<QQuickText>(contentItem, "display", 1);
    QVERIFY(name);
    QCOMPARE(name->text(), QString("Row 2 Item"));
    name = findItem<QQuickText>(contentItem, "display", 2);
    QVERIFY(name);
    QCOMPARE(name->text(), QString("Row 1 Item"));
}

void tst_qquickvisualdatamodel::childChanged_data()
{
    QTest::addColumn<QUrl>("source");

    QTest::newRow("item delegate") << testFileUrl("datalist.qml");
    QTest::newRow("package delegate") << testFileUrl("datalist-package.qml");
}

void tst_qquickvisualdatamodel::childChanged()
{
    QFETCH(QUrl, source);

    QQuickView view;

    QStandardItemModel model;
    initStandardTreeModel(&model);

    view.rootContext()->setContextProperty("myModel", &model);

    view.setSource(source);

    QQuickListView *listview = qobject_cast<QQuickListView*>(view.rootObject());
    QVERIFY(listview != 0);

    QQuickItem *contentItem = listview->contentItem();
    QVERIFY(contentItem != 0);

    QQuickVisualDataModel *vdm = listview->findChild<QQuickVisualDataModel*>("visualModel");
    vdm->setRootIndex(QVariant::fromValue(model.indexFromItem(model.item(1,0))));
    QCOMPARE(listview->count(), 1);

    QQuickText *name = findItem<QQuickText>(contentItem, "display", 0);
    QVERIFY(name);
    QCOMPARE(name->text(), QString("Row 2 Child Item"));

    model.item(1,0)->child(0,0)->setText("Row 2 updated child");

    name = findItem<QQuickText>(contentItem, "display", 0);
    QVERIFY(name);
    QCOMPARE(name->text(), QString("Row 2 updated child"));

    model.item(1,0)->appendRow(new QStandardItem(QLatin1String("Row 2 Child Item 2")));
    QCOMPARE(listview->count(), 2);

    name = findItem<QQuickText>(contentItem, "display", 1);
    QVERIFY(name != 0);
    QCOMPARE(name->text(), QString("Row 2 Child Item 2"));

    model.item(1,0)->takeRow(1);
    name = findItem<QQuickText>(contentItem, "display", 1);
    QVERIFY(name == 0);

    vdm->setRootIndex(QVariant::fromValue(QModelIndex()));
    QCOMPARE(listview->count(), 3);
    name = findItem<QQuickText>(contentItem, "display", 0);
    QVERIFY(name);
    QCOMPARE(name->text(), QString("Row 1 Item"));
    name = findItem<QQuickText>(contentItem, "display", 1);
    QVERIFY(name);
    QCOMPARE(name->text(), QString("Row 2 Item"));
    name = findItem<QQuickText>(contentItem, "display", 2);
    QVERIFY(name);
    QCOMPARE(name->text(), QString("Row 3 Item"));
}

void tst_qquickvisualdatamodel::objectListModel()
{
    QQuickView view;

    QList<QObject*> dataList;
    dataList.append(new DataObject("Item 1", "red"));
    dataList.append(new DataObject("Item 2", "green"));
    dataList.append(new DataObject("Item 3", "blue"));
    dataList.append(new DataObject("Item 4", "yellow"));

    QQmlContext *ctxt = view.rootContext();
    ctxt->setContextProperty("myModel", QVariant::fromValue(dataList));

    view.setSource(testFileUrl("objectlist.qml"));

    QQuickListView *listview = qobject_cast<QQuickListView*>(view.rootObject());
    QVERIFY(listview != 0);

    QQuickItem *contentItem = listview->contentItem();
    QVERIFY(contentItem != 0);

    QQuickText *name = findItem<QQuickText>(contentItem, "name", 0);
    QCOMPARE(name->text(), QString("Item 1"));

    QQuickText *section = findItem<QQuickText>(contentItem, "section", 0);
    QCOMPARE(section->text(), QString("Item 1"));

    dataList[0]->setProperty("name", QLatin1String("Changed"));
    QCOMPARE(name->text(), QString("Changed"));
}

void tst_qquickvisualdatamodel::singleRole()
{
    {
        QQuickView view;

        SingleRoleModel model;

        QQmlContext *ctxt = view.rootContext();
        ctxt->setContextProperty("myModel", &model);

        view.setSource(testFileUrl("singlerole1.qml"));

        QQuickListView *listview = qobject_cast<QQuickListView*>(view.rootObject());
        QVERIFY(listview != 0);

        QQuickItem *contentItem = listview->contentItem();
        QVERIFY(contentItem != 0);

        QQuickText *name = findItem<QQuickText>(contentItem, "name", 1);
        QCOMPARE(name->text(), QString("two"));

        model.set(1, "Changed");
        QCOMPARE(name->text(), QString("Changed"));
    }
    {
        QQuickView view;

        SingleRoleModel model;

        QQmlContext *ctxt = view.rootContext();
        ctxt->setContextProperty("myModel", &model);

        view.setSource(testFileUrl("singlerole2.qml"));

        QQuickListView *listview = qobject_cast<QQuickListView*>(view.rootObject());
        QVERIFY(listview != 0);

        QQuickItem *contentItem = listview->contentItem();
        QVERIFY(contentItem != 0);

        QQuickText *name = findItem<QQuickText>(contentItem, "name", 1);
        QCOMPARE(name->text(), QString("two"));

        model.set(1, "Changed");
        QCOMPARE(name->text(), QString("Changed"));
    }
    {
        QQuickView view;

        SingleRoleModel model("modelData");

        QQmlContext *ctxt = view.rootContext();
        ctxt->setContextProperty("myModel", &model);

        view.setSource(testFileUrl("singlerole2.qml"));

        QQuickListView *listview = qobject_cast<QQuickListView*>(view.rootObject());
        QVERIFY(listview != 0);

        QQuickItem *contentItem = listview->contentItem();
        QVERIFY(contentItem != 0);

        QQuickText *name = findItem<QQuickText>(contentItem, "name", 1);
        QCOMPARE(name->text(), QString("two"));

        model.set(1, "Changed");
        QCOMPARE(name->text(), QString("Changed"));
    }
}

void tst_qquickvisualdatamodel::modelProperties()
{
    {
        QQuickView view;

        SingleRoleModel model;

        QQmlContext *ctxt = view.rootContext();
        ctxt->setContextProperty("myModel", &model);

        view.setSource(testFileUrl("modelproperties.qml"));

        QQuickListView *listview = qobject_cast<QQuickListView*>(view.rootObject());
        QVERIFY(listview != 0);

        QQuickItem *contentItem = listview->contentItem();
        QVERIFY(contentItem != 0);

        QQuickItem *delegate = findItem<QQuickItem>(contentItem, "delegate", 1);
        QVERIFY(delegate);
        QCOMPARE(delegate->property("test1").toString(),QString("two"));
        QCOMPARE(delegate->property("test2").toString(),QString("two"));
        QCOMPARE(delegate->property("test3").toString(),QString("two"));
        QCOMPARE(delegate->property("test4").toString(),QString("two"));
        QVERIFY(!delegate->property("test9").isValid());
        QCOMPARE(delegate->property("test5").toString(),QString(""));
        QVERIFY(delegate->property("test6").value<QObject*>() != 0);
        QCOMPARE(delegate->property("test7").toInt(),1);
        QCOMPARE(delegate->property("test8").toInt(),1);
    }

    {
        QQuickView view;

        QList<QObject*> dataList;
        dataList.append(new DataObject("Item 1", "red"));
        dataList.append(new DataObject("Item 2", "green"));
        dataList.append(new DataObject("Item 3", "blue"));
        dataList.append(new DataObject("Item 4", "yellow"));

        QQmlContext *ctxt = view.rootContext();
        ctxt->setContextProperty("myModel", QVariant::fromValue(dataList));

        view.setSource(testFileUrl("modelproperties.qml"));

        QQuickListView *listview = qobject_cast<QQuickListView*>(view.rootObject());
        QVERIFY(listview != 0);

        QQuickItem *contentItem = listview->contentItem();
        QVERIFY(contentItem != 0);

        QQuickItem *delegate = findItem<QQuickItem>(contentItem, "delegate", 1);
        QVERIFY(delegate);
        QCOMPARE(delegate->property("test1").toString(),QString("Item 2"));
        QCOMPARE(delegate->property("test2").toString(),QString("Item 2"));
        QVERIFY(qobject_cast<DataObject*>(delegate->property("test3").value<QObject*>()) != 0);
        QVERIFY(qobject_cast<DataObject*>(delegate->property("test4").value<QObject*>()) != 0);
        QCOMPARE(delegate->property("test5").toString(),QString("Item 2"));
        QCOMPARE(delegate->property("test9").toString(),QString("Item 2"));
        QVERIFY(delegate->property("test6").value<QObject*>() != 0);
        QCOMPARE(delegate->property("test7").toInt(),1);
        QCOMPARE(delegate->property("test8").toInt(),1);
    }

    {
        QQuickView view;

        QStandardItemModel model;
        initStandardTreeModel(&model);

        view.rootContext()->setContextProperty("myModel", &model);

        QUrl source(testFileUrl("modelproperties2.qml"));

        //3 items, 3 i each
        QTest::ignoreMessage(QtWarningMsg, source.toString().toLatin1() + ":13: ReferenceError: Can't find variable: modelData");
        QTest::ignoreMessage(QtWarningMsg, source.toString().toLatin1() + ":13: ReferenceError: Can't find variable: modelData");
        QTest::ignoreMessage(QtWarningMsg, source.toString().toLatin1() + ":13: ReferenceError: Can't find variable: modelData");
        QTest::ignoreMessage(QtWarningMsg, source.toString().toLatin1() + ":11: ReferenceError: Can't find variable: modelData");
        QTest::ignoreMessage(QtWarningMsg, source.toString().toLatin1() + ":11: ReferenceError: Can't find variable: modelData");
        QTest::ignoreMessage(QtWarningMsg, source.toString().toLatin1() + ":11: ReferenceError: Can't find variable: modelData");
        QTest::ignoreMessage(QtWarningMsg, source.toString().toLatin1() + ":17: TypeError: Cannot read property 'display' of undefined");
        QTest::ignoreMessage(QtWarningMsg, source.toString().toLatin1() + ":17: TypeError: Cannot read property 'display' of undefined");
        QTest::ignoreMessage(QtWarningMsg, source.toString().toLatin1() + ":17: TypeError: Cannot read property 'display' of undefined");

        view.setSource(source);

        QQuickListView *listview = qobject_cast<QQuickListView*>(view.rootObject());
        QVERIFY(listview != 0);

        QQuickItem *contentItem = listview->contentItem();
        QVERIFY(contentItem != 0);

        QQuickItem *delegate = findItem<QQuickItem>(contentItem, "delegate", 1);
        QVERIFY(delegate);
        QCOMPARE(delegate->property("test1").toString(),QString("Row 2 Item"));
        QCOMPARE(delegate->property("test2").toString(),QString("Row 2 Item"));
        QVERIFY(!delegate->property("test3").isValid());
        QVERIFY(!delegate->property("test4").isValid());
        QVERIFY(!delegate->property("test5").isValid());
        QVERIFY(!delegate->property("test9").isValid());
        QVERIFY(delegate->property("test6").value<QObject*>() != 0);
        QCOMPARE(delegate->property("test7").toInt(),1);
        QCOMPARE(delegate->property("test8").toInt(),1);
    }

    //### should also test QStringList and QVariantList
}

void tst_qquickvisualdatamodel::noDelegate_data()
{
    QTest::addColumn<QUrl>("source");

    QTest::newRow("item delegate") << testFileUrl("datalist.qml");
    QTest::newRow("package delegate") << testFileUrl("datalist-package.qml");
}

void tst_qquickvisualdatamodel::noDelegate()
{
    QFETCH(QUrl, source);

    QQuickView view;

    QStandardItemModel model;
    initStandardTreeModel(&model);

    view.rootContext()->setContextProperty("myModel", &model);

    view.setSource(source);

    QQuickListView *listview = qobject_cast<QQuickListView*>(view.rootObject());
    QVERIFY(listview != 0);

    QQuickVisualDataModel *vdm = listview->findChild<QQuickVisualDataModel*>("visualModel");
    QVERIFY(vdm != 0);
    QCOMPARE(vdm->count(), 3);

    vdm->setDelegate(0);
    QCOMPARE(vdm->count(), 0);
}

void tst_qquickvisualdatamodel::itemsDestroyed_data()
{
    QTest::addColumn<QUrl>("source");

    QTest::newRow("listView") << testFileUrl("itemsDestroyed_listView.qml");
    QTest::newRow("package") << testFileUrl("itemsDestroyed_package.qml");
    QTest::newRow("pathView") << testFileUrl("itemsDestroyed_pathView.qml");
    QTest::newRow("repeater") << testFileUrl("itemsDestroyed_repeater.qml");
}

void tst_qquickvisualdatamodel::itemsDestroyed()
{
    QFETCH(QUrl, source);

    QQmlGuard<QQuickItem> delegate;

    {
        QQuickView view;
        QStandardItemModel model;
        initStandardTreeModel(&model);
        view.rootContext()->setContextProperty("myModel", &model);
        view.setSource(source);

        view.show();
        QTest::qWaitForWindowShown(&view);

        QVERIFY(delegate = findItem<QQuickItem>(view.rootItem(), "delegate", 1));
    }
    QCoreApplication::sendPostedEvents(0, QEvent::DeferredDelete);
    QVERIFY(!delegate);
}

void tst_qquickvisualdatamodel::packagesDestroyed()
{
    SingleRoleModel model;
    model.list.clear();
    for (int i=0; i<30; i++)
        model.list << ("item " + i);

    QQuickView view;
    view.rootContext()->setContextProperty("testModel", &model);

    QString filename(testFile("packageView.qml"));
    view.setSource(QUrl::fromLocalFile(filename));

    qApp->processEvents();

    QQuickListView *leftview = findItem<QQuickListView>(view.rootObject(), "leftList");
    QTRY_VERIFY(leftview != 0);

    QQuickListView *rightview = findItem<QQuickListView>(view.rootObject(), "rightList");
    QTRY_VERIFY(rightview != 0);

    QQuickItem *leftContent = leftview->contentItem();
    QTRY_VERIFY(leftContent != 0);

    QQuickItem *rightContent = rightview->contentItem();
    QTRY_VERIFY(rightContent != 0);

    QCOMPARE(leftview->currentIndex(), 0);
    QCOMPARE(rightview->currentIndex(), 0);

    rightview->setCurrentIndex(20);
    QTRY_COMPARE(rightview->contentY(), 100.0);

    QQmlGuard<QQuickItem> left;
    QQmlGuard<QQuickItem> right;

    QVERIFY(findItem<QQuickItem>(leftContent, "wrapper", 1));
    QVERIFY(findItem<QQuickItem>(rightContent, "wrapper", 1));

    QVERIFY(left = findItem<QQuickItem>(leftContent, "wrapper", 19));
    QVERIFY(right = findItem<QQuickItem>(rightContent, "wrapper", 19));

    rightview->setCurrentIndex(0);
    QCOMPARE(rightview->currentIndex(), 0);

    QTRY_COMPARE(rightview->contentY(), 0.0);
    QCoreApplication::sendPostedEvents();

    QVERIFY(!left);
    QVERIFY(!right);

    QVERIFY(left = findItem<QQuickItem>(leftContent, "wrapper", 1));
    QVERIFY(right = findItem<QQuickItem>(rightContent, "wrapper", 1));

    rightview->setCurrentIndex(20);
    QTRY_COMPARE(rightview->contentY(), 100.0);

    QVERIFY(left);
    QVERIFY(right);

    QVERIFY(findItem<QQuickItem>(leftContent, "wrapper", 19));
    QVERIFY(findItem<QQuickItem>(rightContent, "wrapper", 19));

    leftview->setCurrentIndex(20);
    QTRY_COMPARE(leftview->contentY(), 100.0);

    QVERIFY(!left);
    QVERIFY(!right);
}

void tst_qquickvisualdatamodel::qaimRowsMoved()
{
    // Test parameters passed in QAIM::rowsMoved() signal are converted correctly
    // when translated and emitted as the QListModelInterface::itemsMoved() signal
    QFETCH(int, sourceFirst);
    QFETCH(int, sourceLast);
    QFETCH(int, destinationChild);
    QFETCH(int, expectFrom);
    QFETCH(int, expectTo);
    QFETCH(int, expectCount);

    QQmlEngine engine;
    QQmlComponent c(&engine, testFileUrl("visualdatamodel.qml"));

    SingleRoleModel model;
    model.list.clear();
    for (int i=0; i<30; i++)
        model.list << ("item " + i);
    engine.rootContext()->setContextProperty("myModel", &model);

    QQuickVisualDataModel *obj = qobject_cast<QQuickVisualDataModel*>(c.create());
    QVERIFY(obj != 0);

    QSignalSpy spy(obj, SIGNAL(modelUpdated(QQuickChangeSet,bool)));
    model.emitMove(sourceFirst, sourceLast, destinationChild);
    QCOMPARE(spy.count(), 1);

    QCOMPARE(spy[0].count(), 2);
    QQuickChangeSet changeSet = spy[0][0].value<QQuickChangeSet>();
    QCOMPARE(changeSet.removes().count(), 1);
    QCOMPARE(changeSet.removes().at(0).index, expectFrom);
    QCOMPARE(changeSet.removes().at(0).count, expectCount);
    QCOMPARE(changeSet.inserts().count(), 1);
    QCOMPARE(changeSet.inserts().at(0).index, expectTo);
    QCOMPARE(changeSet.inserts().at(0).count, expectCount);
    QCOMPARE(changeSet.removes().at(0).moveId, changeSet.inserts().at(0).moveId);
    QCOMPARE(spy[0][1].toBool(), false);

    delete obj;
}

void tst_qquickvisualdatamodel::qaimRowsMoved_data()
{
    QTest::addColumn<int>("sourceFirst");
    QTest::addColumn<int>("sourceLast");
    QTest::addColumn<int>("destinationChild");
    QTest::addColumn<int>("expectFrom");
    QTest::addColumn<int>("expectTo");
    QTest::addColumn<int>("expectCount");

    QTest::newRow("move 1 forward")
        << 1 << 1 << 6
        << 1 << 5 << 1;

    QTest::newRow("move 1 backwards")
        << 4 << 4 << 1
        << 4 << 1 << 1;

    QTest::newRow("move multiple forwards")
        << 0 << 2 << 13
        << 0 << 10 << 3;

    QTest::newRow("move multiple forwards, with same to")
        << 0 << 1 << 3
        << 0 << 1 << 2;

    QTest::newRow("move multiple backwards")
        << 10 << 14 << 1
        << 10 << 1 << 5;
}

void tst_qquickvisualdatamodel::remove_data()
{
    QTest::addColumn<QUrl>("source");
    QTest::addColumn<QString>("package delegate");

    QTest::newRow("item delegate")
            << testFileUrl("groups.qml")
            << QString();
    QTest::newRow("package")
            << testFileUrl("groups-package.qml")
            << QString("package.");
}

void tst_qquickvisualdatamodel::remove()
{
    QQuickView view;

    SingleRoleModel model;
    model.list = QStringList()
            << "one"
            << "two"
            << "three"
            << "four"
            << "five"
            << "six"
            << "seven"
            << "eight"
            << "nine"
            << "ten"
            << "eleven"
            << "twelve";

    QQmlContext *ctxt = view.rootContext();
    ctxt->setContextProperty("myModel", &model);

    view.setSource(testFileUrl("groups.qml"));

    QQuickListView *listview = qobject_cast<QQuickListView*>(view.rootObject());
    QVERIFY(listview != 0);

    QQuickItem *contentItem = listview->contentItem();
    QVERIFY(contentItem != 0);

    QQuickVisualDataModel *visualModel = qobject_cast<QQuickVisualDataModel *>(qvariant_cast<QObject *>(listview->model()));
    QVERIFY(visualModel);

    {
        QCOMPARE(listview->count(), 12);
        QCOMPARE(visualModel->items()->count(), 12);
        static const int mIndex[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11 };
        static const int iIndex[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11 };

        for (int i = 0; i < lengthOf(mIndex); ++i) {
            QQuickItem *delegate = findItem<QQuickItem>(contentItem, "delegate", mIndex[i]);
            QVERIFY(delegate);
            QCOMPARE(delegate->property("test1").toString(), model.list.at(mIndex[i]));
            QCOMPARE(delegate->property("test2").toInt(), mIndex[i]);
            QCOMPARE(delegate->property("test3").toInt(), iIndex[i]);
        }
    } {
        evaluate<void>(visualModel, "items.remove(2)");
        QCOMPARE(listview->count(), 11);
        QCOMPARE(visualModel->items()->count(), 11);
        static const int mIndex[] = { 0, 1, 3, 4, 5, 6, 7, 8, 9,10,11 };
        static const int iIndex[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10 };

        for (int i = 0; i < lengthOf(mIndex); ++i) {
            QQuickItem *delegate = findItem<QQuickItem>(contentItem, "delegate", mIndex[i]);
            QVERIFY(delegate);
            QCOMPARE(delegate->property("test1").toString(), model.list.at(mIndex[i]));
            QCOMPARE(delegate->property("test2").toInt(), mIndex[i]);
            QCOMPARE(delegate->property("test3").toInt(), iIndex[i]);
        }
    } {
        evaluate<void>(visualModel, "items.remove(1, 4)");
        QCOMPARE(listview->count(), 7);
        QCOMPARE(visualModel->items()->count(), 7);
        static const int mIndex[] = { 0, 6, 7, 8, 9,10,11 };
        static const int iIndex[] = { 0, 1, 2, 3, 4, 5, 6 };

        for (int i = 0; i < lengthOf(mIndex); ++i) {
            QQuickItem *delegate = findItem<QQuickItem>(contentItem, "delegate", mIndex[i]);
            QVERIFY(delegate);
            QCOMPARE(delegate->property("test1").toString(), model.list.at(mIndex[i]));
            QCOMPARE(delegate->property("test2").toInt(), mIndex[i]);
            QCOMPARE(delegate->property("test3").toInt(), iIndex[i]);
        }
    } {
        QTest::ignoreMessage(QtWarningMsg, "<Unknown File>: QML VisualDataGroup: remove: index out of range");
        evaluate<void>(visualModel, "items.remove(-8, 4)");
        QCOMPARE(listview->count(), 7);
        QCOMPARE(visualModel->items()->count(), 7);
    } {
        QTest::ignoreMessage(QtWarningMsg, "<Unknown File>: QML VisualDataGroup: remove: index out of range");
        evaluate<void>(visualModel, "items.remove(12, 2)");
        QCOMPARE(listview->count(), 7);
        QCOMPARE(visualModel->items()->count(), 7);
    } {
        QTest::ignoreMessage(QtWarningMsg, "<Unknown File>: QML VisualDataGroup: remove: invalid count");
        evaluate<void>(visualModel, "items.remove(5, 3)");
        QCOMPARE(listview->count(), 7);
        QCOMPARE(visualModel->items()->count(), 7);
    } {
        QTest::ignoreMessage(QtWarningMsg, "<Unknown File>: QML VisualDataGroup: remove: invalid count");
        evaluate<void>(visualModel, "items.remove(5, -2)");
        QCOMPARE(listview->count(), 7);
        QCOMPARE(visualModel->items()->count(), 7);
    }
}

void tst_qquickvisualdatamodel::move_data()
{
    QTest::addColumn<QUrl>("source");
    QTest::addColumn<QString>("package delegate");

    QTest::newRow("item delegate")
            << testFileUrl("groups.qml")
            << QString();
    QTest::newRow("package")
            << testFileUrl("groups-package.qml")
            << QString("package.");
}

void tst_qquickvisualdatamodel::move()
{
    QQuickView view;

    SingleRoleModel model;
    model.list = QStringList()
            << "one"
            << "two"
            << "three"
            << "four"
            << "five"
            << "six"
            << "seven"
            << "eight"
            << "nine"
            << "ten"
            << "eleven"
            << "twelve";

    QQmlContext *ctxt = view.rootContext();
    ctxt->setContextProperty("myModel", &model);

    view.setSource(testFileUrl("groups.qml"));

    QQuickListView *listview = qobject_cast<QQuickListView*>(view.rootObject());
    QVERIFY(listview != 0);

    QQuickItem *contentItem = listview->contentItem();
    QVERIFY(contentItem != 0);

    QQuickVisualDataModel *visualModel = qobject_cast<QQuickVisualDataModel *>(qvariant_cast<QObject *>(listview->model()));
    QVERIFY(visualModel);

    {
        QCOMPARE(listview->count(), 12);
        QCOMPARE(visualModel->items()->count(), 12);
        static const int mIndex[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11 };
        static const int iIndex[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11 };

        for (int i = 0; i < lengthOf(mIndex); ++i) {
            QQuickItem *delegate = findItem<QQuickItem>(contentItem, "delegate", mIndex[i]);
            QVERIFY(delegate);
            QCOMPARE(delegate->property("test1").toString(), model.list.at(mIndex[i]));
            QCOMPARE(delegate->property("test2").toInt(), mIndex[i]);
            QCOMPARE(delegate->property("test3").toInt(), iIndex[i]);
        }
    } {
        evaluate<void>(visualModel, "items.move(2, 4)");
        QCOMPARE(listview->count(), 12);
        QCOMPARE(visualModel->items()->count(), 12);
        static const int mIndex[] = { 0, 1, 3, 4, 2, 5, 6, 7, 8, 9,10,11 };
        static const int iIndex[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11 };

        for (int i = 0; i < lengthOf(mIndex); ++i) {
            QQuickItem *delegate = findItem<QQuickItem>(contentItem, "delegate", mIndex[i]);
            QVERIFY(delegate);
            QCOMPARE(delegate->property("test1").toString(), model.list.at(mIndex[i]));
            QCOMPARE(delegate->property("test2").toInt(), mIndex[i]);
            QCOMPARE(delegate->property("test3").toInt(), iIndex[i]);
        }
    } {
        evaluate<void>(visualModel, "items.move(4, 2)");
        QCOMPARE(listview->count(), 12);
        QCOMPARE(visualModel->items()->count(), 12);
        static const int mIndex[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11 };
        static const int iIndex[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11 };

        for (int i = 0; i < lengthOf(mIndex); ++i) {
            QQuickItem *delegate = findItem<QQuickItem>(contentItem, "delegate", mIndex[i]);
            QVERIFY(delegate);
            QCOMPARE(delegate->property("test1").toString(), model.list.at(mIndex[i]));
            QCOMPARE(delegate->property("test2").toInt(), mIndex[i]);
            QCOMPARE(delegate->property("test3").toInt(), iIndex[i]);
        }
    } {
        evaluate<void>(visualModel, "items.move(8, 0, 4)");
        QCOMPARE(listview->count(), 12);
        QCOMPARE(visualModel->items()->count(), 12);
        static const int mIndex[] = { 8, 9,10,11, 0, 1, 2, 3, 4, 5, 6, 7 };
        static const int iIndex[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11 };

        for (int i = 0; i < lengthOf(mIndex); ++i) {
            QQuickItem *delegate = findItem<QQuickItem>(contentItem, "delegate", mIndex[i]);
            QVERIFY(delegate);
            QCOMPARE(delegate->property("test1").toString(), model.list.at(mIndex[i]));
            QCOMPARE(delegate->property("test2").toInt(), mIndex[i]);
            QCOMPARE(delegate->property("test3").toInt(), iIndex[i]);
        }
    } {
        evaluate<void>(visualModel, "items.move(3, 4, 5)");
        QCOMPARE(listview->count(), 12);
        QCOMPARE(visualModel->items()->count(), 12);
        static const int mIndex[] = { 8, 9,10,4, 11, 0, 1, 2, 3, 5, 6, 7 };
        static const int iIndex[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11 };

        for (int i = 0; i < lengthOf(mIndex); ++i) {
            QQuickItem *delegate = findItem<QQuickItem>(contentItem, "delegate", mIndex[i]);
            QVERIFY(delegate);
            QCOMPARE(delegate->property("test1").toString(), model.list.at(mIndex[i]));
            QCOMPARE(delegate->property("test2").toInt(), mIndex[i]);
            QCOMPARE(delegate->property("test3").toInt(), iIndex[i]);
        }
    } {
        QTest::ignoreMessage(QtWarningMsg, "<Unknown File>: QML VisualDataGroup: move: invalid count");
        evaluate<void>(visualModel, "items.move(5, 2, -2)");
        QCOMPARE(listview->count(), 12);
        QCOMPARE(visualModel->items()->count(), 12);
    } {
        QTest::ignoreMessage(QtWarningMsg, "<Unknown File>: QML VisualDataGroup: move: from index out of range");
        evaluate<void>(visualModel, "items.move(-6, 2, 1)");
        QCOMPARE(listview->count(), 12);
        QCOMPARE(visualModel->items()->count(), 12);
    } {
        QTest::ignoreMessage(QtWarningMsg, "<Unknown File>: QML VisualDataGroup: move: from index out of range");
        evaluate<void>(visualModel, "items.move(15, 2, 1)");
        QCOMPARE(listview->count(), 12);
        QCOMPARE(visualModel->items()->count(), 12);
    } {
        QTest::ignoreMessage(QtWarningMsg, "<Unknown File>: QML VisualDataGroup: move: from index out of range");
        evaluate<void>(visualModel, "items.move(11, 1, 3)");
        QCOMPARE(listview->count(), 12);
        QCOMPARE(visualModel->items()->count(), 12);
    } {
        QTest::ignoreMessage(QtWarningMsg, "<Unknown File>: QML VisualDataGroup: move: to index out of range");
        evaluate<void>(visualModel, "items.move(2, -5, 1)");
        QCOMPARE(listview->count(), 12);
        QCOMPARE(visualModel->items()->count(), 12);
    } {
        QTest::ignoreMessage(QtWarningMsg, "<Unknown File>: QML VisualDataGroup: move: to index out of range");
        evaluate<void>(visualModel, "items.move(2, 14, 1)");
        QCOMPARE(listview->count(), 12);
        QCOMPARE(visualModel->items()->count(), 12);
    } {
        QTest::ignoreMessage(QtWarningMsg, "<Unknown File>: QML VisualDataGroup: move: to index out of range");
        evaluate<void>(visualModel, "items.move(2, 11, 4)");
        QCOMPARE(listview->count(), 12);
        QCOMPARE(visualModel->items()->count(), 12);
    }
}

void tst_qquickvisualdatamodel::groups_data()
{
    QTest::addColumn<QUrl>("source");
    QTest::addColumn<QString>("part");

    QTest::newRow("item delegate")
            << testFileUrl("groups.qml")
            << QString();
    QTest::newRow("package")
            << testFileUrl("groups-package.qml")
            << QString("visualModel.parts.package.");
}

template <int N> void tst_qquickvisualdatamodel::groups_verify(
        const SingleRoleModel &model,
        QQuickItem *contentItem,
        const int (&mIndex)[N],
        const int (&iIndex)[N],
        const int (&vIndex)[N],
        const int (&sIndex)[N],
        const bool (&vMember)[N],
        const bool (&sMember)[N])
{
    failed = true;
    for (int i = 0; i < N; ++i) {
        QQuickItem *delegate = findItem<QQuickItem>(contentItem, "delegate", mIndex[i]);
        QVERIFY(delegate);
        QCOMPARE(evaluate<QString>(delegate, "test1"), model.list.at(mIndex[i]));
        QCOMPARE(evaluate<int>(delegate, "test2") , mIndex[i]);
        QCOMPARE(evaluate<int>(delegate, "test3") , iIndex[i]);
        QCOMPARE(evaluate<bool>(delegate, "test4"), true);
        QCOMPARE(evaluate<int>(delegate, "test5") , vIndex[i]);
        QCOMPARE(evaluate<bool>(delegate, "test6"), vMember[i]);
        QCOMPARE(evaluate<int>(delegate, "test7") , sIndex[i]);
        QCOMPARE(evaluate<bool>(delegate, "test8"), sMember[i]);
        QCOMPARE(evaluate<QStringList>(delegate, "test9").contains("items")   , bool(true));
        QCOMPARE(evaluate<QStringList>(delegate, "test9").contains("visible") , bool(vMember[i]));
        QCOMPARE(evaluate<QStringList>(delegate, "test9").contains("selected"), bool(sMember[i]));
    }
    failed = false;
}

#define VERIFY_GROUPS \
    groups_verify(model, contentItem, mIndex, iIndex, vIndex, sIndex, vMember, sMember); \
    QVERIFY(!failed)


void tst_qquickvisualdatamodel::groups()
{
    QFETCH(QUrl, source);
    QFETCH(QString, part);

    QQuickView view;

    SingleRoleModel model;
    model.list = QStringList()
            << "one"
            << "two"
            << "three"
            << "four"
            << "five"
            << "six"
            << "seven"
            << "eight"
            << "nine"
            << "ten"
            << "eleven"
            << "twelve";

    QQmlContext *ctxt = view.rootContext();
    ctxt->setContextProperty("myModel", &model);

    view.setSource(source);

    QQuickListView *listview = qobject_cast<QQuickListView*>(view.rootObject());
    QVERIFY(listview != 0);

    QQuickItem *contentItem = listview->contentItem();
    QVERIFY(contentItem != 0);

    QQuickVisualDataModel *visualModel = listview->findChild<QQuickVisualDataModel *>("visualModel");
    QVERIFY(visualModel);

    QQuickVisualDataGroup *visibleItems = listview->findChild<QQuickVisualDataGroup *>("visibleItems");
    QVERIFY(visibleItems);

    QQuickVisualDataGroup *selectedItems = listview->findChild<QQuickVisualDataGroup *>("selectedItems");
    QVERIFY(selectedItems);

    const bool f = false;
    const bool t = true;

    {
        QCOMPARE(listview->count(), 12);
        QCOMPARE(visualModel->items()->count(), 12);
        QCOMPARE(visibleItems->count(), 12);
        QCOMPARE(selectedItems->count(), 0);
        static const int  mIndex [] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11 };
        static const int  iIndex [] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11 };
        static const int  vIndex [] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11 };
        static const bool vMember[] = { t, t, t, t, t, t, t, t, t, t, t, t };
        static const int  sIndex [] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
        static const bool sMember[] = { f, f, f, f, f, f, f, f, f, f, f, f };
        VERIFY_GROUPS;
    } {
        evaluate<void>(visualModel, "items.addGroups(8, \"selected\")");
        QCOMPARE(listview->count(), 12);
        QCOMPARE(visualModel->items()->count(), 12);
        QCOMPARE(visibleItems->count(), 12);
        QCOMPARE(selectedItems->count(), 1);
        static const int  mIndex [] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11 };
        static const int  iIndex [] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11 };
        static const int  vIndex [] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11 };
        static const bool vMember[] = { t, t, t, t, t, t, t, t, t, t, t, t };
        static const int  sIndex [] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1 };
        static const bool sMember[] = { f, f, f, f, f, f, f, f, t, f, f, f };
        VERIFY_GROUPS;
    } {
        evaluate<void>(visualModel, "items.addGroups(6, 4, [\"visible\", \"selected\"])");
        QCOMPARE(listview->count(), 12);
        QCOMPARE(visualModel->items()->count(), 12);
        QCOMPARE(visibleItems->count(), 12);
        QCOMPARE(selectedItems->count(), 4);
        static const int  mIndex [] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11 };
        static const int  iIndex [] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11 };
        static const int  vIndex [] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11 };
        static const bool vMember[] = { t, t, t, t, t, t, t, t, t, t, t, t };
        static const int  sIndex [] = { 0, 0, 0, 0, 0, 0, 0, 1, 2, 3, 4, 4 };
        static const bool sMember[] = { f, f, f, f, f, f, t, t, t, t, f, f };
        VERIFY_GROUPS;
    } {
        evaluate<void>(visualModel, "items.setGroups(2, [\"items\", \"selected\"])");
        QCOMPARE(listview->count(), 12);
        QCOMPARE(visualModel->items()->count(), 12);
        QCOMPARE(visibleItems->count(), 11);
        QCOMPARE(selectedItems->count(), 5);
        static const int  mIndex [] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11 };
        static const int  iIndex [] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11 };
        static const int  vIndex [] = { 0, 1, 2, 2, 3, 4, 5, 6, 7, 8, 9,10 };
        static const bool vMember[] = { t, t, f, t, t, t, t, t, t, t, t, t };
        static const int  sIndex [] = { 0, 0, 0, 1, 1, 1, 1, 2, 3, 4, 5, 5 };
        static const bool sMember[] = { f, f, t, f, f, f, t, t, t, t, f, f };
        VERIFY_GROUPS;
    } {
        evaluate<void>(selectedItems, "setGroups(0, 3, \"items\")");
        QCOMPARE(listview->count(), 12);
        QCOMPARE(visualModel->items()->count(), 12);
        QCOMPARE(visibleItems->count(), 9);
        QCOMPARE(selectedItems->count(), 2);
        static const int  mIndex [] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11 };
        static const int  iIndex [] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11 };
        static const int  vIndex [] = { 0, 1, 2, 2, 3, 4, 5, 5, 5, 6, 7, 8 };
        static const bool vMember[] = { t, t, f, t, t, t, f, f, t, t, t, t };
        static const int  sIndex [] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 2, 2 };
        static const bool sMember[] = { f, f, f, f, f, f, f, f, t, t, f, f };
        VERIFY_GROUPS;
    } {
        QTest::ignoreMessage(QtWarningMsg, "<Unknown File>: QML VisualDataGroup: addGroups: invalid count");
        evaluate<void>(visualModel, "items.addGroups(11, -4, \"items\")");
        QCOMPARE(listview->count(), 12);
        QCOMPARE(visualModel->items()->count(), 12);
        QCOMPARE(visibleItems->count(), 9);
        QCOMPARE(selectedItems->count(), 2);
    } {
        QTest::ignoreMessage(QtWarningMsg, "<Unknown File>: QML VisualDataGroup: addGroups: index out of range");
        evaluate<void>(visualModel, "items.addGroups(-1, 3, \"items\")");
        QCOMPARE(listview->count(), 12);
        QCOMPARE(visualModel->items()->count(), 12);
        QCOMPARE(visibleItems->count(), 9);
        QCOMPARE(selectedItems->count(), 2);
    } {
        QTest::ignoreMessage(QtWarningMsg, "<Unknown File>: QML VisualDataGroup: addGroups: index out of range");
        evaluate<void>(visualModel, "items.addGroups(14, 3, \"items\")");
        QCOMPARE(listview->count(), 12);
        QCOMPARE(visualModel->items()->count(), 12);
        QCOMPARE(visibleItems->count(), 9);
        QCOMPARE(selectedItems->count(), 2);
    } {
        QTest::ignoreMessage(QtWarningMsg, "<Unknown File>: QML VisualDataGroup: addGroups: invalid count");
        evaluate<void>(visualModel, "items.addGroups(11, 5, \"items\")");
        QCOMPARE(listview->count(), 12);
        QCOMPARE(visualModel->items()->count(), 12);
        QCOMPARE(visibleItems->count(), 9);
        QCOMPARE(selectedItems->count(), 2);
    } {
        QTest::ignoreMessage(QtWarningMsg, "<Unknown File>: QML VisualDataGroup: setGroups: invalid count");
        evaluate<void>(visualModel, "items.setGroups(11, -4, \"items\")");
        QCOMPARE(listview->count(), 12);
        QCOMPARE(visualModel->items()->count(), 12);
        QCOMPARE(visibleItems->count(), 9);
        QCOMPARE(selectedItems->count(), 2);
    } {
        QTest::ignoreMessage(QtWarningMsg, "<Unknown File>: QML VisualDataGroup: setGroups: index out of range");
        evaluate<void>(visualModel, "items.setGroups(-1, 3, \"items\")");
        QCOMPARE(listview->count(), 12);
        QCOMPARE(visualModel->items()->count(), 12);
        QCOMPARE(visibleItems->count(), 9);
        QCOMPARE(selectedItems->count(), 2);
    } {
        QTest::ignoreMessage(QtWarningMsg, "<Unknown File>: QML VisualDataGroup: setGroups: index out of range");
        evaluate<void>(visualModel, "items.setGroups(14, 3, \"items\")");
        QCOMPARE(listview->count(), 12);
        QCOMPARE(visualModel->items()->count(), 12);
        QCOMPARE(visibleItems->count(), 9);
        QCOMPARE(selectedItems->count(), 2);
    } {
        QTest::ignoreMessage(QtWarningMsg, "<Unknown File>: QML VisualDataGroup: setGroups: invalid count");
        evaluate<void>(visualModel, "items.setGroups(11, 5, \"items\")");
        QCOMPARE(listview->count(), 12);
        QCOMPARE(visualModel->items()->count(), 12);
        QCOMPARE(visibleItems->count(), 9);
        QCOMPARE(selectedItems->count(), 2);
    } {
        QTest::ignoreMessage(QtWarningMsg, "<Unknown File>: QML VisualDataGroup: removeGroups: invalid count");
        evaluate<void>(visualModel, "items.removeGroups(11, -4, \"items\")");
        QCOMPARE(listview->count(), 12);
        QCOMPARE(visualModel->items()->count(), 12);
    } {
        QTest::ignoreMessage(QtWarningMsg, "<Unknown File>: QML VisualDataGroup: removeGroups: index out of range");
        evaluate<void>(visualModel, "items.removeGroups(-1, 3, \"items\")");
        QCOMPARE(listview->count(), 12);
        QCOMPARE(visualModel->items()->count(), 12);
        QCOMPARE(visibleItems->count(), 9);
        QCOMPARE(selectedItems->count(), 2);
    } {
        QTest::ignoreMessage(QtWarningMsg, "<Unknown File>: QML VisualDataGroup: removeGroups: index out of range");
        evaluate<void>(visualModel, "items.removeGroups(14, 3, \"items\")");
        QCOMPARE(listview->count(), 12);
        QCOMPARE(visualModel->items()->count(), 12);
        QCOMPARE(visibleItems->count(), 9);
        QCOMPARE(selectedItems->count(), 2);
    } {
        QTest::ignoreMessage(QtWarningMsg, "<Unknown File>: QML VisualDataGroup: removeGroups: invalid count");
        evaluate<void>(visualModel, "items.removeGroups(11, 5, \"items\")");
        QCOMPARE(listview->count(), 12);
        QCOMPARE(visualModel->items()->count(), 12);
        QCOMPARE(visibleItems->count(), 9);
        QCOMPARE(selectedItems->count(), 2);
    } {
        evaluate<void>(visualModel, part + "filterOnGroup = \"visible\"");
        QCOMPARE(listview->count(), 9);
        QCOMPARE(visualModel->items()->count(), 12);
        QCOMPARE(visibleItems->count(), 9);
        QCOMPARE(selectedItems->count(), 2);
        QCOMPARE(evaluate<QString>(visualModel, part + "filterOnGroup"), QString("visible"));
    } {
        evaluate<void>(visualModel, part + "filterOnGroup = \"selected\"");
        QCOMPARE(listview->count(), 2);
        QCOMPARE(visualModel->items()->count(), 12);
        QCOMPARE(visibleItems->count(), 9);
        QCOMPARE(selectedItems->count(), 2);
        QCOMPARE(evaluate<QString>(visualModel, part + "filterOnGroup"), QString("selected"));
    } {
        evaluate<void>(visualModel, part + "filterOnGroup = undefined");
        QCOMPARE(listview->count(), 12);
        QCOMPARE(visualModel->items()->count(), 12);
        QCOMPARE(visibleItems->count(), 9);
        QCOMPARE(selectedItems->count(), 2);
        QCOMPARE(evaluate<QString>(visualModel, part + "filterOnGroup"), QString("items"));
    } {
        QQuickItem *delegate = findItem<QQuickItem>(contentItem, "delegate", 5);
        QVERIFY(delegate);

        evaluate<void>(delegate, "hide()");
        QCOMPARE(listview->count(), 12);
        QCOMPARE(visualModel->items()->count(), 12);
        QCOMPARE(visibleItems->count(), 8);
        QCOMPARE(selectedItems->count(), 2);
        static const int  mIndex [] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11 };
        static const int  iIndex [] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11 };
        static const int  vIndex [] = { 0, 1, 2, 2, 3, 4, 4, 4, 4, 5, 6, 7 };
        static const bool vMember[] = { t, t, f, t, t, f, f, f, t, t, t, t };
        static const int  sIndex [] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 2, 2 };
        static const bool sMember[] = { f, f, f, f, f, f, f, f, t, t, f, f };
        VERIFY_GROUPS;
    } {
        QQuickItem *delegate = findItem<QQuickItem>(contentItem, "delegate", 5);
        QVERIFY(delegate);

        evaluate<void>(delegate, "select()");
        QCOMPARE(listview->count(), 12);
        QCOMPARE(visualModel->items()->count(), 12);
        QCOMPARE(visibleItems->count(), 8);
        QCOMPARE(selectedItems->count(), 3);
        static const int  mIndex [] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11 };
        static const int  iIndex [] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11 };
        static const int  vIndex [] = { 0, 1, 2, 2, 3, 4, 4, 4, 4, 5, 6, 7 };
        static const bool vMember[] = { t, t, f, t, t, f, f, f, t, t, t, t };
        static const int  sIndex [] = { 0, 0, 0, 0, 0, 0, 1, 1, 1, 2, 3, 3 };
        static const bool sMember[] = { f, f, f, f, f, t, f, f, t, t, f, f };
        VERIFY_GROUPS;
    } {
        evaluate<void>(visualModel, "items.move(2, 6, 3)");
        QCOMPARE(listview->count(), 12);
        QCOMPARE(visualModel->items()->count(), 12);
        QCOMPARE(visibleItems->count(), 8);
        QCOMPARE(selectedItems->count(), 3);
        static const int  mIndex [] = { 0, 1, 5, 6, 7, 8, 2, 3, 4, 9,10,11 };
        static const int  iIndex [] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11 };
        static const int  vIndex [] = { 0, 1, 2, 2, 2, 2, 3, 3, 4, 5, 6, 7 };
        static const bool vMember[] = { t, t, f, f, f, t, f, t, t, t, t, t };
        static const int  sIndex [] = { 0, 0, 0, 1, 1, 1, 2, 2, 2, 2, 3, 3 };
        static const bool sMember[] = { f, f, t, f, f, t, f, f, f, t, f, f };
        VERIFY_GROUPS;
    }
}

template <int N> void tst_qquickvisualdatamodel::get_verify(
        const SingleRoleModel &model,
        QQuickVisualDataModel *visualModel,
        QQuickVisualDataGroup *visibleItems,
        QQuickVisualDataGroup *selectedItems,
        const int (&mIndex)[N],
        const int (&iIndex)[N],
        const int (&vIndex)[N],
        const int (&sIndex)[N],
        const bool (&vMember)[N],
        const bool (&sMember)[N])
{
    failed = true;
    for (int i = 0; i < N; ++i) {
        QCOMPARE(evaluate<QString>(visualModel, QString("items.get(%1).model.name").arg(i)), model.list.at(mIndex[i]));
        QCOMPARE(evaluate<QString>(visualModel, QString("items.get(%1).model.modelData").arg(i)), model.list.at(mIndex[i]));
        QCOMPARE(evaluate<int>(visualModel, QString("items.get(%1).model.index").arg(i)), mIndex[i]);
        QCOMPARE(evaluate<int>(visualModel, QString("items.get(%1).itemsIndex").arg(i)), iIndex[i]);
        QCOMPARE(evaluate<bool>(visualModel, QString("items.get(%1).inItems").arg(i)), true);
        QCOMPARE(evaluate<int>(visualModel, QString("items.get(%1).visibleIndex").arg(i)), vIndex[i]);
        QCOMPARE(evaluate<bool>(visualModel, QString("items.get(%1).inVisible").arg(i)), vMember[i]);
        QCOMPARE(evaluate<int>(visualModel, QString("items.get(%1).selectedIndex").arg(i)), sIndex[i]);
        QCOMPARE(evaluate<bool>(visualModel, QString("items.get(%1).inSelected").arg(i)), sMember[i]);
        QCOMPARE(evaluate<bool>(visualModel, QString("contains(items.get(%1).groups, \"items\")").arg(i)), true);
        QCOMPARE(evaluate<bool>(visualModel, QString("contains(items.get(%1).groups, \"visible\")").arg(i)), vMember[i]);
        QCOMPARE(evaluate<bool>(visualModel, QString("contains(items.get(%1).groups, \"selected\")").arg(i)), sMember[i]);

        if (vMember[i]) {
            QCOMPARE(evaluate<QString>(visibleItems, QString("get(%1).model.name").arg(vIndex[i])), model.list.at(mIndex[i]));
            QCOMPARE(evaluate<QString>(visibleItems, QString("get(%1).model.modelData").arg(vIndex[i])), model.list.at(mIndex[i]));
            QCOMPARE(evaluate<int>(visibleItems, QString("get(%1).model.index").arg(vIndex[i])), mIndex[i]);
            QCOMPARE(evaluate<int>(visibleItems, QString("get(%1).itemsIndex").arg(vIndex[i])), iIndex[i]);
            QCOMPARE(evaluate<bool>(visibleItems, QString("get(%1).inItems").arg(vIndex[i])), true);
            QCOMPARE(evaluate<int>(visibleItems, QString("get(%1).visibleIndex").arg(vIndex[i])), vIndex[i]);
            QCOMPARE(evaluate<bool>(visibleItems, QString("get(%1).inVisible").arg(vIndex[i])), vMember[i]);
            QCOMPARE(evaluate<int>(visibleItems, QString("get(%1).selectedIndex").arg(vIndex[i])), sIndex[i]);
            QCOMPARE(evaluate<bool>(visibleItems, QString("get(%1).inSelected").arg(vIndex[i])), sMember[i]);

            QCOMPARE(evaluate<bool>(visibleItems, QString("contains(get(%1).groups, \"items\")").arg(vIndex[i])), true);
            QCOMPARE(evaluate<bool>(visibleItems, QString("contains(get(%1).groups, \"visible\")").arg(vIndex[i])), vMember[i]);
            QCOMPARE(evaluate<bool>(visibleItems, QString("contains(get(%1).groups, \"selected\")").arg(vIndex[i])), sMember[i]);
        }
        if (sMember[i]) {
            QCOMPARE(evaluate<QString>(selectedItems, QString("get(%1).model.name").arg(sIndex[i])), model.list.at(mIndex[i]));
            QCOMPARE(evaluate<QString>(selectedItems, QString("get(%1).model.modelData").arg(sIndex[i])), model.list.at(mIndex[i]));
            QCOMPARE(evaluate<int>(selectedItems, QString("get(%1).model.index").arg(sIndex[i])), mIndex[i]);
            QCOMPARE(evaluate<int>(selectedItems, QString("get(%1).itemsIndex").arg(sIndex[i])), iIndex[i]);
            QCOMPARE(evaluate<bool>(selectedItems, QString("get(%1).inItems").arg(sIndex[i])), true);
            QCOMPARE(evaluate<int>(selectedItems, QString("get(%1).visibleIndex").arg(sIndex[i])), vIndex[i]);
            QCOMPARE(evaluate<bool>(selectedItems, QString("get(%1).inVisible").arg(sIndex[i])), vMember[i]);
            QCOMPARE(evaluate<int>(selectedItems, QString("get(%1).selectedIndex").arg(sIndex[i])), sIndex[i]);
            QCOMPARE(evaluate<bool>(selectedItems, QString("get(%1).inSelected").arg(sIndex[i])), sMember[i]);
            QCOMPARE(evaluate<bool>(selectedItems, QString("contains(get(%1).groups, \"items\")").arg(sIndex[i])), true);
            QCOMPARE(evaluate<bool>(selectedItems, QString("contains(get(%1).groups, \"visible\")").arg(sIndex[i])), vMember[i]);
            QCOMPARE(evaluate<bool>(selectedItems, QString("contains(get(%1).groups, \"selected\")").arg(sIndex[i])), sMember[i]);
        }
    }
    failed = false;
}

#define VERIFY_GET \
    get_verify(model, visualModel, visibleItems, selectedItems, mIndex, iIndex, vIndex, sIndex, vMember, sMember); \
    QVERIFY(!failed)

void tst_qquickvisualdatamodel::get()
{
    QQuickView view;

    SingleRoleModel model;
    model.list = QStringList()
            << "one"
            << "two"
            << "three"
            << "four"
            << "five"
            << "six"
            << "seven"
            << "eight"
            << "nine"
            << "ten"
            << "eleven"
            << "twelve";

    QQmlContext *ctxt = view.rootContext();
    ctxt->setContextProperty("myModel", &model);

    view.setSource(testFileUrl("groups.qml"));

    QQuickListView *listview = qobject_cast<QQuickListView*>(view.rootObject());
    QVERIFY(listview != 0);

    QQuickItem *contentItem = listview->contentItem();
    QVERIFY(contentItem != 0);

    QQuickVisualDataModel *visualModel = qobject_cast<QQuickVisualDataModel *>(qvariant_cast<QObject *>(listview->model()));
    QVERIFY(visualModel);

    QQuickVisualDataGroup *visibleItems = visualModel->findChild<QQuickVisualDataGroup *>("visibleItems");
    QVERIFY(visibleItems);

    QQuickVisualDataGroup *selectedItems = visualModel->findChild<QQuickVisualDataGroup *>("selectedItems");
    QVERIFY(selectedItems);

    QV8Engine *v8Engine = QQmlEnginePrivate::getV8Engine(ctxt->engine());
    QVERIFY(v8Engine);

    const bool f = false;
    const bool t = true;

    {
        QCOMPARE(listview->count(), 12);
        QCOMPARE(visualModel->items()->count(), 12);
        QCOMPARE(visibleItems->count(), 12);
        QCOMPARE(selectedItems->count(), 0);
        static const int  mIndex [] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11 };
        static const int  iIndex [] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11 };
        static const int  vIndex [] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11 };
        static const bool vMember[] = { t, t, t, t, t, t, t, t, t, t, t, t };
        static const int  sIndex [] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
        static const bool sMember[] = { f, f, f, f, f, f, f, f, f, f, f, f };
        VERIFY_GET;
    } {
        evaluate<void>(visualModel, "items.addGroups(8, \"selected\")");
        QCOMPARE(listview->count(), 12);
        QCOMPARE(visualModel->items()->count(), 12);
        QCOMPARE(visibleItems->count(), 12);
        QCOMPARE(selectedItems->count(), 1);
        static const int  mIndex [] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11 };
        static const int  iIndex [] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11 };
        static const int  vIndex [] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11 };
        static const bool vMember[] = { t, t, t, t, t, t, t, t, t, t, t, t };
        static const int  sIndex [] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1 };
        static const bool sMember[] = { f, f, f, f, f, f, f, f, t, f, f, f };
        VERIFY_GET;
    } {
        evaluate<void>(visualModel, "items.addGroups(6, 4, [\"visible\", \"selected\"])");
        QCOMPARE(listview->count(), 12);
        QCOMPARE(visualModel->items()->count(), 12);
        QCOMPARE(visibleItems->count(), 12);
        QCOMPARE(selectedItems->count(), 4);
        static const int  mIndex [] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11 };
        static const int  iIndex [] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11 };
        static const int  vIndex [] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11 };
        static const bool vMember[] = { t, t, t, t, t, t, t, t, t, t, t, t };
        static const int  sIndex [] = { 0, 0, 0, 0, 0, 0, 0, 1, 2, 3, 4, 4 };
        static const bool sMember[] = { f, f, f, f, f, f, t, t, t, t, f, f };
        VERIFY_GET;
    } {
        evaluate<void>(visualModel, "items.setGroups(2, [\"items\", \"selected\"])");
        QCOMPARE(listview->count(), 12);
        QCOMPARE(visualModel->items()->count(), 12);
        QCOMPARE(visibleItems->count(), 11);
        QCOMPARE(selectedItems->count(), 5);
        static const int  mIndex [] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11 };
        static const int  iIndex [] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11 };
        static const int  vIndex [] = { 0, 1, 2, 2, 3, 4, 5, 6, 7, 8, 9,10 };
        static const bool vMember[] = { t, t, f, t, t, t, t, t, t, t, t, t };
        static const int  sIndex [] = { 0, 0, 0, 1, 1, 1, 1, 2, 3, 4, 5, 5 };
        static const bool sMember[] = { f, f, t, f, f, f, t, t, t, t, f, f };
        VERIFY_GET;
    } {
        evaluate<void>(selectedItems, "setGroups(0, 3, \"items\")");
        QCOMPARE(listview->count(), 12);
        QCOMPARE(visualModel->items()->count(), 12);
        QCOMPARE(visibleItems->count(), 9);
        QCOMPARE(selectedItems->count(), 2);
        static const int  mIndex [] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11 };
        static const int  iIndex [] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11 };
        static const int  vIndex [] = { 0, 1, 2, 2, 3, 4, 5, 5, 5, 6, 7, 8 };
        static const bool vMember[] = { t, t, f, t, t, t, f, f, t, t, t, t };
        static const int  sIndex [] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 2, 2 };
        static const bool sMember[] = { f, f, f, f, f, f, f, f, t, t, f, f };
        VERIFY_GET;
    } {
        evaluate<void>(visualModel, "items.get(5).inVisible = false");
        QCOMPARE(listview->count(), 12);
        QCOMPARE(visualModel->items()->count(), 12);
        QCOMPARE(visibleItems->count(), 8);
        QCOMPARE(selectedItems->count(), 2);
        static const int  mIndex [] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11 };
        static const int  iIndex [] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11 };
        static const int  vIndex [] = { 0, 1, 2, 2, 3, 4, 4, 4, 4, 5, 6, 7 };
        static const bool vMember[] = { t, t, f, t, t, f, f, f, t, t, t, t };
        static const int  sIndex [] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 2, 2 };
        static const bool sMember[] = { f, f, f, f, f, f, f, f, t, t, f, f };
        VERIFY_GET;
    } {
        evaluate<void>(visualModel, "items.get(5).inSelected = true");
        QCOMPARE(listview->count(), 12);
        QCOMPARE(visualModel->items()->count(), 12);
        QCOMPARE(visibleItems->count(), 8);
        QCOMPARE(selectedItems->count(), 3);
        static const int  mIndex [] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11 };
        static const int  iIndex [] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11 };
        static const int  vIndex [] = { 0, 1, 2, 2, 3, 4, 4, 4, 4, 5, 6, 7 };
        static const bool vMember[] = { t, t, f, t, t, f, f, f, t, t, t, t };
        static const int  sIndex [] = { 0, 0, 0, 0, 0, 0, 1, 1, 1, 2, 3, 3 };
        static const bool sMember[] = { f, f, f, f, f, t, f, f, t, t, f, f };
        VERIFY_GET;
    } {
        evaluate<void>(visualModel, "items.get(5).groups = [\"visible\", \"items\"]");
        QCOMPARE(listview->count(), 12);
        QCOMPARE(visualModel->items()->count(), 12);
        QCOMPARE(visibleItems->count(), 9);
        QCOMPARE(selectedItems->count(), 2);
        static const int  mIndex [] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11 };
        static const int  iIndex [] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11 };
        static const int  vIndex [] = { 0, 1, 2, 2, 3, 4, 5, 5, 5, 6, 7, 8 };
        static const bool vMember[] = { t, t, f, t, t, t, f, f, t, t, t, t };
        static const int  sIndex [] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 2, 2 };
        static const bool sMember[] = { f, f, f, f, f, f, f, f, t, t, f, f };
        VERIFY_GET;
    }
}

void tst_qquickvisualdatamodel::invalidGroups()
{
    QUrl source = testFileUrl("groups-invalid.qml");
    QTest::ignoreMessage(QtWarningMsg, (source.toString() + ":12:9: QML VisualDataGroup: " + QQuickVisualDataGroup::tr("Group names must start with a lower case letter")).toUtf8());

    QQmlComponent component(&engine, source);
    QScopedPointer<QObject> object(component.create());
    QVERIFY(object);

    QCOMPARE(evaluate<int>(object.data(), "groups.length"), 4);
    QCOMPARE(evaluate<QString>(object.data(), "groups[0].name"), QString("items"));
    QCOMPARE(evaluate<QString>(object.data(), "groups[1].name"), QString("persistedItems"));
    QCOMPARE(evaluate<QString>(object.data(), "groups[2].name"), QString("visible"));
    QCOMPARE(evaluate<QString>(object.data(), "groups[3].name"), QString("selected"));
}

void tst_qquickvisualdatamodel::onChanged_data()
{
    QTest::addColumn<QString>("expression");
    QTest::addColumn<QStringList>("tests");

    QTest::newRow("item appended")
            << QString("listModel.append({\"number\": \"five\"})")
            << (QStringList()
                << "verify(vm.removed, [], [], [])"
                << "verify(vm.inserted, [4], [1], [undefined])"
                << "verify(vi.removed, [], [], [])"
                << "verify(vi.inserted, [4], [1], [undefined])"
                << "verify(si.removed, [], [], [])"
                << "verify(si.inserted, [], [], [])");
    QTest::newRow("item prepended")
            << QString("listModel.insert(0, {\"number\": \"five\"})")
            << (QStringList()
                << "verify(vm.removed, [], [], [])"
                << "verify(vm.inserted, [0], [1], [undefined])"
                << "verify(vi.removed, [], [], [])"
                << "verify(vi.inserted, [0], [1], [undefined])"
                << "verify(si.removed, [], [], [])"
                << "verify(si.inserted, [], [], [])");
    QTest::newRow("item inserted")
            << QString("listModel.insert(2, {\"number\": \"five\"})")
            << (QStringList()
                << "verify(vm.removed, [], [], [])"
                << "verify(vm.inserted, [2], [1], [undefined])"
                << "verify(vi.removed, [], [], [])"
                << "verify(vi.inserted, [2], [1], [undefined])"
                << "verify(si.removed, [], [], [])"
                << "verify(si.inserted, [], [], [])");

    QTest::newRow("item removed tail")
            << QString("listModel.remove(3)")
            << (QStringList()
                << "verify(vm.removed, [3], [1], [undefined])"
                << "verify(vm.inserted, [], [], [])"
                << "verify(vi.removed, [3], [1], [undefined])"
                << "verify(vi.inserted, [], [], [])"
                << "verify(si.removed, [], [], [])"
                << "verify(si.inserted, [], [], [])");
    QTest::newRow("item removed head")
            << QString("listModel.remove(0)")
            << (QStringList()
                << "verify(vm.removed, [0], [1], [undefined])"
                << "verify(vm.inserted, [], [], [])"
                << "verify(vi.removed, [0], [1], [undefined])"
                << "verify(vi.inserted, [], [], [])"
                << "verify(si.removed, [], [], [])"
                << "verify(si.inserted, [], [], [])");
    QTest::newRow("item removed middle")
            << QString("listModel.remove(1)")
            << (QStringList()
                << "verify(vm.removed, [1], [1], [undefined])"
                << "verify(vm.inserted, [], [], [])"
                << "verify(vi.removed, [1], [1], [undefined])"
                << "verify(vi.inserted, [], [], [])"
                << "verify(si.removed, [], [], [])"
                << "verify(si.inserted, [], [], [])");


    QTest::newRow("item moved from tail")
            << QString("listModel.move(3, 0, 1)")
            << (QStringList()
                << "verify(vm.removed, [3], [1], [vm.inserted[0].moveId])"
                << "verify(vm.inserted, [0], [1], [vm.removed[0].moveId])"
                << "verify(vi.removed, [3], [1], [vi.inserted[0].moveId])"
                << "verify(vi.inserted, [0], [1], [vi.removed[0].moveId])"
                << "verify(si.removed, [], [], [])"
                << "verify(si.inserted, [], [], [])");
    QTest::newRow("item moved from head")
            << QString("listModel.move(0, 2, 2)")
            << (QStringList()
                << "verify(vm.removed, [0], [2], [vm.inserted[0].moveId])"
                << "verify(vm.inserted, [2], [2], [vm.removed[0].moveId])"
                << "verify(vi.removed, [0], [2], [vi.inserted[0].moveId])"
                << "verify(vi.inserted, [2], [2], [vi.removed[0].moveId])"
                << "verify(si.removed, [], [], [])"
                << "verify(si.inserted, [], [], [])");

    QTest::newRow("groups changed")
            << QString("items.setGroups(1, 2, [\"items\", \"selected\"])")
            << (QStringList()
                << "verify(vm.inserted, [], [], [])"
                << "verify(vm.removed, [], [], [])"
                << "verify(vi.removed, [1], [2], [undefined])"
                << "verify(vi.inserted, [], [], [])"
                << "verify(si.removed, [], [], [])"
                << "verify(si.inserted, [0], [2], [undefined])");

    QTest::newRow("multiple removes")
            << QString("{ vi.remove(1, 1); "
                       "vi.removeGroups(0, 2, \"items\") }")
            << (QStringList()
                << "verify(vm.removed, [0, 1], [1, 1], [undefined, undefined])"
                << "verify(vm.inserted, [], [], [])"
                << "verify(vi.removed, [1], [1], [undefined])"
                << "verify(vi.inserted, [], [], [])"
                << "verify(si.removed, [], [], [])"
                << "verify(si.inserted, [], [], [])");
}

void tst_qquickvisualdatamodel::onChanged()
{
    QFETCH(QString, expression);
    QFETCH(QStringList, tests);

    QQmlComponent component(&engine, testFileUrl("onChanged.qml"));
    QScopedPointer<QObject> object(component.create());
    QVERIFY(object);

    evaluate<void>(object.data(), expression);

    foreach (const QString &test, tests) {
        bool passed = evaluate<bool>(object.data(), test);
        if (!passed)
            qWarning() << test;
        QVERIFY(passed);
    }
}

void tst_qquickvisualdatamodel::create()
{
    QQuickView view;

    SingleRoleModel model;
    model.list = QStringList()
            << "one"
            << "two"
            << "three"
            << "four"
            << "five"
            << "six"
            << "seven"
            << "eight"
            << "nine"
            << "ten"
            << "eleven"
            << "twelve"
            << "thirteen"
            << "fourteen"
            << "fifteen"
            << "sixteen"
            << "seventeen"
            << "eighteen"
            << "nineteen"
            << "twenty";

    QQmlContext *ctxt = view.rootContext();
    ctxt->setContextProperty("myModel", &model);

    view.setSource(testFileUrl("create.qml"));

    QQuickListView *listview = qobject_cast<QQuickListView*>(view.rootObject());
    QVERIFY(listview != 0);

    QQuickItem *contentItem = listview->contentItem();
    QVERIFY(contentItem != 0);

    QQuickVisualDataModel *visualModel = qobject_cast<QQuickVisualDataModel *>(qvariant_cast<QObject *>(listview->model()));
    QVERIFY(visualModel);

    QCOMPARE(listview->count(), 20);

    QQmlGuard<QQuickItem> delegate;

    // persistedItems.includeByDefault is true, so all items belong to persistedItems initially.
    QVERIFY(delegate = findItem<QQuickItem>(contentItem, "delegate", 1));
    QCOMPARE(evaluate<bool>(delegate, "VisualDataModel.inPersistedItems"), true);

    // changing include by default doesn't remove persistance.
    evaluate<void>(visualModel, "persistedItems.includeByDefault = false");
    QCOMPARE(evaluate<bool>(delegate, "VisualDataModel.inPersistedItems"), true);

    // removing from persistedItems does.
    evaluate<void>(visualModel, "persistedItems.remove(0, 20)");
    QCOMPARE(listview->count(), 20);
    QCOMPARE(evaluate<bool>(delegate, "VisualDataModel.inPersistedItems"), false);

    // Request an item instantiated by the view.
    QVERIFY(delegate = qobject_cast<QQuickItem *>(evaluate<QObject *>(visualModel, "items.create(1)")));
    QCOMPARE(delegate.data(), findItem<QQuickItem>(contentItem, "delegate", 1));
    QCOMPARE(evaluate<bool>(delegate, "VisualDataModel.inPersistedItems"), true);
    QCOMPARE(evaluate<int>(visualModel, "persistedItems.count"), 1);

    evaluate<void>(delegate, "VisualDataModel.inPersistedItems = false");
    QCOMPARE(listview->count(), 20);
    QCoreApplication::sendPostedEvents(delegate, QEvent::DeferredDelete);
    QVERIFY(delegate);
    QCOMPARE(evaluate<bool>(delegate, "VisualDataModel.inPersistedItems"), false);
    QCOMPARE(evaluate<int>(visualModel, "persistedItems.count"), 0);

    // Request an item not instantiated by the view.
    QVERIFY(!findItem<QQuickItem>(contentItem, "delegate", 15));
    QVERIFY(delegate = qobject_cast<QQuickItem *>(evaluate<QObject *>(visualModel, "items.create(15)")));
    QCOMPARE(delegate.data(), findItem<QQuickItem>(contentItem, "delegate", 15));
    QCOMPARE(evaluate<bool>(delegate, "VisualDataModel.inPersistedItems"), true);
    QCOMPARE(evaluate<int>(visualModel, "persistedItems.count"), 1);

    evaluate<void>(visualModel, "persistedItems.remove(0)");
    QCoreApplication::sendPostedEvents(delegate, QEvent::DeferredDelete);
    QVERIFY(!delegate);
    QCOMPARE(evaluate<int>(visualModel, "persistedItems.count"), 0);

    // Request an item not instantiated by the view, then scroll the view so it will request it.
    QVERIFY(!findItem<QQuickItem>(contentItem, "delegate", 16));
    QVERIFY(delegate = qobject_cast<QQuickItem *>(evaluate<QObject *>(visualModel, "items.create(16)")));
    QCOMPARE(delegate.data(), findItem<QQuickItem>(contentItem, "delegate", 16));
    QCOMPARE(evaluate<bool>(delegate, "VisualDataModel.inPersistedItems"), true);
    QCOMPARE(evaluate<int>(visualModel, "persistedItems.count"), 1);

    evaluate<void>(listview, "positionViewAtIndex(19, ListView.End)");
    QCOMPARE(listview->count(), 20);
    evaluate<void>(delegate, "VisualDataModel.groups = [\"items\"]");
    QCoreApplication::sendPostedEvents(delegate, QEvent::DeferredDelete);
    QVERIFY(delegate);
    QCOMPARE(evaluate<bool>(delegate, "VisualDataModel.inPersistedItems"), false);
    QCOMPARE(evaluate<int>(visualModel, "persistedItems.count"), 0);

    // Request and release an item instantiated by the view, then scroll the view so it releases it.
    QVERIFY(findItem<QQuickItem>(contentItem, "delegate", 17));
    QVERIFY(delegate = qobject_cast<QQuickItem *>(evaluate<QObject *>(visualModel, "items.create(17)")));
    QCOMPARE(delegate.data(), findItem<QQuickItem>(contentItem, "delegate", 17));
    QCOMPARE(evaluate<bool>(delegate, "VisualDataModel.inPersistedItems"), true);
    QCOMPARE(evaluate<int>(visualModel, "persistedItems.count"), 1);

    evaluate<void>(visualModel, "items.removeGroups(17, \"persistedItems\")");
    QCoreApplication::sendPostedEvents(delegate, QEvent::DeferredDelete);
    QVERIFY(delegate);
    QCOMPARE(evaluate<bool>(delegate, "VisualDataModel.inPersistedItems"), false);
    QCOMPARE(evaluate<int>(visualModel, "persistedItems.count"), 0);
    evaluate<void>(listview, "positionViewAtIndex(1, ListView.Beginning)");
    QCOMPARE(listview->count(), 20);
    QCoreApplication::sendPostedEvents(delegate, QEvent::DeferredDelete);
    QVERIFY(!delegate);

    // Adding an item to the persistedItems group won't instantiate it, but if later requested by
    // the view it will be persisted.
    evaluate<void>(visualModel, "items.addGroups(18, \"persistedItems\")");
    QCOMPARE(evaluate<int>(visualModel, "persistedItems.count"), 1);
    QVERIFY(!findItem<QQuickItem>(contentItem, "delegate", 18));
    evaluate<void>(listview, "positionViewAtIndex(19, ListView.End)");
    QCOMPARE(listview->count(), 20);
    QVERIFY(delegate = findItem<QQuickItem>(contentItem, "delegate", 18));
    QCOMPARE(evaluate<bool>(delegate, "VisualDataModel.inPersistedItems"), true);
    QCoreApplication::sendPostedEvents(delegate, QEvent::DeferredDelete);
    QVERIFY(delegate);
    evaluate<void>(listview, "positionViewAtIndex(1, ListView.Beginning)");
    QCOMPARE(listview->count(), 20);
    QCoreApplication::sendPostedEvents(delegate, QEvent::DeferredDelete);
    QVERIFY(delegate);

    // Remove an uninstantiated but cached item from the persistedItems group.
    evaluate<void>(visualModel, "items.addGroups(19, \"persistedItems\")");
    QCOMPARE(evaluate<int>(visualModel, "persistedItems.count"), 2);
    QVERIFY(!findItem<QQuickItem>(contentItem, "delegate", 19));
     // Store a reference to the item so it is retained in the cache.
    evaluate<void>(visualModel, "persistentHandle = items.get(19)");
    QCOMPARE(evaluate<bool>(visualModel, "persistentHandle.inPersistedItems"), true);
    evaluate<void>(visualModel, "items.removeGroups(19, \"persistedItems\")");
    QCOMPARE(evaluate<bool>(visualModel, "persistentHandle.inPersistedItems"), false);
}

void tst_qquickvisualdatamodel::incompleteModel()
{
    // VisualDataModel is first populated in componentComplete.  Verify various functions are
    // harmlessly ignored until then.

    QQmlComponent component(&engine);
    component.setData("import QtQuick 2.0\n VisualDataModel {}", testFileUrl(""));

    QScopedPointer<QObject> object(component.beginCreate(engine.rootContext()));

    QQuickVisualDataModel *model = qobject_cast<QQuickVisualDataModel *>(object.data());
    QVERIFY(model);

    QSignalSpy itemsSpy(model->items(), SIGNAL(countChanged()));
    QSignalSpy persistedItemsSpy(model->items(), SIGNAL(countChanged()));

    evaluate<void>(model, "items.removeGroups(0, items.count, \"items\")");
    QCOMPARE(itemsSpy.count(), 0);
    QCOMPARE(persistedItemsSpy.count(), 0);

    evaluate<void>(model, "items.setGroups(0, items.count, \"persistedItems\")");
    QCOMPARE(itemsSpy.count(), 0);
    QCOMPARE(persistedItemsSpy.count(), 0);

    evaluate<void>(model, "items.addGroups(0, items.count, \"persistedItems\")");
    QCOMPARE(itemsSpy.count(), 0);
    QCOMPARE(persistedItemsSpy.count(), 0);

    evaluate<void>(model, "items.remove(0, items.count)");
    QCOMPARE(itemsSpy.count(), 0);
    QCOMPARE(persistedItemsSpy.count(), 0);

    evaluate<void>(model, "items.insert([ \"color\": \"blue\" ])");
    QCOMPARE(itemsSpy.count(), 0);
    QCOMPARE(persistedItemsSpy.count(), 0);

    QTest::ignoreMessage(QtWarningMsg, "<Unknown File>: QML VisualDataGroup: get: index out of range");
    QVERIFY(evaluate<bool>(model, "items.get(0) === undefined"));

    component.completeCreate();
}

void tst_qquickvisualdatamodel::insert_data()
{
    QTest::addColumn<QUrl>("source");
    QTest::addColumn<QString>("expression");
    QTest::addColumn<int>("modelCount");
    QTest::addColumn<int>("visualCount");
    QTest::addColumn<int>("index");
    QTest::addColumn<bool>("inItems");
    QTest::addColumn<bool>("persisted");
    QTest::addColumn<bool>("visible");
    QTest::addColumn<bool>("selected");
    QTest::addColumn<bool>("modelData");
    QTest::addColumn<QString>("property");
    QTest::addColumn<QStringList>("propertyData");

    const QUrl listModelSource[] = {
        testFileUrl("listmodelproperties.qml"),
        testFileUrl("listmodelproperties-package.qml") };
    const QUrl singleRoleSource[] = {
        testFileUrl("singleroleproperties.qml"),
        testFileUrl("singleroleproperties-package.qml") };
    const QUrl multipleRoleSource[] = {
        testFileUrl("multipleroleproperties.qml"),
        testFileUrl("multipleroleproperties-package.qml") };
    const QUrl stringListSource[] = {
        testFileUrl("stringlistproperties.qml"),
        testFileUrl("stringlistproperties-package.qml") };
    const QUrl objectListSource[] = {
        testFileUrl("objectlistproperties.qml"),
        testFileUrl("objectlistproperties-package.qml") };

    for (int i = 0; i < 2; ++i) {
        // List Model.
        QTest::newRow("ListModel.items prepend")
                << listModelSource[i]
                << QString("items.insert(0, {\"number\": \"eight\"})")
                << 4 << 5 << 0 << true << false << false << false << true
                << QString("number")
                << (QStringList() << "eight" << "one" << "two" << "three" << "four");

        QTest::newRow("ListModel.items append")
                << listModelSource[i]
                << QString("items.insert({\"number\": \"eight\"})")
                << 4 << 5 << 4 << true << false << false << false << true
                << QString("number")
                << (QStringList() << "one" << "two" << "three" << "four" << "eight");

        QTest::newRow("ListModel.items insert at 2")
                << listModelSource[i]
                << QString("items.insert(2, {\"number\": \"eight\"})")
                << 4 << 5 << 2 << true << false << false << false << true
                << QString("number")
                << (QStringList() << "one" << "two" << "eight" << "three" << "four");

        QTest::newRow("ListModel.items insert at items.get(2)")
                << listModelSource[i]
                << QString("items.insert(items.get(2), {\"number\": \"eight\"})")
                << 4 << 5 << 2 << true << false << false << false << true
                << QString("number")
                << (QStringList() << "one" << "two" << "eight" << "three" << "four");

        QTest::newRow("ListModel.items insert at visibleItems.get(2)")
                << listModelSource[i]
                << QString("items.insert(visibleItems.get(2), {\"number\": \"eight\"})")
                << 4 << 5 << 2 << true << false << false << false << true
                << QString("number")
                << (QStringList() << "one" << "two" << "eight" << "three" << "four");

        QTest::newRow("ListModel.selectedItems insert at items.get(2)")
                << listModelSource[i]
                << QString("selectedItems.insert(items.get(2), {\"number\": \"eight\"})")
                << 4 << 5 << 2 << false << false << false << true << true
                << QString("number")
                << (QStringList() << "one" << "two" << "eight" << "three" << "four");

        QTest::newRow("ListModel.selectedItems insert at visibleItems.get(2)")
                << listModelSource[i]
                << QString("selectedItems.insert(visibleItems.get(2), {\"number\": \"eight\"})")
                << 4 << 5 << 2 << false << false << false << true << true
                << QString("number")
                << (QStringList() << "one" << "two" << "eight" << "three" << "four");

        QTest::newRow("ListModel.items prepend modelData")
                << listModelSource[i]
                << QString("items.insert(0, {\"modelData\": \"eight\"})")
                << 4 << 5 << 0 << true << false << false << false << true
                << QString("number")
                << (QStringList() << "eight" << "one" << "two" << "three" << "four");

        QTest::newRow("ListModel.items prepend, edit number")
                << listModelSource[i]
                << QString("{ "
                       "items.insert(0, {\"number\": \"eight\"}); "
                       "items.get(0).model.number = \"seven\"; }")
                << 4 << 5 << 0 << true << false << false << false << true
                << QString("number")
                << (QStringList() << "seven" << "one" << "two" << "three" << "four");

        QTest::newRow("ListModel.items prepend, edit modelData")
                << listModelSource[i]
                << QString("{ "
                       "items.insert(0, {\"number\": \"eight\"}); "
                       "items.get(0).model.modelData = \"seven\"; }")
                << 4 << 5 << 0 << true << false << false << false << true
                << QString("number")
                << (QStringList() << "seven" << "one" << "two" << "three" << "four");

        QTest::newRow("ListModel.items prepend, edit resolved")
                << listModelSource[i]
                << QString("{ "
                       "items.insert(0, {\"number\": \"eight\"}); "
                       "items.get(2).model.number = \"seven\"; }")
                << 4 << 5 << 0 << true << false << false << false << true
                << QString("number")
                << (QStringList() << "eight" << "one" << "two" << "three" << "four");

        QTest::newRow("ListModel.items prepend with groups")
                << listModelSource[i]
                << QString("items.insert(0, {\"number\": \"eight\"}, [\"visible\", \"truncheon\"])")
                << 4 << 5 << 0 << true << false << true << false << true
                << QString("number")
                << (QStringList() << "eight" << "one" << "two" << "three" << "four");

        QTest::newRow("ListModel.items append with groups")
                << listModelSource[i]
                << QString("items.insert({\"number\": \"eight\"}, [\"visible\", \"selected\"])")
                << 4 << 5 << 4 << true << false << true << true << true
                << QString("number")
                << (QStringList() << "one" << "two" << "three" << "four" << "eight");

        QTest::newRow("ListModel.items insert at 2 with groups")
                << listModelSource[i]
                << QString("items.insert(2, {\"number\": \"eight\"}, \"visible\")")
                << 4 << 5 << 2 << true << false << true << false << true
                << QString("number")
                << (QStringList() << "one" << "two" << "eight" << "three" << "four");

        // create ListModel
        QTest::newRow("ListModel.items prepend")
                << listModelSource[i]
                << QString("items.create(0, {\"number\": \"eight\"})")
                << 4 << 5 << 0 << true << true << false << false << true
                << QString("number")
                << (QStringList() << "eight" << "one" << "two" << "three" << "four");

        QTest::newRow("ListModel.items append")
                << listModelSource[i]
                << QString("items.create({\"number\": \"eight\"})")
                << 4 << 5 << 4 << true << true << false << false << true
                << QString("number")
                << (QStringList() << "one" << "two" << "three" << "four" << "eight");

        QTest::newRow("ListModel.items create at 2")
                << listModelSource[i]
                << QString("items.create(2, {\"number\": \"eight\"})")
                << 4 << 5 << 2 << true << true << false << false << true
                << QString("number")
                << (QStringList() << "one" << "two" << "eight" << "three" << "four");

        QTest::newRow("ListModel.items create at items.get(2)")
                << listModelSource[i]
                << QString("items.create(items.get(2), {\"number\": \"eight\"})")
                << 4 << 5 << 2 << true << true << false << false << true
                << QString("number")
                << (QStringList() << "one" << "two" << "eight" << "three" << "four");

        QTest::newRow("ListModel.items create at visibleItems.get(2)")
                << listModelSource[i]
                << QString("items.create(visibleItems.get(2), {\"number\": \"eight\"})")
                << 4 << 5 << 2 << true << true << false << false << true
                << QString("number")
                << (QStringList() << "one" << "two" << "eight" << "three" << "four");

        QTest::newRow("ListModel.selectedItems create at items.get(2)")
                << listModelSource[i]
                << QString("selectedItems.create(items.get(2), {\"number\": \"eight\"})")
                << 4 << 5 << 2 << false << true << false << true << true
                << QString("number")
                << (QStringList() << "one" << "two" << "eight" << "three" << "four");

        QTest::newRow("ListModel.selectedItems create at visibleItems.get(2)")
                << listModelSource[i]
                << QString("selectedItems.create(visibleItems.get(2), {\"number\": \"eight\"})")
                << 4 << 5 << 2 << false << true << false << true << true
                << QString("number")
                << (QStringList() << "one" << "two" << "eight" << "three" << "four");

        QTest::newRow("ListModel.items create prepended")
                << listModelSource[i]
                << QString("items.create(0, {\"number\": \"eight\"})")
                << 4 << 5 << 0 << true << true << false << false << true
                << QString("number")
                << (QStringList() << "eight" << "one" << "two" << "three" << "four");

        QTest::newRow("ListModel.items create appended")
                << listModelSource[i]
                << QString("items.create({\"number\": \"eight\"})")
                << 4 << 5 << 4 << true << true << false << false << true
                << QString("number")
                << (QStringList() << "one" << "two" << "three" << "four" << "eight");

        QTest::newRow("ListModel.items create at 2")
                << listModelSource[i]
                << QString("items.create(2, {\"number\": \"eight\"})")
                << 4 << 5 << 2 << true << true << false << false << true
                << QString("number")
                << (QStringList() << "one" << "two" << "eight" << "three" << "four");

        QTest::newRow("ListModel.items create at items.get(2)")
                << listModelSource[i]
                << QString("items.create(items.get(2), {\"number\": \"eight\"})")
                << 4 << 5 << 2 << true << true << false << false << true
                << QString("number")
                << (QStringList() << "one" << "two" << "eight" << "three" << "four");

        QTest::newRow("ListModel.items create at visibleItems.get(2)")
                << listModelSource[i]
                << QString("items.create(visibleItems.get(2), {\"number\": \"eight\"})")
                << 4 << 5 << 2 << true << true << false << false << true
                << QString("number")
                << (QStringList() << "one" << "two" << "eight" << "three" << "four");

        QTest::newRow("ListModel.create prepend modelData")
                << listModelSource[i]
                << QString("items.create(0, {\"modelData\": \"eight\"})")
                << 4 << 5 << 0 << true << true << false << false << true
                << QString("number")
                << (QStringList() << "eight" << "one" << "two" << "three" << "four");

        QTest::newRow("ListModel.items create prepended, edit number")
                << listModelSource[i]
                << QString("{ "
                       "var item = items.create(0, {\"number\": \"eight\"}); "
                       "item.setTest3(\"seven\"); }")
                << 4 << 5 << 0 << true << true << false << false << true
                << QString("number")
                << (QStringList() << "seven" << "one" << "two" << "three" << "four");

        QTest::newRow("ListModel.items create prepended, edit model.number")
                << listModelSource[i]
                << QString("{ "
                       "var item = items.create(0, {\"number\": \"eight\"}); "
                       "item.setTest4(\"seven\"); }")
                << 4 << 5 << 0 << true << true << false << false << true
                << QString("number")
                << (QStringList() << "seven" << "one" << "two" << "three" << "four");

        QTest::newRow("ListModel.items create prepended, edit modelData")
                << listModelSource[i]
                << QString("{ "
                       "var item = items.create(0, {\"number\": \"eight\"}); "
                       "item.setTest5(\"seven\"); }")
                << 4 << 5 << 0 << true << true << false << false << true
                << QString("number")
                << (QStringList() << "seven" << "one" << "two" << "three" << "four");

        QTest::newRow("ListModel.items create prepended, edit model.modelData")
                << listModelSource[i]
                << QString("{ "
                       "var item = items.create(0, {\"number\": \"eight\"}); "
                       "item.setTest6(\"seven\"); }")
                << 4 << 5 << 0 << true << true << false << false << true
                << QString("number")
                << (QStringList() << "seven" << "one" << "two" << "three" << "four");

        QTest::newRow("ListModel.items create prepended with groups")
                << listModelSource[i]
                << QString("items.create(0, {\"number\": \"eight\"}, [\"visible\", \"truncheon\"])")
                << 4 << 5 << 0 << true << true << true << false << true
                << QString("number")
                << (QStringList() << "eight" << "one" << "two" << "three" << "four");

        QTest::newRow("ListModel.items create appended with groups")
                << listModelSource[i]
                << QString("items.create({\"number\": \"eight\"}, [\"visible\", \"selected\"])")
                << 4 << 5 << 4 << true << true << true << true << true
                << QString("number")
                << (QStringList() << "one" << "two" << "three" << "four" << "eight");

        QTest::newRow("ListModel.items create inserted  with groups")
                << listModelSource[i]
                << QString("items.create(2, {\"number\": \"eight\"}, \"visible\")")
                << 4 << 5 << 2 << true << true << true << false << true
                << QString("number")
                << (QStringList() << "one" << "two" << "eight" << "three" << "four");

        QTest::newRow("ListModel.items create prepended clear persistence")
                << listModelSource[i]
                << QString("{ items.create(0, {\"number\": \"eight\"}); "
                           "items.get(0).inPersistedItems = false }")
                << 4 << 5 << 0 << true << false << false << false << true
                << QString("number")
                << (QStringList() << "eight" << "one" << "two" << "three" << "four");

        QTest::newRow("ListModel.items create appended clear persistence")
                << listModelSource[i]
                << QString("{ items.create({\"number\": \"eight\"}); "
                           "items.get(4).inPersistedItems = false }")
                << 4 << 5 << 4 << true << false << false << false << true
                << QString("number")
                << (QStringList() << "one" << "two" << "three" << "four" << "eight");

        QTest::newRow("ListModel.items create inserted clear persistence")
                << listModelSource[i]
                << QString("{ items.create(2, {\"number\": \"eight\"}); "
                           "items.get(2).inPersistedItems = false }")
                << 4 << 5 << 2 << true << false << false << false << true
                << QString("number")
                << (QStringList() << "one" << "two" << "eight" << "three" << "four");

        // AbstractItemModel (Single Role).
        QTest::newRow("AbstractItemModel.items prepend")
                << singleRoleSource[i]
                << QString("items.insert(0, {\"name\": \"eight\"})")
                << 4 << 5 << 0 << true << false << false << false << true
                << QString("name")
                << (QStringList() << "eight" << "one" << "two" << "three" << "four");

        QTest::newRow("AbstractItemModel.items append")
                << singleRoleSource[i]
                << QString("items.insert({\"name\": \"eight\"})")
                << 4 << 5 << 4 << true << false << false << false << true
                << QString("name")
                << (QStringList() << "one" << "two" << "three" << "four" << "eight");

        QTest::newRow("AbstractItemModel.items insert at 2")
                << singleRoleSource[i]
                << QString("items.insert(2, {\"name\": \"eight\"})")
                << 4 << 5 << 2 << true << false << false << false << true
                << QString("name")
                << (QStringList() << "one" << "two" << "eight" << "three" << "four");

        QTest::newRow("AbstractItemModel.items prepend modelData")
                << singleRoleSource[i]
                << QString("items.insert(0, {\"modelData\": \"eight\"})")
                << 4 << 5 << 0 << true << false << false << false << true
                << QString("name")
                << (QStringList() << "eight" << "one" << "two" << "three" << "four");

        QTest::newRow("AbstractItemModel.items prepend, edit name")
                << singleRoleSource[i]
                << QString("{ "
                       "items.insert(0, {\"name\": \"eight\"}); "
                       "items.get(0).model.name = \"seven\"; }")
                << 4 << 5 << 0 << true << false << false << false << true
                << QString("name")
                << (QStringList() << "seven" << "one" << "two" << "three" << "four");

        QTest::newRow("AbstractItemModel.items prepend, edit modelData")
                << singleRoleSource[i]
                << QString("{ "
                       "items.insert(0, {\"name\": \"eight\"}); "
                       "items.get(0).model.modelData = \"seven\"; }")
                << 4 << 5 << 0 << true << false << false << false << true
                << QString("name")
                << (QStringList() << "seven" << "one" << "two" << "three" << "four");

        QTest::newRow("AbstractItemModel.items prepend, edit resolved")
                << singleRoleSource[i]
                << QString("{ "
                       "items.insert(0, {\"name\": \"eight\"}); "
                       "items.get(2).model.name = \"seven\"; }")
                << 4 << 5 << 0 << true << false << false << false << true
                << QString("name")
                << (QStringList() << "eight" << "one" << "two" << "three" << "four");

        QTest::newRow("AbstractItemModel.create prepend modelData")
                << singleRoleSource[i]
                << QString("items.create(0, {\"modelData\": \"eight\"})")
                << 4 << 5 << 0 << true << true << false << false << true
                << QString("name")
                << (QStringList() << "eight" << "one" << "two" << "three" << "four");

        QTest::newRow("AbstractItemModel.items create prepended, edit name")
                << singleRoleSource[i]
                << QString("{ "
                       "var item = items.create(0, {\"name\": \"eight\"}); "
                       "item.setTest3(\"seven\"); }")
                << 4 << 5 << 0 << true << true << false << false << true
                << QString("name")
                << (QStringList() << "seven" << "one" << "two" << "three" << "four");

        QTest::newRow("AbstractItemModel.items create prepended, edit model.name")
                << singleRoleSource[i]
                << QString("{ "
                       "var item = items.create(0, {\"name\": \"eight\"}); "
                       "item.setTest4(\"seven\"); }")
                << 4 << 5 << 0 << true << true << false << false << true
                << QString("name")
                << (QStringList() << "seven" << "one" << "two" << "three" << "four");

        QTest::newRow("AbstractItemModel.items create prepended, edit modelData")
                << singleRoleSource[i]
                << QString("{ "
                       "var item = items.create(0, {\"name\": \"eight\"}); "
                       "item.setTest5(\"seven\"); }")
                << 4 << 5 << 0 << true << true << false << false << true
                << QString("name")
                << (QStringList() << "seven" << "one" << "two" << "three" << "four");

        QTest::newRow("AbstractItemModel.items create prepended, edit model.modelData")
                << singleRoleSource[i]
                << QString("{ "
                       "var item = items.create(0, {\"name\": \"eight\"}); "
                       "item.setTest6(\"seven\"); }")
                << 4 << 5 << 0 << true << true << false << false << true
                << QString("name")
                << (QStringList() << "seven" << "one" << "two" << "three" << "four");

        // AbstractItemModel (Multiple Roles).
        QTest::newRow("StandardItemModel.items prepend")
                << multipleRoleSource[i]
                << QString("items.insert(0, {\"display\": \"Row 8 Item\"})")
                << 4 << 5 << 0 << true << false << false << false << false
                << QString("display")
                << (QStringList() << "Row 8 Item" << "Row 1 Item" << "Row 2 Item" << "Row 3 Item" << "Row 4 Item");

        QTest::newRow("StandardItemModel.items append")
                << multipleRoleSource[i]
                << QString("items.insert({\"display\": \"Row 8 Item\"})")
                << 4 << 5 << 4 << true << false << false << false << false
                << QString("display")
                << (QStringList() << "Row 1 Item" << "Row 2 Item" << "Row 3 Item" << "Row 4 Item" << "Row 8 Item");

        QTest::newRow("StandardItemModel.items insert at 2")
                << multipleRoleSource[i]
                << QString("items.insert(2, {\"display\": \"Row 8 Item\"})")
                << 4 << 5 << 2 << true << false << false << false << false
                << QString("display")
                << (QStringList() << "Row 1 Item" << "Row 2 Item" << "Row 8 Item" << "Row 3 Item" << "Row 4 Item");

        QTest::newRow("StandardItemModel.items prepend modelData")
                << multipleRoleSource[i]
                << QString("items.insert(0, {\"modelData\": \"Row 8 Item\"})")
                << 4 << 5 << 0 << true << false << false << false << false
                << QString("display")
                << (QStringList() << QString() << "Row 1 Item" << "Row 2 Item" << "Row 3 Item" << "Row 4 Item");

        QTest::newRow("StandardItemModel.items prepend, edit display")
                << multipleRoleSource[i]
                << QString("{ "
                       "items.insert(0, {\"display\": \"Row 8 Item\"}); "
                       "items.get(0).model.display = \"Row 7 Item\"; }")
                << 4 << 5 << 0 << true << false << false << false << false
                << QString("display")
                << (QStringList() << "Row 7 Item" << "Row 1 Item" << "Row 2 Item" << "Row 3 Item" << "Row 4 Item");

        QTest::newRow("StandardItemModel.items prepend, edit modelData")
                << multipleRoleSource[i]
                << QString("{ "
                       "items.insert(0, {\"display\": \"Row 8 Item\"}); "
                       "items.get(0).model.modelData = \"Row 7 Item\"; }")
                << 4 << 5 << 0 << true << false << false << false << false
                << QString("display")
                << (QStringList() << "Row 8 Item" << "Row 1 Item" << "Row 2 Item" << "Row 3 Item" << "Row 4 Item");

        QTest::newRow("StandardItemModel.items prepend, edit resolved")
                << multipleRoleSource[i]
                << QString("{ "
                       "items.insert(0, {\"display\": \"Row 8 Item\"}); "
                       "items.get(2).model.display = \"Row 7 Item\"; }")
                << 4 << 5 << 0 << true << false << false << false << false
                << QString("display")
                << (QStringList() << "Row 8 Item" << "Row 1 Item" << "Row 2 Item" << "Row 3 Item" << "Row 4 Item");

        QTest::newRow("StandardItemModel.create prepend modelData")
                << multipleRoleSource[i]
                << QString("items.create(0, {\"modelData\": \"Row 8 Item\"})")
                << 4 << 5 << 0 << true << true << false << false << false
                << QString("display")
                << (QStringList() << QString() << "Row 1 Item" << "Row 2 Item" << "Row 3 Item" << "Row 4 Item");

        QTest::newRow("StandardItemModel.items create prepended, edit display")
                << multipleRoleSource[i]
                << QString("{ "
                       "var item = items.create(0, {\"display\": \"Row 8 Item\"}); "
                       "item.setTest3(\"Row 7 Item\"); }")
                << 4 << 5 << 0 << true << true << false << false << false
                << QString("display")
                << (QStringList() << "Row 7 Item" << "Row 1 Item" << "Row 2 Item" << "Row 3 Item" << "Row 4 Item");

        QTest::newRow("StandardItemModel.items create prepended, edit model.display")
                << multipleRoleSource[i]
                << QString("{ "
                       "var item = items.create(0, {\"display\": \"Row 8 Item\"}); "
                       "item.setTest4(\"Row 7 Item\"); }")
                << 4 << 5 << 0 << true << true << false << false << false
                << QString("display")
                << (QStringList() << "Row 7 Item" << "Row 1 Item" << "Row 2 Item" << "Row 3 Item" << "Row 4 Item");

        // StringList.
        QTest::newRow("StringList.items prepend")
                << stringListSource[i]
                << QString("items.insert(0, {\"modelData\": \"eight\"})")
                << 4 << 5 << 0 << true << false << false << false << false
                << QString("modelData")
                << (QStringList() << "eight" << "one" << "two" << "three" << "four");

        QTest::newRow("StringList.items append")
                << stringListSource[i]
                << QString("items.insert({\"modelData\": \"eight\"})")
                << 4 << 5 << 4 << true << false << false << false << false
                << QString("modelData")
                << (QStringList() << "one" << "two" << "three" << "four" << "eight");

        QTest::newRow("StringList.items insert at 2")
                << stringListSource[i]
                << QString("items.insert(2, {\"modelData\": \"eight\"})")
                << 4 << 5 << 2 << true << false << false << false << false
                << QString("modelData")
                << (QStringList() << "one" << "two" << "eight" << "three" << "four");

        QTest::newRow("StringList.items prepend, edit modelData")
                << stringListSource[i]
                << QString("{ "
                       "items.insert(0, {\"modelData\": \"eight\"}); "
                       "items.get(0).model.modelData = \"seven\"; }")
                << 4 << 5 << 0 << true << false << false << false << false
                << QString("modelData")
                << (QStringList() << "seven" << "one" << "two" << "three" << "four");

        QTest::newRow("StringList.items prepend, edit resolved")
                << stringListSource[i]
                << QString("{ "
                       "items.insert(0, {\"modelData\": \"eight\"}); "
                       "items.get(2).model.modelData = \"seven\"; }")
                << 4 << 5 << 0 << true << false << false << false << false
                << QString("modelData")
                << (QStringList() << "eight" << "one" << "two" << "three" << "four");

        QTest::newRow("StringList.create prepend modelData")
                << stringListSource[i]
                << QString("items.create(0, {\"modelData\": \"eight\"})")
                << 4 << 5 << 0 << true << true << false << false << false
                << QString("modelData")
                << (QStringList() << "eight" << "one" << "two" << "three" << "four");

        QTest::newRow("StringList.items create prepended, edit modelData")
                << stringListSource[i]
                << QString("{ "
                       "var item = items.create(0, {\"modelData\": \"eight\"}); "
                       "item.setTest3(\"seven\"); }")
                << 4 << 5 << 0 << true << true << false << false << false
                << QString("modelData")
                << (QStringList() << "seven" << "one" << "two" << "three" << "four");

        QTest::newRow("StringList.items create prepended, edit model.modelData")
                << stringListSource[i]
                << QString("{ "
                       "var item = items.create(0, {\"modelData\": \"eight\"}); "
                       "item.setTest4(\"seven\"); }")
                << 4 << 5 << 0 << true << true << false << false << false
                << QString("modelData")
                << (QStringList() << "seven" << "one" << "two" << "three" << "four");

        // ObjectList
        QTest::newRow("ObjectList.items prepend")
                << objectListSource[i]
                << QString("items.insert(0, {\"name\": \"Item 8\"})")
                << 4 << 4 << 4 << false << false << false << false << false
                << QString("name")
                << (QStringList() << "Item 1" << "Item 2" << "Item 3" << "Item 4");

        QTest::newRow("ObjectList.items append")
                << objectListSource[i]
                << QString("items.insert({\"name\": \"Item 8\"})")
                << 4 << 4 << 4 << false << false << false << false << false
                << QString("name")
                << (QStringList() << "Item 1" << "Item 2" << "Item 3" << "Item 4");

        QTest::newRow("ObjectList.items insert at 2")
                << objectListSource[i]
                << QString("items.insert(2, {\"name\": \"Item 8\"})")
                << 4 << 4 << 4 << false << false << false << false << false
                << QString("name")
                << (QStringList() << "Item 1" << "Item 2" << "Item 3" << "Item 4");
    }
}

void tst_qquickvisualdatamodel::insert()
{
    QFETCH(QUrl, source);
    QFETCH(QString, expression);
    QFETCH(int, modelCount);
    QFETCH(int, visualCount);
    QFETCH(int, index);
    QFETCH(bool, inItems);
    QFETCH(bool, persisted);
    QFETCH(bool, visible);
    QFETCH(bool, selected);
    QFETCH(bool, modelData);
    QFETCH(QString, property);
    QFETCH(QStringList, propertyData);

    QQuickCanvas canvas;

    QQmlComponent component(&engine);
    component.loadUrl(source);
    QScopedPointer<QObject> object(component.create());
    QQuickListView *listView = qobject_cast<QQuickListView *>(object.data());
    QVERIFY(listView);
    listView->setParentItem(canvas.rootItem());

    QQuickItem *contentItem = listView->contentItem();
    QVERIFY(contentItem);

    QObject *visualModel = listView->findChild<QObject *>("visualModel");
    QVERIFY(visualModel);

    evaluate<void>(visualModel, expression);

    QCOMPARE(evaluate<int>(listView, "count"), inItems ? visualCount : modelCount);
    QCOMPARE(evaluate<int>(visualModel, "count"), inItems ? visualCount : modelCount);
    QCOMPARE(evaluate<int>(visualModel, "items.count"), inItems ? visualCount : modelCount);
    QCOMPARE(evaluate<int>(visualModel, "persistedItems.count"), persisted ? 1 : 0);
    QCOMPARE(evaluate<int>(visualModel, "visibleItems.count"), visible ? visualCount : modelCount);
    QCOMPARE(evaluate<int>(visualModel, "selectedItems.count"), selected ? 1 : 0);

    QCOMPARE(propertyData.count(), visualCount);
    for (int i = 0; i < visualCount; ++i) {
        int modelIndex = i;
        if (modelIndex > index)
            modelIndex -= 1;
        else if (modelIndex == index)
            modelIndex = -1;

        const int itemsIndex = inItems || i <= index ? i : i - 1;
        QString get;

        if (i != index) {
            get = QString("items.get(%1)").arg(itemsIndex);

            QQuickItem *item = findItem<QQuickItem>(contentItem, "delegate", modelIndex);
            QVERIFY(item);

            QCOMPARE(evaluate<int>(item, "test1"), modelIndex);
            QCOMPARE(evaluate<int>(item, "test2"), modelIndex);
            QCOMPARE(evaluate<QString>(item, "test3"), propertyData.at(i));
            QCOMPARE(evaluate<QString>(item, "test4"), propertyData.at(i));

            if (modelData) {
                QCOMPARE(evaluate<QString>(item, "test5"), propertyData.at(i));
                QCOMPARE(evaluate<QString>(item, "test6"), propertyData.at(i));
            }

            QCOMPARE(evaluate<bool>(item, "delegate.VisualDataModel.inItems"), true);
            QCOMPARE(evaluate<bool>(item, "delegate.VisualDataModel.inPersistedItems"), false);
            QCOMPARE(evaluate<bool>(item, "delegate.VisualDataModel.inVisible"), true);
            QCOMPARE(evaluate<bool>(item, "delegate.VisualDataModel.inSelected"), false);
            QCOMPARE(evaluate<bool>(item, "delegate.VisualDataModel.isUnresolved"), false);

            QCOMPARE(evaluate<int>(item, "delegate.VisualDataModel.itemsIndex"), itemsIndex);
            QCOMPARE(evaluate<int>(item, "delegate.VisualDataModel.persistedItemsIndex"), persisted && i > index ? 1 : 0);
            QCOMPARE(evaluate<int>(item, "delegate.VisualDataModel.visibleIndex"), visible || i <= index ? i : i - 1);
            QCOMPARE(evaluate<int>(item, "delegate.VisualDataModel.selectedIndex"), selected && i > index ? 1 : 0);
        } else if (inItems) {
            get = QString("items.get(%1)").arg(index);
        } else if (persisted) {
            get = "persistedItems.get(0)";
        } else if (visible) {
            get = QString("visibleItems.get(%1)").arg(index);
        } else if (selected) {
            get = "selectedItems.get(0)";
        } else {
            continue;
        }

        QCOMPARE(evaluate<int>(visualModel, get + ".model.index"), modelIndex);

        QCOMPARE(evaluate<QString>(visualModel, get + ".model." + property), propertyData.at(i));

        QCOMPARE(evaluate<bool>(visualModel, get + ".inItems"), inItems || i != index);
        QCOMPARE(evaluate<bool>(visualModel, get + ".inPersistedItems"), persisted && i == index);
        QCOMPARE(evaluate<bool>(visualModel, get + ".inVisible"), visible || i != index);
        QCOMPARE(evaluate<bool>(visualModel, get + ".inSelected"), selected && i == index);
        QCOMPARE(evaluate<bool>(visualModel, get + ".isUnresolved"), i == index);

        QCOMPARE(evaluate<int>(visualModel, get + ".itemsIndex"), inItems || i <= index ? i : i - 1);
        QCOMPARE(evaluate<int>(visualModel, get + ".persistedItemsIndex"), persisted && i > index ? 1 : 0);
        QCOMPARE(evaluate<int>(visualModel, get + ".visibleIndex"), visible || i <= index ? i : i - 1);
        QCOMPARE(evaluate<int>(visualModel, get + ".selectedIndex"), selected && i > index ? 1 : 0);
    }

    QObject *item = 0;

    if (inItems)
        item = evaluate<QObject *>(visualModel, QString("items.create(%1)").arg(index));
    else if (persisted)
        item = evaluate<QObject *>(visualModel, QString("persistedItems.create(%1)").arg(0));
    else if (visible)
        item = evaluate<QObject *>(visualModel, QString("visibleItems.create(%1)").arg(index));
    else if (selected)
        item = evaluate<QObject *>(visualModel, QString("selectedItems.create(%1)").arg(0));
    else
        return;

    QVERIFY(item);

    QCOMPARE(evaluate<int>(item, "test1"), -1);
    QCOMPARE(evaluate<int>(item, "test2"), -1);
    QCOMPARE(evaluate<QString>(item, "test3"), propertyData.at(index));
    QCOMPARE(evaluate<QString>(item, "test4"), propertyData.at(index));

    if (modelData) {
        QCOMPARE(evaluate<QString>(item, "test5"), propertyData.at(index));
        QCOMPARE(evaluate<QString>(item, "test6"), propertyData.at(index));
    }

    QCOMPARE(evaluate<bool>(item, "delegate.VisualDataModel.inItems"), inItems);
    QCOMPARE(evaluate<bool>(item, "delegate.VisualDataModel.inPersistedItems"), true);
    QCOMPARE(evaluate<bool>(item, "delegate.VisualDataModel.inVisible"), visible);
    QCOMPARE(evaluate<bool>(item, "delegate.VisualDataModel.inSelected"), selected);
    QCOMPARE(evaluate<bool>(item, "delegate.VisualDataModel.isUnresolved"), true);

    QCOMPARE(evaluate<int>(item, "delegate.VisualDataModel.itemsIndex"), index);
    QCOMPARE(evaluate<int>(item, "delegate.VisualDataModel.persistedItemsIndex"), 0);
    QCOMPARE(evaluate<int>(item, "delegate.VisualDataModel.visibleIndex"), index);
    QCOMPARE(evaluate<int>(item, "delegate.VisualDataModel.selectedIndex"), 0);
}

void tst_qquickvisualdatamodel::resolve_data()
{
    QTest::addColumn<QUrl>("source");
    QTest::addColumn<QString>("setupExpression");
    QTest::addColumn<QString>("resolveExpression");
    QTest::addColumn<int>("unresolvedCount");
    QTest::addColumn<int>("modelCount");
    QTest::addColumn<int>("visualCount");
    QTest::addColumn<int>("index");
    QTest::addColumn<bool>("inItems");
    QTest::addColumn<bool>("persisted");
    QTest::addColumn<bool>("visible");
    QTest::addColumn<bool>("selected");
    QTest::addColumn<bool>("modelData");
    QTest::addColumn<QString>("property");
    QTest::addColumn<QStringList>("propertyData");

    const QUrl listModelSource[] = {
        testFileUrl("listmodelproperties.qml"),
        testFileUrl("listmodelproperties-package.qml") };
    const QUrl singleRoleSource[] = {
        testFileUrl("singleroleproperties.qml"),
        testFileUrl("singleroleproperties-package.qml") };
    const QUrl multipleRoleSource[] = {
        testFileUrl("multipleroleproperties.qml"),
        testFileUrl("multipleroleproperties-package.qml") };
    const QUrl stringListSource[] = {
        testFileUrl("stringlistproperties.qml"),
        testFileUrl("stringlistproperties-package.qml") };
    const QUrl objectListSource[] = {
        testFileUrl("objectlistproperties.qml"),
        testFileUrl("objectlistproperties-package.qml") };

    for (int i = 0; i < 2; ++i) {
        // List Model.
        QTest::newRow("ListModel.items prepend, resolve prepended")
                << listModelSource[i]
                << QString("items.insert(0, {\"number\": \"eight\"})")
                << QString("{ listModel.insert(0, {\"number\": \"seven\"}); items.resolve(0, 1) }")
                << 5 << 5 << 5 << 0 << true << false << true << false << true
                << QString("number")
                << (QStringList() << "seven" << "one" << "two" << "three" << "four");

        QTest::newRow("ListModel.items prepend, resolve appended")
                << listModelSource[i]
                << QString("items.insert(0, {\"number\": \"eight\"})")
                << QString("{ listModel.append({\"number\": \"seven\"}); items.resolve(0, 5) }")
                << 5 << 5 << 5 << 4 << true << false << true << false << true
                << QString("number")
                << (QStringList() << "one" << "two" << "three" << "four" << "seven");

        QTest::newRow("ListModel.items prepend, resolve inserted")
                << listModelSource[i]
                << QString("items.insert(0, {\"number\": \"eight\"})")
                << QString("{ listModel.insert(2, {\"number\": \"seven\"}); items.resolve(0, 3) }")
                << 5 << 5 << 5 << 2 << true << false << true << false << true
                << QString("number")
                << (QStringList() << "one" << "two" << "seven" << "three" << "four");

        QTest::newRow("ListModel.items append, resolve prepended")
                << listModelSource[i]
                << QString("items.insert({\"number\": \"eight\"})")
                << QString("{ listModel.insert(0, {\"number\": \"seven\"}); items.resolve(5, 0) }")
                << 5 << 5 << 5 << 0 << true << false << true << false << true
                << QString("number")
                << (QStringList() << "seven" << "one" << "two" << "three" << "four");

        QTest::newRow("ListModel.items append, resolve appended")
                << listModelSource[i]
                << QString("items.insert({\"number\": \"eight\"})")
                << QString("{ listModel.append({\"number\": \"seven\"}); items.resolve(5, 4) }")
                << 5 << 5 << 5 << 4 << true << false << true << false << true
                << QString("number")
                << (QStringList() << "one" << "two" << "three" << "four" << "seven");

        QTest::newRow("ListModel.items append, resolve inserted")
                << listModelSource[i]
                << QString("items.insert({\"number\": \"eight\"})")
                << QString("{ listModel.insert(2, {\"number\": \"seven\"}); items.resolve(5, 2) }")
                << 5 << 5 << 5 << 2 << true << false << true << false << true
                << QString("number")
                << (QStringList() << "one" << "two" << "seven" << "three" << "four");

        QTest::newRow("ListModel.items insert, resolve prepended")
                << listModelSource[i]
                << QString("items.insert(2, {\"number\": \"eight\"})")
                << QString("{ listModel.insert(0, {\"number\": \"seven\"}); items.resolve(3, 0) }")
                << 5 << 5 << 5 << 0 << true << false << true << false << true
                << QString("number")
                << (QStringList() << "seven" << "one" << "two" << "three" << "four");

        QTest::newRow("ListModel.items insert, resolve appended")
                << listModelSource[i]
                << QString("items.insert(2, {\"number\": \"eight\"})")
                << QString("{ listModel.append({\"number\": \"seven\"}); items.resolve(2, 5) }")
                << 5 << 5 << 5 << 4 << true << false << true << false << true
                << QString("number")
                << (QStringList() << "one" << "two" << "three" << "four" << "seven");

        QTest::newRow("ListModel.items insert, resolve inserted")
                << listModelSource[i]
                << QString("items.insert(2, {\"number\": \"eight\"})")
                << QString("{ listModel.insert(2, {\"number\": \"seven\"}); items.resolve(2, 3) }")
                << 5 << 5 << 5 << 2 << true << false << true << false << true
                << QString("number")
                << (QStringList() << "one" << "two" << "seven" << "three" << "four");

        QTest::newRow("ListModel.items prepend, move resolved")
                << listModelSource[i]
                << QString("items.insert(0, {\"number\": \"eight\"})")
                << QString("{ listModel.insert(0, {\"number\": \"seven\"}); "
                           "items.resolve(0, 1); "
                           "listModel.move(0, 2, 1) }")
                << 5 << 5 << 5 << 2 << true << false << true << false << true
                << QString("number")
                << (QStringList() << "one" << "two" << "seven" << "three" << "four");

        QTest::newRow("ListModel.items append, move resolved")
                << listModelSource[i]
                << QString("items.insert({\"number\": \"eight\"})")
                << QString("{ listModel.append({\"number\": \"seven\"}); "
                           "items.resolve(5, 4); "
                           "listModel.move(4, 2, 1) }")
                << 5 << 5 << 5 << 2 << true << false << true << false << true
                << QString("number")
                << (QStringList() << "one" << "two" << "seven" << "three" << "four");

        QTest::newRow("ListModel.items insert, move resolved")
                << listModelSource[i]
                << QString("items.insert(2, {\"number\": \"eight\"})")
                << QString("{ listModel.insert(2, {\"number\": \"seven\"}); "
                           "items.resolve(2, 3);"
                           "listModel.move(2, 0, 1) }")
                << 5 << 5 << 5 << 0 << true << false << true << false << true
                << QString("number")
                << (QStringList() << "seven" << "one" << "two" << "three" << "four");

        QTest::newRow("ListModel.items prepend, remove resolved")
                << listModelSource[i]
                << QString("items.insert(0, {\"number\": \"eight\"})")
                << QString("{ listModel.insert(0, {\"number\": \"seven\"}); "
                           "items.resolve(0, 1); "
                           "listModel.remove(0, 1) }")
                << 5 << 4 << 4 << 4 << false << false << false << false << true
                << QString("number")
                << (QStringList() << "one" << "two" << "three" << "four");

        QTest::newRow("ListModel.items append, remove resolved")
                << listModelSource[i]
                << QString("items.insert({\"number\": \"eight\"})")
                << QString("{ listModel.append({\"number\": \"seven\"}); "
                           "items.resolve(5, 4); "
                           "listModel.remove(4, 1) }")
                << 5 << 4 << 4 << 4 << false << false << false << false << true
                << QString("number")
                << (QStringList() << "one" << "two" << "three" << "four");

        QTest::newRow("ListModel.items insert, remove resolved")
                << listModelSource[i]
                << QString("items.insert(2, {\"number\": \"eight\"})")
                << QString("{ listModel.insert(2, {\"number\": \"seven\"}); "
                           "items.resolve(2, 3);"
                           "listModel.remove(2, 1) }")
                << 5 << 4 << 4 << 4 << false << false << false << false << true
                << QString("number")
                << (QStringList() << "one" << "two" << "three" << "four");

        QTest::newRow("ListModel.selectedItems prepend, resolve prepended")
                << listModelSource[i]
                << QString("selectedItems.insert(0, {\"number\": \"eight\"})")
                << QString("{ listModel.insert(0, {\"number\": \"seven\"}); "
                           "selectedItems.resolve(selectedItems.get(0), items.get(0)) }")
                << 4 << 5 << 5 << 0 << true << false << true << true << true
                << QString("number")
                << (QStringList() << "seven" << "one" << "two" << "three" << "four");

        QTest::newRow("ListModel.selectedItems prepend, resolve appended")
                << listModelSource[i]
                << QString("selectedItems.insert(0, {\"number\": \"eight\"})")
                << QString("{ listModel.append({\"number\": \"seven\"}); "
                           "selectedItems.resolve(selectedItems.get(0), items.get(4)) }")
                << 4 << 5 << 5 << 4 << true << false << true << true << true
                << QString("number")
                << (QStringList() << "one" << "two" << "three" << "four" << "seven");

        QTest::newRow("ListModel.selectedItems prepend, resolve inserted")
                << listModelSource[i]
                << QString("selectedItems.insert(0, {\"number\": \"eight\"})")
                << QString("{ listModel.insert(2, {\"number\": \"seven\"}); "
                           "selectedItems.resolve(selectedItems.get(0), items.get(2)) }")
                << 4 << 5 << 5 << 2 << true << false << true << true << true
                << QString("number")
                << (QStringList() << "one" << "two" << "seven" << "three" << "four");

        QTest::newRow("ListModel.selectedItems append, resolve prepended")
                << listModelSource[i]
                << QString("selectedItems.insert({\"number\": \"eight\"})")
                << QString("{ listModel.insert(0, {\"number\": \"seven\"}); "
                           "selectedItems.resolve(selectedItems.get(0), items.get(0)) }")
                << 4 << 5 << 5 << 0 << true << false << true << true << true
                << QString("number")
                << (QStringList() << "seven" << "one" << "two" << "three" << "four");

        QTest::newRow("ListModel.selectedItems append, resolve appended")
                << listModelSource[i]
                << QString("selectedItems.insert({\"number\": \"eight\"})")
                << QString("{ listModel.append({\"number\": \"seven\"}); "
                           "selectedItems.resolve(selectedItems.get(0), items.get(4)) }")
                << 4 << 5 << 5 << 4 << true << false << true << true << true
                << QString("number")
                << (QStringList() << "one" << "two" << "three" << "four" << "seven");

        QTest::newRow("ListModel.selectedItems append, resolve inserted")
                << listModelSource[i]
                << QString("selectedItems.insert({\"number\": \"eight\"})")
                << QString("{ listModel.insert(2, {\"number\": \"seven\"}); "
                           "selectedItems.resolve(selectedItems.get(0), items.get(2)) }")
                << 4 << 5 << 5 << 2 << true << false << true << true << true
                << QString("number")
                << (QStringList() << "one" << "two" << "seven" << "three" << "four");

        // AbstractItemModel (Single Role)
        QTest::newRow("ListModel.items prepend, resolve prepended")
                << singleRoleSource[i]
                << QString("items.insert(0, {\"name\": \"eight\"})")
                << QString("{ items.resolve(0, 1) }")
                << 5 << 4 << 4 << 0 << true << false << true << false << true
                << QString("name")
                << (QStringList() << "one" << "two" << "three" << "four");


        QTest::newRow("ListModel.items append, resolve appended")
                << singleRoleSource[i]
                << QString("items.insert({\"name\": \"eight\"})")
                << QString("{ items.resolve(4, 3) }")
                << 5 << 4 << 4 << 3 << true << false << true << false << true
                << QString("name")
                << (QStringList() << "one" << "two" << "three" << "four");

        QTest::newRow("ListModel.items insert, resolve inserted")
                << singleRoleSource[i]
                << QString("items.insert(2, {\"name\": \"eight\"})")
                << QString("{ items.resolve(2, 3) }")
                << 5 << 4 << 4 << 2 << true << false << true << false << true
                << QString("name")
                << (QStringList() << "one" << "two" << "three" << "four");

        // AbstractItemModel (Single Role)
        QTest::newRow("AbstractItemModel.items prepend, resolve prepended")
                << singleRoleSource[i]
                << QString("items.insert(0, {\"name\": \"eight\"})")
                << QString("{ items.resolve(0, 1) }")
                << 5 << 4 << 4 << 0 << true << false << true << false << true
                << QString("name")
                << (QStringList() << "one" << "two" << "three" << "four");

        QTest::newRow("AbstractItemModel.items append, resolve appended")
                << singleRoleSource[i]
                << QString("items.insert({\"name\": \"eight\"})")
                << QString("{ items.resolve(4, 3) }")
                << 5 << 4 << 4 << 3 << true << false << true << false << true
                << QString("name")
                << (QStringList() << "one" << "two" << "three" << "four");

        QTest::newRow("AbstractItemModel.items insert, resolve inserted")
                << singleRoleSource[i]
                << QString("items.insert(2, {\"name\": \"eight\"})")
                << QString("{ items.resolve(2, 3) }")
                << 5 << 4 << 4 << 2 << true << false << true << false << true
                << QString("name")
                << (QStringList() << "one" << "two" << "three" << "four");

        // AbstractItemModel (Multiple Roles)
        QTest::newRow("StandardItemModel.items prepend, resolve prepended")
                << multipleRoleSource[i]
                << QString("items.insert(0, {\"display\": \"Row 8 Item\"})")
                << QString("{ items.resolve(0, 1) }")
                << 5 << 4 << 4 << 0 << true << false << true << false << false
                << QString("display")
                << (QStringList() << "Row 1 Item" << "Row 2 Item" << "Row 3 Item" << "Row 4 Item");

        QTest::newRow("StandardItemModel.items append, resolve appended")
                << multipleRoleSource[i]
                << QString("items.insert({\"display\": \"Row 8 Item\"})")
                << QString("{ items.resolve(4, 3) }")
                << 5 << 4 << 4 << 3 << true << false << true << false << false
                << QString("display")
                << (QStringList() << "Row 1 Item" << "Row 2 Item" << "Row 3 Item" << "Row 4 Item");

        QTest::newRow("StandardItemModel.items insert, resolve inserted")
                << multipleRoleSource[i]
                << QString("items.insert(2, {\"display\": \"Row 8 Item\"})")
                << QString("{ items.resolve(2, 3) }")
                << 5 << 4 << 4 << 2 << true << false << true << false << false
                << QString("display")
                << (QStringList() << "Row 1 Item" << "Row 2 Item" << "Row 3 Item" << "Row 4 Item");

        // StringList
        QTest::newRow("StringList.items prepend, resolve prepended")
                << stringListSource[i]
                << QString("items.insert(0, {\"modelData\": \"eight\"})")
                << QString("{ items.resolve(0, 1) }")
                << 5 << 4 << 4 << 0 << true << false << true << false << false
                << QString("modelData")
                << (QStringList() << "one" << "two" << "three" << "four");

        QTest::newRow("StringList.items append, resolve appended")
                << stringListSource[i]
                << QString("items.insert({\"modelData\": \"eight\"})")
                << QString("{ items.resolve(4, 3) }")
                << 5 << 4 << 4 << 3 << true << false << true << false << false
                << QString("modelData")
                << (QStringList() << "one" << "two" << "three" << "four");

        QTest::newRow("StringList.items insert, resolve inserted")
                << stringListSource[i]
                << QString("items.insert(2, {\"modelData\": \"eight\"})")
                << QString("{ items.resolve(2, 3) }")
                << 5 << 4 << 4 << 2 << true << false << true << false << false
                << QString("modelData")
                << (QStringList() << "one" << "two" << "three" << "four");
    }
}

void tst_qquickvisualdatamodel::resolve()
{
    QFETCH(QUrl, source);
    QFETCH(QString, setupExpression);
    QFETCH(QString, resolveExpression);
    QFETCH(int, unresolvedCount);
    QFETCH(int, modelCount);
    QFETCH(int, visualCount);
    QFETCH(int, index);
    QFETCH(bool, inItems);
    QFETCH(bool, persisted);
    QFETCH(bool, visible);
    QFETCH(bool, selected);
    QFETCH(bool, modelData);
    QFETCH(QString, property);
    QFETCH(QStringList, propertyData);

    QQuickCanvas canvas;

    QQmlComponent component(&engine);
    component.loadUrl(source);
    QScopedPointer<QObject> object(component.create());
    QQuickListView *listView = qobject_cast<QQuickListView *>(object.data());
    QVERIFY(listView);
    listView->setParentItem(canvas.rootItem());

    QQuickItem *contentItem = listView->contentItem();
    QVERIFY(contentItem);

    QObject *visualModel = listView->findChild<QObject *>("visualModel");
    QVERIFY(visualModel);

    evaluate<void>(visualModel, setupExpression);
    QCOMPARE(evaluate<int>(listView, "count"), unresolvedCount);

    evaluate<void>(visualModel, resolveExpression);

    QCOMPARE(evaluate<int>(listView, "count"), inItems ? visualCount : modelCount);
    QCOMPARE(evaluate<int>(visualModel, "count"), inItems ? visualCount : modelCount);
    QCOMPARE(evaluate<int>(visualModel, "items.count"), inItems ? visualCount : modelCount);
    QCOMPARE(evaluate<int>(visualModel, "persistedItems.count"), persisted ? 1 : 0);
    QCOMPARE(evaluate<int>(visualModel, "visibleItems.count"), visible ? visualCount : modelCount);
    QCOMPARE(evaluate<int>(visualModel, "selectedItems.count"), selected ? 1 : 0);

    QCOMPARE(propertyData.count(), visualCount);
    for (int i = 0; i < visualCount; ++i) {
        int modelIndex = i;

        const int itemsIndex = inItems || i <= index ? i : i - 1;
        QString get;

        if (i != index) {
            get = QString("items.get(%1)").arg(itemsIndex);

            QQuickItem *item = findItem<QQuickItem>(contentItem, "delegate", modelIndex);
            QVERIFY(item);

            QCOMPARE(evaluate<int>(item, "test1"), modelIndex);
            QCOMPARE(evaluate<int>(item, "test2"), modelIndex);
            QCOMPARE(evaluate<QString>(item, "test3"), propertyData.at(i));
            QCOMPARE(evaluate<QString>(item, "test4"), propertyData.at(i));

            if (modelData) {
                QCOMPARE(evaluate<QString>(item, "test5"), propertyData.at(i));
                QCOMPARE(evaluate<QString>(item, "test6"), propertyData.at(i));
            }

            QCOMPARE(evaluate<bool>(item, "delegate.VisualDataModel.inItems"), true);
            QCOMPARE(evaluate<bool>(item, "delegate.VisualDataModel.inPersistedItems"), false);
            QCOMPARE(evaluate<bool>(item, "delegate.VisualDataModel.inVisible"), true);
            QCOMPARE(evaluate<bool>(item, "delegate.VisualDataModel.inSelected"), false);
            QCOMPARE(evaluate<bool>(item, "delegate.VisualDataModel.isUnresolved"), false);

            QCOMPARE(evaluate<int>(item, "delegate.VisualDataModel.itemsIndex"), itemsIndex);
            QCOMPARE(evaluate<int>(item, "delegate.VisualDataModel.persistedItemsIndex"), persisted && i > index ? 1 : 0);
            QCOMPARE(evaluate<int>(item, "delegate.VisualDataModel.visibleIndex"), visible || i <= index ? i : i - 1);
            QCOMPARE(evaluate<int>(item, "delegate.VisualDataModel.selectedIndex"), selected && i > index ? 1 : 0);
        } else if (inItems) {
            get = QString("items.get(%1)").arg(index);
        } else if (persisted) {
            get = "persistedItems.get(0)";
        } else if (visible) {
            get = QString("visibleItems.get(%1)").arg(index);
        } else if (selected) {
            get = "selectedItems.get(0)";
        } else {
            continue;
        }

        QCOMPARE(evaluate<int>(visualModel, get + ".model.index"), modelIndex);

        QCOMPARE(evaluate<QString>(visualModel, get + ".model." + property), propertyData.at(i));

        QCOMPARE(evaluate<bool>(visualModel, get + ".inItems"), inItems || i != index);
        QCOMPARE(evaluate<bool>(visualModel, get + ".inPersistedItems"), persisted && i == index);
        QCOMPARE(evaluate<bool>(visualModel, get + ".inVisible"), visible || i != index);
        QCOMPARE(evaluate<bool>(visualModel, get + ".inSelected"), selected && i == index);
        QCOMPARE(evaluate<bool>(visualModel, get + ".isUnresolved"), false);

        QCOMPARE(evaluate<int>(visualModel, get + ".itemsIndex"), inItems || i <= index ? i : i - 1);
        QCOMPARE(evaluate<int>(visualModel, get + ".persistedItemsIndex"), persisted && i > index ? 1 : 0);
        QCOMPARE(evaluate<int>(visualModel, get + ".visibleIndex"), visible || i <= index ? i : i - 1);
        QCOMPARE(evaluate<int>(visualModel, get + ".selectedIndex"), selected && i > index ? 1 : 0);
    }

    QObject *item = 0;

    if (inItems)
        item = evaluate<QObject *>(visualModel, QString("items.create(%1)").arg(index));
    else if (persisted)
        item = evaluate<QObject *>(visualModel, QString("persistedItems.create(%1)").arg(0));
    else if (visible)
        item = evaluate<QObject *>(visualModel, QString("visibleItems.create(%1)").arg(index));
    else if (selected)
        item = evaluate<QObject *>(visualModel, QString("selectedItems.create(%1)").arg(0));
    else
        return;

    QVERIFY(item);

    QCOMPARE(evaluate<int>(item, "test1"), index);
    QCOMPARE(evaluate<int>(item, "test2"), index);
    QCOMPARE(evaluate<QString>(item, "test3"), propertyData.at(index));
    QCOMPARE(evaluate<QString>(item, "test4"), propertyData.at(index));

    if (modelData) {
        QCOMPARE(evaluate<QString>(item, "test5"), propertyData.at(index));
        QCOMPARE(evaluate<QString>(item, "test6"), propertyData.at(index));
    }

    QCOMPARE(evaluate<bool>(item, "delegate.VisualDataModel.inItems"), inItems);
    QCOMPARE(evaluate<bool>(item, "delegate.VisualDataModel.inPersistedItems"), true);
    QCOMPARE(evaluate<bool>(item, "delegate.VisualDataModel.inVisible"), visible);
    QCOMPARE(evaluate<bool>(item, "delegate.VisualDataModel.inSelected"), selected);
    QCOMPARE(evaluate<bool>(item, "delegate.VisualDataModel.isUnresolved"), false);

    QCOMPARE(evaluate<int>(item, "delegate.VisualDataModel.itemsIndex"), index);
    QCOMPARE(evaluate<int>(item, "delegate.VisualDataModel.persistedItemsIndex"), 0);
    QCOMPARE(evaluate<int>(item, "delegate.VisualDataModel.visibleIndex"), index);
    QCOMPARE(evaluate<int>(item, "delegate.VisualDataModel.selectedIndex"), 0);
}

void tst_qquickvisualdatamodel::warnings_data()
{
    QTest::addColumn<QUrl>("source");
    QTest::addColumn<QString>("expression");
    QTest::addColumn<QString>("warning");
    QTest::addColumn<int>("count");

    QTest::newRow("insert < 0")
            << testFileUrl("listmodelproperties.qml")
            << QString("items.insert(-2, {\"number\": \"eight\"})")
            << ("<Unknown File>: QML VisualDataGroup: " + QQuickVisualDataGroup::tr("insert: index out of range"))
            << 4;

    QTest::newRow("insert > length")
            << testFileUrl("listmodelproperties.qml")
            << QString("items.insert(8, {\"number\": \"eight\"})")
            << ("<Unknown File>: QML VisualDataGroup: " + QQuickVisualDataGroup::tr("insert: index out of range"))
            << 4;

    QTest::newRow("create < 0")
            << testFileUrl("listmodelproperties.qml")
            << QString("items.create(-2, {\"number\": \"eight\"})")
            << ("<Unknown File>: QML VisualDataGroup: " + QQuickVisualDataGroup::tr("create: index out of range"))
            << 4;

    QTest::newRow("create > length")
            << testFileUrl("listmodelproperties.qml")
            << QString("items.create(8, {\"number\": \"eight\"})")
            << ("<Unknown File>: QML VisualDataGroup: " + QQuickVisualDataGroup::tr("create: index out of range"))
            << 4;

    QTest::newRow("resolve from < 0")
            << testFileUrl("listmodelproperties.qml")
            << QString("items.resolve(-2, 3)")
            << ("<Unknown File>: QML VisualDataGroup: " + QQuickVisualDataGroup::tr("resolve: from index out of range"))
            << 4;

    QTest::newRow("resolve from > length")
            << testFileUrl("listmodelproperties.qml")
            << QString("items.resolve(8, 3)")
            << ("<Unknown File>: QML VisualDataGroup: " + QQuickVisualDataGroup::tr("resolve: from index out of range"))
            << 4;

    QTest::newRow("resolve to < 0")
            << testFileUrl("listmodelproperties.qml")
            << QString("items.resolve(3, -2)")
            << ("<Unknown File>: QML VisualDataGroup: " + QQuickVisualDataGroup::tr("resolve: to index out of range"))
            << 4;

    QTest::newRow("resolve to > length")
            << testFileUrl("listmodelproperties.qml")
            << QString("items.resolve(3, 8)")
            << ("<Unknown File>: QML VisualDataGroup: " + QQuickVisualDataGroup::tr("resolve: to index out of range"))
            << 4;

    QTest::newRow("resolve from invalid index")
            << testFileUrl("listmodelproperties.qml")
            << QString("items.resolve(\"two\", 3)")
            << ("<Unknown File>: QML VisualDataGroup: " + QQuickVisualDataGroup::tr("resolve: from index invalid"))
            << 4;

    QTest::newRow("resolve to invalid index")
            << testFileUrl("listmodelproperties.qml")
            << QString("items.resolve(3, \"two\")")
            << ("<Unknown File>: QML VisualDataGroup: " + QQuickVisualDataGroup::tr("resolve: to index invalid"))
            << 4;

    QTest::newRow("resolve already resolved item")
            << testFileUrl("listmodelproperties.qml")
            << QString("items.resolve(3, 2)")
            << ("<Unknown File>: QML VisualDataGroup: " + QQuickVisualDataGroup::tr("resolve: from is not an unresolved item"))
            << 4;

    QTest::newRow("resolve already resolved item")
            << testFileUrl("listmodelproperties.qml")
            << QString("{ items.insert(0, {\"number\": \"eight\"});"
                       "items.insert(1, {\"number\": \"seven\"});"
                       "items.resolve(0, 1)}")
            << ("<Unknown File>: QML VisualDataGroup: " + QQuickVisualDataGroup::tr("resolve: to is not a model item"))
            << 6;

    QTest::newRow("remove index < 0")
            << testFileUrl("listmodelproperties.qml")
            << QString("items.remove(-2, 1)")
            << ("<Unknown File>: QML VisualDataGroup: " + QQuickVisualDataGroup::tr("remove: index out of range"))
            << 4;

    QTest::newRow("remove index == length")
            << testFileUrl("listmodelproperties.qml")
            << QString("items.remove(4, 1)")
            << ("<Unknown File>: QML VisualDataGroup: " + QQuickVisualDataGroup::tr("remove: index out of range"))
            << 4;

    QTest::newRow("remove index > length")
            << testFileUrl("listmodelproperties.qml")
            << QString("items.remove(9, 1)")
            << ("<Unknown File>: QML VisualDataGroup: " + QQuickVisualDataGroup::tr("remove: index out of range"))
            << 4;

    QTest::newRow("remove invalid index")
            << testFileUrl("listmodelproperties.qml")
            << QString("items.remove(\"nine\", 1)")
            << ("<Unknown File>: QML VisualDataGroup: " + QQuickVisualDataGroup::tr("remove: invalid index"))
            << 4;

    QTest::newRow("remove count < 0")
            << testFileUrl("listmodelproperties.qml")
            << QString("items.remove(1, -2)")
            << ("<Unknown File>: QML VisualDataGroup: " + QQuickVisualDataGroup::tr("remove: invalid count"))
            << 4;

    QTest::newRow("remove index + count > length")
            << testFileUrl("listmodelproperties.qml")
            << QString("items.remove(2, 4, \"selected\")")
            << ("<Unknown File>: QML VisualDataGroup: " + QQuickVisualDataGroup::tr("remove: invalid count"))
            << 4;

    QTest::newRow("addGroups index < 0")
            << testFileUrl("listmodelproperties.qml")
            << QString("items.addGroups(-2, 1, \"selected\")")
            << ("<Unknown File>: QML VisualDataGroup: " + QQuickVisualDataGroup::tr("addGroups: index out of range"))
            << 4;

    QTest::newRow("addGroups index == length")
            << testFileUrl("listmodelproperties.qml")
            << QString("items.addGroups(4, 1, \"selected\")")
            << ("<Unknown File>: QML VisualDataGroup: " + QQuickVisualDataGroup::tr("addGroups: index out of range"))
            << 4;

    QTest::newRow("addGroups index > length")
            << testFileUrl("listmodelproperties.qml")
            << QString("items.addGroups(9, 1, \"selected\")")
            << ("<Unknown File>: QML VisualDataGroup: " + QQuickVisualDataGroup::tr("addGroups: index out of range"))
            << 4;

    QTest::newRow("addGroups count < 0")
            << testFileUrl("listmodelproperties.qml")
            << QString("items.addGroups(1, -2, \"selected\")")
            << ("<Unknown File>: QML VisualDataGroup: " + QQuickVisualDataGroup::tr("addGroups: invalid count"))
            << 4;

    QTest::newRow("addGroups index + count > length")
            << testFileUrl("listmodelproperties.qml")
            << QString("items.addGroups(2, 4, \"selected\")")
            << ("<Unknown File>: QML VisualDataGroup: " + QQuickVisualDataGroup::tr("addGroups: invalid count"))
            << 4;

    QTest::newRow("removeGroups index < 0")
            << testFileUrl("listmodelproperties.qml")
            << QString("items.removeGroups(-2, 1, \"selected\")")
            << ("<Unknown File>: QML VisualDataGroup: " + QQuickVisualDataGroup::tr("removeGroups: index out of range"))
            << 4;

    QTest::newRow("removeGroups index == length")
            << testFileUrl("listmodelproperties.qml")
            << QString("items.removeGroups(4, 1, \"selected\")")
            << ("<Unknown File>: QML VisualDataGroup: " + QQuickVisualDataGroup::tr("removeGroups: index out of range"))
            << 4;

    QTest::newRow("removeGroups index > length")
            << testFileUrl("listmodelproperties.qml")
            << QString("items.removeGroups(9, 1, \"selected\")")
            << ("<Unknown File>: QML VisualDataGroup: " + QQuickVisualDataGroup::tr("removeGroups: index out of range"))
            << 4;

    QTest::newRow("removeGroups count < 0")
            << testFileUrl("listmodelproperties.qml")
            << QString("items.removeGroups(1, -2, \"selected\")")
            << ("<Unknown File>: QML VisualDataGroup: " + QQuickVisualDataGroup::tr("removeGroups: invalid count"))
            << 4;

    QTest::newRow("removeGroups index + count > length")
            << testFileUrl("listmodelproperties.qml")
            << QString("items.removeGroups(2, 4, \"selected\")")
            << ("<Unknown File>: QML VisualDataGroup: " + QQuickVisualDataGroup::tr("removeGroups: invalid count"))
            << 4;

    QTest::newRow("setGroups index < 0")
            << testFileUrl("listmodelproperties.qml")
            << QString("items.setGroups(-2, 1, \"selected\")")
            << ("<Unknown File>: QML VisualDataGroup: " + QQuickVisualDataGroup::tr("setGroups: index out of range"))
            << 4;

    QTest::newRow("setGroups index == length")
            << testFileUrl("listmodelproperties.qml")
            << QString("items.setGroups(4, 1, \"selected\")")
            << ("<Unknown File>: QML VisualDataGroup: " + QQuickVisualDataGroup::tr("setGroups: index out of range"))
            << 4;

    QTest::newRow("setGroups index > length")
            << testFileUrl("listmodelproperties.qml")
            << QString("items.setGroups(9, 1, \"selected\")")
            << ("<Unknown File>: QML VisualDataGroup: " + QQuickVisualDataGroup::tr("setGroups: index out of range"))
            << 4;

    QTest::newRow("setGroups count < 0")
            << testFileUrl("listmodelproperties.qml")
            << QString("items.setGroups(1, -2, \"selected\")")
            << ("<Unknown File>: QML VisualDataGroup: " + QQuickVisualDataGroup::tr("setGroups: invalid count"))
            << 4;

    QTest::newRow("setGroups index + count > length")
            << testFileUrl("listmodelproperties.qml")
            << QString("items.setGroups(2, 4, \"selected\")")
            << ("<Unknown File>: QML VisualDataGroup: " + QQuickVisualDataGroup::tr("setGroups: invalid count"))
            << 4;

    QTest::newRow("move from < 0")
            << testFileUrl("listmodelproperties.qml")
            << QString("items.move(-2, 1, 1)")
            << ("<Unknown File>: QML VisualDataGroup: " + QQuickVisualDataGroup::tr("move: from index out of range"))
            << 4;

    QTest::newRow("move from == length")
            << testFileUrl("listmodelproperties.qml")
            << QString("items.move(4, 1, 1)")
            << ("<Unknown File>: QML VisualDataGroup: " + QQuickVisualDataGroup::tr("move: from index out of range"))
            << 4;

    QTest::newRow("move from > length")
            << testFileUrl("listmodelproperties.qml")
            << QString("items.move(9, 1, 1)")
            << ("<Unknown File>: QML VisualDataGroup: " + QQuickVisualDataGroup::tr("move: from index out of range"))
            << 4;

    QTest::newRow("move invalid from")
            << testFileUrl("listmodelproperties.qml")
            << QString("items.move(\"nine\", 1, 1)")
            << ("<Unknown File>: QML VisualDataGroup: " + QQuickVisualDataGroup::tr("move: invalid from index"))
            << 4;

    QTest::newRow("move to < 0")
            << testFileUrl("listmodelproperties.qml")
            << QString("items.move(1, -2, 1)")
            << ("<Unknown File>: QML VisualDataGroup: " + QQuickVisualDataGroup::tr("move: to index out of range"))
            << 4;

    QTest::newRow("move to == length")
            << testFileUrl("listmodelproperties.qml")
            << QString("items.move(1, 4, 1)")
            << ("<Unknown File>: QML VisualDataGroup: " + QQuickVisualDataGroup::tr("move: to index out of range"))
            << 4;

    QTest::newRow("move to > length")
            << testFileUrl("listmodelproperties.qml")
            << QString("items.move(1, 9, 1)")
            << ("<Unknown File>: QML VisualDataGroup: " + QQuickVisualDataGroup::tr("move: to index out of range"))
            << 4;

    QTest::newRow("move invalid to")
            << testFileUrl("listmodelproperties.qml")
            << QString("items.move(1, \"nine\", 1)")
            << ("<Unknown File>: QML VisualDataGroup: " + QQuickVisualDataGroup::tr("move: invalid to index"))
            << 4;

    QTest::newRow("move count < 0")
            << testFileUrl("listmodelproperties.qml")
            << QString("items.move(1, 1, -2)")
            << ("<Unknown File>: QML VisualDataGroup: " + QQuickVisualDataGroup::tr("move: invalid count"))
            << 4;

    QTest::newRow("move from + count > length")
            << testFileUrl("listmodelproperties.qml")
            << QString("items.move(2, 1, 4)")
            << ("<Unknown File>: QML VisualDataGroup: " + QQuickVisualDataGroup::tr("move: from index out of range"))
            << 4;
}

void tst_qquickvisualdatamodel::warnings()
{
    QFETCH(QUrl, source);
    QFETCH(QString, expression);
    QFETCH(QString, warning);
    QFETCH(int, count);

    QQuickCanvas canvas;

    QQmlComponent component(&engine);
    component.loadUrl(source);
    QScopedPointer<QObject> object(component.create());
    QQuickListView *listView = qobject_cast<QQuickListView *>(object.data());
    QVERIFY(listView);
    listView->setParentItem(canvas.rootItem());

    QQuickItem *contentItem = listView->contentItem();
    QVERIFY(contentItem);

    QObject *visualModel = evaluate<QObject *>(listView, "model");
    QVERIFY(visualModel);

    QTest::ignoreMessage(QtWarningMsg, warning.toUtf8());

    evaluate<void>(visualModel, expression);
    QCOMPARE(evaluate<int>(listView, "count"), count);
}


QTEST_MAIN(tst_qquickvisualdatamodel)

#include "tst_qquickvisualdatamodel.moc"