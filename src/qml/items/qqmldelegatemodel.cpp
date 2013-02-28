/****************************************************************************
**
** Copyright (C) 2013 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtQml module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia.  For licensing terms and
** conditions see http://qt.digia.com/licensing.  For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights.  These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3.0 as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU General Public License version 3.0 requirements will be
** met: http://www.gnu.org/copyleft/gpl.html.
**
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qqmldelegatemodel_p_p.h"

#include <QtQml/qqmlinfo.h>

#include <private/qquickpackage_p.h>
#include <private/qmetaobjectbuilder_p.h>
#include <private/qqmladaptormodel_p.h>
#include <private/qqmlchangeset_p.h>
#include <private/qqmlengine_p.h>
#include <private/qqmlcomponent_p.h>
#include <private/qqmlincubator_p.h>
#include <private/qqmlcompiler_p.h>

QT_BEGIN_NAMESPACE

class QQmlDelegateModelEngineData : public QV8Engine::Deletable
{
public:
    enum
    {
        Model,
        Groups,
        IsUnresolved,
        ItemsIndex,
        PersistedItemsIndex,
        InItems,
        InPersistedItems,
        StringCount
    };

    QQmlDelegateModelEngineData(QV8Engine *engine);
    ~QQmlDelegateModelEngineData();

    v8::Local<v8::Object> array(
            QV8Engine *engine, const QVector<QQmlChangeSet::Remove> &changes);
    v8::Local<v8::Object> array(
            QV8Engine *engine, const QVector<QQmlChangeSet::Insert> &changes);
    v8::Local<v8::Object> array(
            QV8Engine *engine, const QVector<QQmlChangeSet::Change> &changes);


    inline v8::Local<v8::String> model() { return strings->Get(Model)->ToString(); }
    inline v8::Local<v8::String> groups() { return strings->Get(Groups)->ToString(); }
    inline v8::Local<v8::String> isUnresolved() { return strings->Get(IsUnresolved)->ToString(); }
    inline v8::Local<v8::String> itemsIndex() { return strings->Get(ItemsIndex)->ToString(); }
    inline v8::Local<v8::String> persistedItemsIndex() { return strings->Get(PersistedItemsIndex)->ToString(); }
    inline v8::Local<v8::String> inItems() { return strings->Get(InItems)->ToString(); }
    inline v8::Local<v8::String> inPersistedItems() { return strings->Get(InPersistedItems)->ToString(); }

    v8::Persistent<v8::Array> strings;
    v8::Persistent<v8::Function> constructorChange;
    v8::Persistent<v8::Function> constructorChangeArray;
};

V8_DEFINE_EXTENSION(QQmlDelegateModelEngineData, engineData)


void QQmlDelegateModelPartsMetaObject::propertyCreated(int, QMetaPropertyBuilder &prop)
{
    prop.setWritable(false);
}

QVariant QQmlDelegateModelPartsMetaObject::initialValue(int id)
{
    QQmlDelegateModelParts *parts = static_cast<QQmlDelegateModelParts *>(object());
    QQmlPartsModel *m = new QQmlPartsModel(
            parts->model, QString::fromUtf8(name(id)), parts);
    parts->models.append(m);
    return QVariant::fromValue(static_cast<QObject *>(m));
}

QQmlDelegateModelParts::QQmlDelegateModelParts(QQmlDelegateModel *parent)
: QObject(parent), model(parent)
{
    new QQmlDelegateModelPartsMetaObject(this);
}

//---------------------------------------------------------------------------

/*!
    \qmltype VisualDataModel
    \instantiates QQmlDelegateModel
    \inqmlmodule QtQuick 2
    \ingroup qtquick-models
    \brief Encapsulates a model and delegate

    The VisualDataModel type encapsulates a model and the delegate that will
    be instantiated for items in the model.

    It is usually not necessary to create a VisualDataModel.
    However, it can be useful for manipulating and accessing the \l modelIndex
    when a QAbstractItemModel subclass is used as the
    model. Also, VisualDataModel is used together with \l Package to
    provide delegates to multiple views, and with VisualDataGroup to sort and filter
    delegate items.

    The example below illustrates using a VisualDataModel with a ListView.

    \snippet qml/visualdatamodel.qml 0
*/

QQmlDelegateModelPrivate::QQmlDelegateModelPrivate(QQmlContext *ctxt)
    : m_delegate(0)
    , m_cacheMetaType(0)
    , m_context(ctxt)
    , m_parts(0)
    , m_filterGroup(QStringLiteral("items"))
    , m_count(0)
    , m_groupCount(Compositor::MinimumGroupCount)
    , m_compositorGroup(Compositor::Cache)
    , m_complete(false)
    , m_delegateValidated(false)
    , m_reset(false)
    , m_transaction(false)
    , m_incubatorCleanupScheduled(false)
    , m_cacheItems(0)
    , m_items(0)
    , m_persistedItems(0)
{
}

QQmlDelegateModelPrivate::~QQmlDelegateModelPrivate()
{
    qDeleteAll(m_finishedIncubating);

    if (m_cacheMetaType)
        m_cacheMetaType->release();
}

void QQmlDelegateModelPrivate::init()
{
    Q_Q(QQmlDelegateModel);
    m_compositor.setRemoveGroups(Compositor::GroupMask & ~Compositor::PersistedFlag);

    m_items = new QQmlDataGroup(QStringLiteral("items"), q, Compositor::Default, q);
    m_items->setDefaultInclude(true);
    m_persistedItems = new QQmlDataGroup(QStringLiteral("persistedItems"), q, Compositor::Persisted, q);
    QQmlDataGroupPrivate::get(m_items)->emitters.insert(this);
}

QQmlDelegateModel::QQmlDelegateModel()
: QQmlInstanceModel(*(new QQmlDelegateModelPrivate(0)))
{
    Q_D(QQmlDelegateModel);
    d->init();
}

QQmlDelegateModel::QQmlDelegateModel(QQmlContext *ctxt, QObject *parent)
: QQmlInstanceModel(*(new QQmlDelegateModelPrivate(ctxt)), parent)
{
    Q_D(QQmlDelegateModel);
    d->init();
}

QQmlDelegateModel::~QQmlDelegateModel()
{
    Q_D(QQmlDelegateModel);

    foreach (QQmlDelegateModelItem *cacheItem, d->m_cache) {
        if (cacheItem->object) {
            delete cacheItem->object;

            cacheItem->object = 0;
            cacheItem->contextData->destroy();
            cacheItem->contextData = 0;
            cacheItem->scriptRef -= 1;
        }
        cacheItem->groups &= ~Compositor::UnresolvedFlag;
        cacheItem->objectRef = 0;
        if (!cacheItem->isReferenced())
            delete cacheItem;
    }
}


void QQmlDelegateModel::classBegin()
{
    Q_D(QQmlDelegateModel);
    if (!d->m_context)
        d->m_context = qmlContext(this);
}

void QQmlDelegateModel::componentComplete()
{
    Q_D(QQmlDelegateModel);
    d->m_complete = true;

    int defaultGroups = 0;
    QStringList groupNames;
    groupNames.append(QStringLiteral("items"));
    groupNames.append(QStringLiteral("persistedItems"));
    if (QQmlDataGroupPrivate::get(d->m_items)->defaultInclude)
        defaultGroups |= Compositor::DefaultFlag;
    if (QQmlDataGroupPrivate::get(d->m_persistedItems)->defaultInclude)
        defaultGroups |= Compositor::PersistedFlag;
    for (int i = Compositor::MinimumGroupCount; i < d->m_groupCount; ++i) {
        QString name = d->m_groups[i]->name();
        if (name.isEmpty()) {
            d->m_groups[i] = d->m_groups[d->m_groupCount - 1];
            --d->m_groupCount;
            --i;
        } else if (name.at(0).isUpper()) {
            qmlInfo(d->m_groups[i]) << QQmlDataGroup::tr("Group names must start with a lower case letter");
            d->m_groups[i] = d->m_groups[d->m_groupCount - 1];
            --d->m_groupCount;
            --i;
        } else {
            groupNames.append(name);

            QQmlDataGroupPrivate *group = QQmlDataGroupPrivate::get(d->m_groups[i]);
            group->setModel(this, Compositor::Group(i));
            if (group->defaultInclude)
                defaultGroups |= (1 << i);
        }
    }

    d->m_cacheMetaType = new QQmlDelegateModelItemMetaType(
            QQmlEnginePrivate::getV8Engine(d->m_context->engine()), this, groupNames);

    d->m_compositor.setGroupCount(d->m_groupCount);
    d->m_compositor.setDefaultGroups(defaultGroups);
    d->updateFilterGroup();

    while (!d->m_pendingParts.isEmpty())
        static_cast<QQmlPartsModel *>(d->m_pendingParts.first())->updateFilterGroup();

    QVector<Compositor::Insert> inserts;
    d->m_count = d->m_adaptorModel.count();
    d->m_compositor.append(
            &d->m_adaptorModel,
            0,
            d->m_count,
            defaultGroups | Compositor::AppendFlag | Compositor::PrependFlag,
            &inserts);
    d->itemsInserted(inserts);
    d->emitChanges();

    if (d->m_adaptorModel.canFetchMore())
        QCoreApplication::postEvent(this, new QEvent(QEvent::UpdateRequest));
}

/*!
    \qmlproperty model QtQuick2::VisualDataModel::model
    This property holds the model providing data for the VisualDataModel.

    The model provides a set of data that is used to create the items
    for a view.  For large or dynamic datasets the model is usually
    provided by a C++ model object.  The C++ model object must be a \l
    {QAbstractItemModel} subclass or a simple list.

    Models can also be created directly in QML, using a \l{ListModel} or
    \l{XmlListModel}.

    \sa {qml-data-models}{Data Models}
*/
QVariant QQmlDelegateModel::model() const
{
    Q_D(const QQmlDelegateModel);
    return d->m_adaptorModel.model();
}

void QQmlDelegateModel::setModel(const QVariant &model)
{
    Q_D(QQmlDelegateModel);

    if (d->m_complete)
        _q_itemsRemoved(0, d->m_count);

    d->m_adaptorModel.setModel(model, this, d->m_context->engine());
    d->m_adaptorModel.replaceWatchedRoles(QList<QByteArray>(), d->m_watchedRoles);
    for (int i = 0; d->m_parts && i < d->m_parts->models.count(); ++i) {
        d->m_adaptorModel.replaceWatchedRoles(
                QList<QByteArray>(), d->m_parts->models.at(i)->watchedRoles());
    }

    if (d->m_complete) {
        _q_itemsInserted(0, d->m_adaptorModel.count());
        if (d->m_adaptorModel.canFetchMore())
            QCoreApplication::postEvent(this, new QEvent(QEvent::UpdateRequest));
    }
}

/*!
    \qmlproperty Component QtQuick2::VisualDataModel::delegate

    The delegate provides a template defining each item instantiated by a view.
    The index is exposed as an accessible \c index property.  Properties of the
    model are also available depending upon the type of \l {qml-data-models}{Data Model}.
*/
QQmlComponent *QQmlDelegateModel::delegate() const
{
    Q_D(const QQmlDelegateModel);
    return d->m_delegate;
}

void QQmlDelegateModel::setDelegate(QQmlComponent *delegate)
{
    Q_D(QQmlDelegateModel);
    if (d->m_transaction) {
        qmlInfo(this) << tr("The delegate of a VisualDataModel cannot be changed within onUpdated.");
        return;
    }
    bool wasValid = d->m_delegate != 0;
    d->m_delegate = delegate;
    d->m_delegateValidated = false;
    if (wasValid && d->m_complete) {
        for (int i = 1; i < d->m_groupCount; ++i) {
            QQmlDataGroupPrivate::get(d->m_groups[i])->changeSet.remove(
                    0, d->m_compositor.count(Compositor::Group(i)));
        }
    }
    if (d->m_complete && d->m_delegate) {
        for (int i = 1; i < d->m_groupCount; ++i) {
            QQmlDataGroupPrivate::get(d->m_groups[i])->changeSet.insert(
                    0, d->m_compositor.count(Compositor::Group(i)));
        }
    }
    d->emitChanges();
}

/*!
    \qmlproperty QModelIndex QtQuick2::VisualDataModel::rootIndex

    QAbstractItemModel provides a hierarchical tree of data, whereas
    QML only operates on list data.  \c rootIndex allows the children of
    any node in a QAbstractItemModel to be provided by this model.

    This property only affects models of type QAbstractItemModel that
    are hierarchical (e.g, a tree model).

    For example, here is a simple interactive file system browser.
    When a directory name is clicked, the view's \c rootIndex is set to the
    QModelIndex node of the clicked directory, thus updating the view to show
    the new directory's contents.

    \c main.cpp:
    \snippet qml/visualdatamodel_rootindex/main.cpp 0

    \c view.qml:
    \snippet qml/visualdatamodel_rootindex/view.qml 0

    If the \l model is a QAbstractItemModel subclass, the delegate can also
    reference a \c hasModelChildren property (optionally qualified by a
    \e model. prefix) that indicates whether the delegate's model item has
    any child nodes.


    \sa modelIndex(), parentModelIndex()
*/
QVariant QQmlDelegateModel::rootIndex() const
{
    Q_D(const QQmlDelegateModel);
    return QVariant::fromValue(QModelIndex(d->m_adaptorModel.rootIndex));
}

void QQmlDelegateModel::setRootIndex(const QVariant &root)
{
    Q_D(QQmlDelegateModel);

    QModelIndex modelIndex = qvariant_cast<QModelIndex>(root);
    const bool changed = d->m_adaptorModel.rootIndex != modelIndex;
    if (changed || !d->m_adaptorModel.isValid()) {
        const int oldCount = d->m_count;
        d->m_adaptorModel.rootIndex = modelIndex;
        if (!d->m_adaptorModel.isValid() && d->m_adaptorModel.aim())  // The previous root index was invalidated, so we need to reconnect the model.
            d->m_adaptorModel.setModel(d->m_adaptorModel.list.list(), this, d->m_context->engine());
        if (d->m_adaptorModel.canFetchMore())
            d->m_adaptorModel.fetchMore();
        if (d->m_complete) {
            const int newCount = d->m_adaptorModel.count();
            if (oldCount)
                _q_itemsRemoved(0, oldCount);
            if (newCount)
                _q_itemsInserted(0, newCount);
        }
        if (changed)
            emit rootIndexChanged();
    }
}

/*!
    \qmlmethod QModelIndex QtQuick2::VisualDataModel::modelIndex(int index)

    QAbstractItemModel provides a hierarchical tree of data, whereas
    QML only operates on list data.  This function assists in using
    tree models in QML.

    Returns a QModelIndex for the specified index.
    This value can be assigned to rootIndex.

    \sa rootIndex
*/
QVariant QQmlDelegateModel::modelIndex(int idx) const
{
    Q_D(const QQmlDelegateModel);
    return d->m_adaptorModel.modelIndex(idx);
}

/*!
    \qmlmethod QModelIndex QtQuick2::VisualDataModel::parentModelIndex()

    QAbstractItemModel provides a hierarchical tree of data, whereas
    QML only operates on list data.  This function assists in using
    tree models in QML.

    Returns a QModelIndex for the parent of the current rootIndex.
    This value can be assigned to rootIndex.

    \sa rootIndex
*/
QVariant QQmlDelegateModel::parentModelIndex() const
{
    Q_D(const QQmlDelegateModel);
    return d->m_adaptorModel.parentModelIndex();
}

/*!
    \qmlproperty int QtQuick2::VisualDataModel::count
*/

int QQmlDelegateModel::count() const
{
    Q_D(const QQmlDelegateModel);
    if (!d->m_delegate)
        return 0;
    return d->m_compositor.count(d->m_compositorGroup);
}

QQmlDelegateModel::ReleaseFlags QQmlDelegateModelPrivate::release(QObject *object)
{
    QQmlDelegateModel::ReleaseFlags stat = 0;
    if (!object)
        return stat;

    if (QQmlDelegateModelItem *cacheItem = QQmlDelegateModelItem::dataForObject(object)) {
        if (cacheItem->releaseObject()) {
            cacheItem->destroyObject();
            emitDestroyingItem(object);
            if (cacheItem->incubationTask) {
                releaseIncubator(cacheItem->incubationTask);
                cacheItem->incubationTask = 0;
            }
            cacheItem->Dispose();
            stat |= QQmlInstanceModel::Destroyed;
        } else {
            stat |= QQmlDelegateModel::Referenced;
        }
    }
    return stat;
}

/*
  Returns ReleaseStatus flags.
*/

QQmlDelegateModel::ReleaseFlags QQmlDelegateModel::release(QObject *item)
{
    Q_D(QQmlDelegateModel);
    QQmlInstanceModel::ReleaseFlags stat = d->release(item);
    return stat;
}

// Cancel a requested async item
void QQmlDelegateModel::cancel(int index)
{
    Q_D(QQmlDelegateModel);
    if (!d->m_delegate || index < 0 || index >= d->m_compositor.count(d->m_compositorGroup)) {
        qWarning() << "VisualDataModel::cancel: index out range" << index << d->m_compositor.count(d->m_compositorGroup);
        return;
    }

    Compositor::iterator it = d->m_compositor.find(d->m_compositorGroup, index);
    QQmlDelegateModelItem *cacheItem = it->inCache() ? d->m_cache.at(it.cacheIndex) : 0;
    if (cacheItem) {
        if (cacheItem->incubationTask && !cacheItem->isObjectReferenced()) {
            d->releaseIncubator(cacheItem->incubationTask);
            cacheItem->incubationTask = 0;

            if (cacheItem->object) {
                QObject *object = cacheItem->object;
                cacheItem->destroyObject();
                if (QQuickPackage *package = qmlobject_cast<QQuickPackage *>(object))
                    d->emitDestroyingPackage(package);
                else
                    d->emitDestroyingItem(object);
            }

            cacheItem->scriptRef -= 1;
        }
        if (!cacheItem->isReferenced()) {
            d->m_compositor.clearFlags(Compositor::Cache, it.cacheIndex, 1, Compositor::CacheFlag);
            d->m_cache.removeAt(it.cacheIndex);
            delete cacheItem;
            Q_ASSERT(d->m_cache.count() == d->m_compositor.count(Compositor::Cache));
        }
    }
}

void QQmlDelegateModelPrivate::group_append(
        QQmlListProperty<QQmlDataGroup> *property, QQmlDataGroup *group)
{
    QQmlDelegateModelPrivate *d = static_cast<QQmlDelegateModelPrivate *>(property->data);
    if (d->m_complete)
        return;
    if (d->m_groupCount == Compositor::MaximumGroupCount) {
        qmlInfo(d->q_func()) << QQmlDelegateModel::tr("The maximum number of supported VisualDataGroups is 8");
        return;
    }
    d->m_groups[d->m_groupCount] = group;
    d->m_groupCount += 1;
}

int QQmlDelegateModelPrivate::group_count(
        QQmlListProperty<QQmlDataGroup> *property)
{
    QQmlDelegateModelPrivate *d = static_cast<QQmlDelegateModelPrivate *>(property->data);
    return d->m_groupCount - 1;
}

QQmlDataGroup *QQmlDelegateModelPrivate::group_at(
        QQmlListProperty<QQmlDataGroup> *property, int index)
{
    QQmlDelegateModelPrivate *d = static_cast<QQmlDelegateModelPrivate *>(property->data);
    return index >= 0 && index < d->m_groupCount - 1
            ? d->m_groups[index + 1]
            : 0;
}

/*!
    \qmlproperty list<VisualDataGroup> QtQuick2::VisualDataModel::groups

    This property holds a visual data model's group definitions.

    Groups define a sub-set of the items in a visual data model and can be used to filter
    a model.

    For every group defined in a VisualDataModel two attached properties are added to each
    delegate item.  The first of the form VisualDataModel.in\e{GroupName} holds whether the
    item belongs to the group and the second VisualDataModel.\e{groupName}Index holds the
    index of the item in that group.

    The following example illustrates using groups to select items in a model.

    \snippet qml/visualdatagroup.qml 0
*/

QQmlListProperty<QQmlDataGroup> QQmlDelegateModel::groups()
{
    Q_D(QQmlDelegateModel);
    return QQmlListProperty<QQmlDataGroup>(
            this,
            d,
            QQmlDelegateModelPrivate::group_append,
            QQmlDelegateModelPrivate::group_count,
            QQmlDelegateModelPrivate::group_at,
            0);
}

/*!
    \qmlproperty VisualDataGroup QtQuick2::VisualDataModel::items

    This property holds visual data model's default group to which all new items are added.
*/

QQmlDataGroup *QQmlDelegateModel::items()
{
    Q_D(QQmlDelegateModel);
    return d->m_items;
}

/*!
    \qmlproperty VisualDataGroup QtQuick2::VisualDataModel::persistedItems

    This property holds visual data model's persisted items group.

    Items in this group are not destroyed when released by a view, instead they are persisted
    until removed from the group.

    An item can be removed from the persistedItems group by setting the
    VisualDataModel.inPersistedItems property to false.  If the item is not referenced by a view
    at that time it will be destroyed.  Adding an item to this group will not create a new
    instance.

    Items returned by the \l QtQuick2::VisualDataGroup::create() function are automatically added
    to this group.
*/

QQmlDataGroup *QQmlDelegateModel::persistedItems()
{
    Q_D(QQmlDelegateModel);
    return d->m_persistedItems;
}

/*!
    \qmlproperty string QtQuick2::VisualDataModel::filterOnGroup

    This property holds the name of the group used to filter the visual data model.

    Only items which belong to this group are visible to a view.

    By default this is the \l items group.
*/

QString QQmlDelegateModel::filterGroup() const
{
    Q_D(const QQmlDelegateModel);
    return d->m_filterGroup;
}

void QQmlDelegateModel::setFilterGroup(const QString &group)
{
    Q_D(QQmlDelegateModel);

    if (d->m_transaction) {
        qmlInfo(this) << tr("The group of a VisualDataModel cannot be changed within onChanged");
        return;
    }

    if (d->m_filterGroup != group) {
        d->m_filterGroup = group;
        d->updateFilterGroup();
        emit filterGroupChanged();
    }
}

void QQmlDelegateModel::resetFilterGroup()
{
    setFilterGroup(QStringLiteral("items"));
}

void QQmlDelegateModelPrivate::updateFilterGroup()
{
    Q_Q(QQmlDelegateModel);
    if (!m_cacheMetaType)
        return;

    QQmlListCompositor::Group previousGroup = m_compositorGroup;
    m_compositorGroup = Compositor::Default;
    for (int i = 1; i < m_groupCount; ++i) {
        if (m_filterGroup == m_cacheMetaType->groupNames.at(i - 1)) {
            m_compositorGroup = Compositor::Group(i);
            break;
        }
    }

    QQmlDataGroupPrivate::get(m_groups[m_compositorGroup])->emitters.insert(this);
    if (m_compositorGroup != previousGroup) {
        QVector<QQmlChangeSet::Remove> removes;
        QVector<QQmlChangeSet::Insert> inserts;
        m_compositor.transition(previousGroup, m_compositorGroup, &removes, &inserts);

        QQmlChangeSet changeSet;
        changeSet.move(removes, inserts);
        emit q->modelUpdated(changeSet, false);

        if (changeSet.difference() != 0)
            emit q->countChanged();

        if (m_parts) {
            foreach (QQmlPartsModel *model, m_parts->models)
                model->updateFilterGroup(m_compositorGroup, changeSet);
        }
    }
}

/*!
    \qmlproperty object QtQuick2::VisualDataModel::parts

    The \a parts property selects a VisualDataModel which creates
    delegates from the part named.  This is used in conjunction with
    the \l Package type.

    For example, the code below selects a model which creates
    delegates named \e list from a \l Package:

    \code
    VisualDataModel {
        id: visualModel
        delegate: Package {
            Item { Package.name: "list" }
        }
        model: myModel
    }

    ListView {
        width: 200; height:200
        model: visualModel.parts.list
    }
    \endcode

    \sa Package
*/

QObject *QQmlDelegateModel::parts()
{
    Q_D(QQmlDelegateModel);
    if (!d->m_parts)
        d->m_parts = new QQmlDelegateModelParts(this);
    return d->m_parts;
}

void QQmlDelegateModelPrivate::emitCreatedPackage(QQDMIncubationTask *incubationTask, QQuickPackage *package)
{
    for (int i = 1; i < m_groupCount; ++i)
        QQmlDataGroupPrivate::get(m_groups[i])->createdPackage(incubationTask->index[i], package);
}

void QQmlDelegateModelPrivate::emitInitPackage(QQDMIncubationTask *incubationTask, QQuickPackage *package)
{
    for (int i = 1; i < m_groupCount; ++i)
        QQmlDataGroupPrivate::get(m_groups[i])->initPackage(incubationTask->index[i], package);
}

void QQmlDelegateModelPrivate::emitDestroyingPackage(QQuickPackage *package)
{
    for (int i = 1; i < m_groupCount; ++i)
        QQmlDataGroupPrivate::get(m_groups[i])->destroyingPackage(package);
}

void QQDMIncubationTask::statusChanged(Status status)
{
    vdm->incubatorStatusChanged(this, status);
}

void QQmlDelegateModelPrivate::releaseIncubator(QQDMIncubationTask *incubationTask)
{
    Q_Q(QQmlDelegateModel);
    if (!incubationTask->isError())
        incubationTask->clear();
    m_finishedIncubating.append(incubationTask);
    if (!m_incubatorCleanupScheduled) {
        m_incubatorCleanupScheduled = true;
        QCoreApplication::postEvent(q, new QEvent(QEvent::User));
    }
}

void QQmlDelegateModelPrivate::removeCacheItem(QQmlDelegateModelItem *cacheItem)
{
    int cidx = m_cache.indexOf(cacheItem);
    if (cidx >= 0) {
        m_compositor.clearFlags(Compositor::Cache, cidx, 1, Compositor::CacheFlag);
        m_cache.removeAt(cidx);
    }
    Q_ASSERT(m_cache.count() == m_compositor.count(Compositor::Cache));
}

void QQmlDelegateModelPrivate::incubatorStatusChanged(QQDMIncubationTask *incubationTask, QQmlIncubator::Status status)
{
    Q_Q(QQmlDelegateModel);
    if (status != QQmlIncubator::Ready && status != QQmlIncubator::Error)
        return;

    QQmlDelegateModelItem *cacheItem = incubationTask->incubating;
    cacheItem->incubationTask = 0;
    incubationTask->incubating = 0;
    releaseIncubator(incubationTask);

    if (status == QQmlIncubator::Ready) {
        if (QQuickPackage *package = qmlobject_cast<QQuickPackage *>(cacheItem->object))
            emitCreatedPackage(incubationTask, package);
        else
            emitCreatedItem(incubationTask, cacheItem->object);
    } else if (status == QQmlIncubator::Error) {
        qmlInfo(q, m_delegate->errors()) << "Error creating delegate";
    }

    if (!cacheItem->isObjectReferenced()) {
        if (QQuickPackage *package = qmlobject_cast<QQuickPackage *>(cacheItem->object))
            emitDestroyingPackage(package);
        else
            emitDestroyingItem(cacheItem->object);
        delete cacheItem->object;
        cacheItem->object = 0;
        cacheItem->scriptRef -= 1;
        cacheItem->contextData->destroy();
        cacheItem->contextData = 0;
        if (!cacheItem->isReferenced()) {
            removeCacheItem(cacheItem);
            delete cacheItem;
        }
    }
}

void QQDMIncubationTask::setInitialState(QObject *o)
{
    vdm->setInitialState(this, o);
}

void QQmlDelegateModelPrivate::setInitialState(QQDMIncubationTask *incubationTask, QObject *o)
{
    QQmlDelegateModelItem *cacheItem = incubationTask->incubating;
    cacheItem->object = o;

    if (QQuickPackage *package = qmlobject_cast<QQuickPackage *>(cacheItem->object))
        emitInitPackage(incubationTask, package);
    else
        emitInitItem(incubationTask, cacheItem->object);
}

QObject *QQmlDelegateModelPrivate::object(Compositor::Group group, int index, bool asynchronous)
{
    Q_Q(QQmlDelegateModel);
    if (!m_delegate || index < 0 || index >= m_compositor.count(group)) {
        qWarning() << "VisualDataModel::item: index out range" << index << m_compositor.count(group);
        return 0;
    } else if (!m_context->isValid()) {
        return 0;
    }

    Compositor::iterator it = m_compositor.find(group, index);

    QQmlDelegateModelItem *cacheItem = it->inCache() ? m_cache.at(it.cacheIndex) : 0;

    if (!cacheItem) {
        cacheItem = m_adaptorModel.createItem(m_cacheMetaType, m_context->engine(), it.modelIndex());
        if (!cacheItem)
            return 0;

        cacheItem->groups = it->flags;

        m_cache.insert(it.cacheIndex, cacheItem);
        m_compositor.setFlags(it, 1, Compositor::CacheFlag);
        Q_ASSERT(m_cache.count() == m_compositor.count(Compositor::Cache));
    }

    // Bump the reference counts temporarily so neither the content data or the delegate object
    // are deleted if incubatorStatusChanged() is called synchronously.
    cacheItem->scriptRef += 1;
    cacheItem->referenceObject();

    if (cacheItem->incubationTask) {
        if (!asynchronous && cacheItem->incubationTask->incubationMode() == QQmlIncubator::Asynchronous) {
            // previously requested async - now needed immediately
            cacheItem->incubationTask->forceCompletion();
        }
    } else if (!cacheItem->object) {
        QQmlContext *creationContext = m_delegate->creationContext();

        cacheItem->scriptRef += 1;

        cacheItem->incubationTask = new QQDMIncubationTask(this, asynchronous ? QQmlIncubator::Asynchronous : QQmlIncubator::AsynchronousIfNested);
        cacheItem->incubationTask->incubating = cacheItem;
        cacheItem->incubationTask->clear();

        for (int i = 1; i < m_groupCount; ++i)
            cacheItem->incubationTask->index[i] = it.index[i];

        QQmlContextData *ctxt = new QQmlContextData;
        ctxt->setParent(QQmlContextData::get(creationContext  ? creationContext : m_context));
        ctxt->contextObject = cacheItem;
        cacheItem->contextData = ctxt;

        if (m_adaptorModel.hasProxyObject()) {
            if (QQmlAdaptorModelProxyInterface *proxy
                    = qobject_cast<QQmlAdaptorModelProxyInterface *>(cacheItem)) {
                ctxt = new QQmlContextData;
                ctxt->setParent(cacheItem->contextData, true);
                ctxt->contextObject = proxy->proxiedObject();
            }
        }

        cacheItem->incubateObject(
                    m_delegate,
                    m_context->engine(),
                    ctxt,
                    QQmlContextData::get(m_context));
    }

    if (index == m_compositor.count(group) - 1 && m_adaptorModel.canFetchMore())
        QCoreApplication::postEvent(q, new QEvent(QEvent::UpdateRequest));

    // Remove the temporary reference count.
    cacheItem->scriptRef -= 1;
    if (cacheItem->object)
        return cacheItem->object;

    cacheItem->releaseObject();
    if (!cacheItem->isReferenced()) {
        removeCacheItem(cacheItem);
        delete cacheItem;
    }

    return 0;
}

/*
  If asynchronous is true or the component is being loaded asynchronously due
  to an ancestor being loaded asynchronously, item() may return 0.  In this
  case itemCreated() will be emitted when the item is available.  The item
  at this stage does not have any references, so item() must be called again
  to ensure a reference is held.  Any call to item() which returns a valid item
  must be matched by a call to release() in order to destroy the item.
*/
QObject *QQmlDelegateModel::object(int index, bool asynchronous)
{
    Q_D(QQmlDelegateModel);
    if (!d->m_delegate || index < 0 || index >= d->m_compositor.count(d->m_compositorGroup)) {
        qWarning() << "VisualDataModel::item: index out range" << index << d->m_compositor.count(d->m_compositorGroup);
        return 0;
    }

    QObject *object = d->object(d->m_compositorGroup, index, asynchronous);
    if (!object)
        return 0;

    return object;
}

QString QQmlDelegateModelPrivate::stringValue(Compositor::Group group, int index, const QString &name)
{
    Compositor::iterator it = m_compositor.find(group, index);
    if (QQmlAdaptorModel *model = it.list<QQmlAdaptorModel>()) {
        QString role = name;
        int dot = name.indexOf(QLatin1Char('.'));
        if (dot > 0)
            role = name.left(dot);
        QVariant value = model->value(it.modelIndex(), role);
        while (dot > 0) {
            QObject *obj = qvariant_cast<QObject*>(value);
            if (!obj)
                return QString();
            int from = dot+1;
            dot = name.indexOf(QLatin1Char('.'), from);
            value = obj->property(name.mid(from, dot-from).toUtf8());
        }
        return value.toString();
    }
    return QString();
}

QString QQmlDelegateModel::stringValue(int index, const QString &name)
{
    Q_D(QQmlDelegateModel);
    return d->stringValue(d->m_compositorGroup, index, name);
}

int QQmlDelegateModel::indexOf(QObject *item, QObject *) const
{
    Q_D(const QQmlDelegateModel);
    if (QQmlDelegateModelItem *cacheItem = QQmlDelegateModelItem::dataForObject(item))
        return cacheItem->groupIndex(d->m_compositorGroup);
    return -1;
}

void QQmlDelegateModel::setWatchedRoles(QList<QByteArray> roles)
{
    Q_D(QQmlDelegateModel);
    d->m_adaptorModel.replaceWatchedRoles(d->m_watchedRoles, roles);
    d->m_watchedRoles = roles;
}

void QQmlDelegateModelPrivate::addGroups(
        Compositor::iterator from, int count, Compositor::Group group, int groupFlags)
{
    QVector<Compositor::Insert> inserts;
    m_compositor.setFlags(from, count, group, groupFlags, &inserts);
    itemsInserted(inserts);
    emitChanges();
}

void QQmlDelegateModelPrivate::removeGroups(
        Compositor::iterator from, int count, Compositor::Group group, int groupFlags)
{
    QVector<Compositor::Remove> removes;
    m_compositor.clearFlags(from, count, group, groupFlags, &removes);
    itemsRemoved(removes);
    emitChanges();
}

void QQmlDelegateModelPrivate::setGroups(
        Compositor::iterator from, int count, Compositor::Group group, int groupFlags)
{
    QVector<Compositor::Remove> removes;
    QVector<Compositor::Insert> inserts;

    m_compositor.setFlags(from, count, group, groupFlags, &inserts);
    itemsInserted(inserts);
    const int removeFlags = ~groupFlags & Compositor::GroupMask;

    from = m_compositor.find(from.group, from.index[from.group]);
    m_compositor.clearFlags(from, count, group, removeFlags, &removes);
    itemsRemoved(removes);
    emitChanges();
}

bool QQmlDelegateModel::event(QEvent *e)
{
    Q_D(QQmlDelegateModel);
    if (e->type() == QEvent::UpdateRequest) {
        d->m_adaptorModel.fetchMore();
    } else if (e->type() == QEvent::User) {
        d->m_incubatorCleanupScheduled = false;
        qDeleteAll(d->m_finishedIncubating);
        d->m_finishedIncubating.clear();
    }
    return QQmlInstanceModel::event(e);
}

void QQmlDelegateModelPrivate::itemsChanged(const QVector<Compositor::Change> &changes)
{
    if (!m_delegate)
        return;

    QVarLengthArray<QVector<QQmlChangeSet::Change>, Compositor::MaximumGroupCount> translatedChanges(m_groupCount);

    foreach (const Compositor::Change &change, changes) {
        for (int i = 1; i < m_groupCount; ++i) {
            if (change.inGroup(i)) {
                translatedChanges[i].append(QQmlChangeSet::Change(change.index[i], change.count));
            }
        }
    }

    for (int i = 1; i < m_groupCount; ++i)
        QQmlDataGroupPrivate::get(m_groups[i])->changeSet.change(translatedChanges.at(i));
}

void QQmlDelegateModel::_q_itemsChanged(int index, int count, const QVector<int> &roles)
{
    Q_D(QQmlDelegateModel);
    if (count <= 0 || !d->m_complete)
        return;

    if (d->m_adaptorModel.notify(d->m_cache, index, count, roles)) {
        QVector<Compositor::Change> changes;
        d->m_compositor.listItemsChanged(&d->m_adaptorModel, index, count, &changes);
        d->itemsChanged(changes);
        d->emitChanges();
    }
}

static void incrementIndexes(QQmlDelegateModelItem *cacheItem, int count, const int *deltas)
{
    if (QQDMIncubationTask *incubationTask = cacheItem->incubationTask) {
        for (int i = 1; i < count; ++i)
            incubationTask->index[i] += deltas[i];
    }
    if (QQmlDelegateModelAttached *attached = cacheItem->attached) {
        for (int i = 1; i < count; ++i)
            attached->m_currentIndex[i] += deltas[i];
    }
}

void QQmlDelegateModelPrivate::itemsInserted(
        const QVector<Compositor::Insert> &inserts,
        QVarLengthArray<QVector<QQmlChangeSet::Insert>, Compositor::MaximumGroupCount> *translatedInserts,
        QHash<int, QList<QQmlDelegateModelItem *> > *movedItems)
{
    int cacheIndex = 0;

    int inserted[Compositor::MaximumGroupCount];
    for (int i = 1; i < m_groupCount; ++i)
        inserted[i] = 0;

    foreach (const Compositor::Insert &insert, inserts) {
        for (; cacheIndex < insert.cacheIndex; ++cacheIndex)
            incrementIndexes(m_cache.at(cacheIndex), m_groupCount, inserted);

        for (int i = 1; i < m_groupCount; ++i) {
            if (insert.inGroup(i)) {
                (*translatedInserts)[i].append(
                        QQmlChangeSet::Insert(insert.index[i], insert.count, insert.moveId));
                inserted[i] += insert.count;
            }
        }

        if (!insert.inCache())
            continue;

        if (movedItems && insert.isMove()) {
            QList<QQmlDelegateModelItem *> items = movedItems->take(insert.moveId);
            Q_ASSERT(items.count() == insert.count);
            m_cache = m_cache.mid(0, insert.cacheIndex) + items + m_cache.mid(insert.cacheIndex);
        }
        if (insert.inGroup()) {
            for (int offset = 0; cacheIndex < insert.cacheIndex + insert.count; ++cacheIndex, ++offset) {
                QQmlDelegateModelItem *cacheItem = m_cache.at(cacheIndex);
                cacheItem->groups |= insert.flags & Compositor::GroupMask;

                if (QQDMIncubationTask *incubationTask = cacheItem->incubationTask) {
                    for (int i = 1; i < m_groupCount; ++i)
                        incubationTask->index[i] = cacheItem->groups & (1 << i)
                                ? insert.index[i] + offset
                                : insert.index[i];
                }
                if (QQmlDelegateModelAttached *attached = cacheItem->attached) {
                    for (int i = 1; i < m_groupCount; ++i)
                        attached->m_currentIndex[i] = cacheItem->groups & (1 << i)
                                ? insert.index[i] + offset
                                : insert.index[i];
                }
            }
        } else {
            cacheIndex = insert.cacheIndex + insert.count;
        }
    }
    for (; cacheIndex < m_cache.count(); ++cacheIndex)
        incrementIndexes(m_cache.at(cacheIndex), m_groupCount, inserted);
}

void QQmlDelegateModelPrivate::itemsInserted(const QVector<Compositor::Insert> &inserts)
{
    QVarLengthArray<QVector<QQmlChangeSet::Insert>, Compositor::MaximumGroupCount> translatedInserts(m_groupCount);
    itemsInserted(inserts, &translatedInserts);
    Q_ASSERT(m_cache.count() == m_compositor.count(Compositor::Cache));
    if (!m_delegate)
        return;

    for (int i = 1; i < m_groupCount; ++i)
        QQmlDataGroupPrivate::get(m_groups[i])->changeSet.insert(translatedInserts.at(i));
}

void QQmlDelegateModel::_q_itemsInserted(int index, int count)
{

    Q_D(QQmlDelegateModel);
    if (count <= 0 || !d->m_complete)
        return;

    d->m_count += count;

    for (int i = 0, c = d->m_cache.count();  i < c; ++i) {
        QQmlDelegateModelItem *item = d->m_cache.at(i);
        if (item->modelIndex() >= index)
            item->setModelIndex(item->modelIndex() + count);
    }

    QVector<Compositor::Insert> inserts;
    d->m_compositor.listItemsInserted(&d->m_adaptorModel, index, count, &inserts);
    d->itemsInserted(inserts);
    d->emitChanges();
}

void QQmlDelegateModelPrivate::itemsRemoved(
        const QVector<Compositor::Remove> &removes,
        QVarLengthArray<QVector<QQmlChangeSet::Remove>, Compositor::MaximumGroupCount> *translatedRemoves,
        QHash<int, QList<QQmlDelegateModelItem *> > *movedItems)
{
    int cacheIndex = 0;
    int removedCache = 0;

    int removed[Compositor::MaximumGroupCount];
    for (int i = 1; i < m_groupCount; ++i)
        removed[i] = 0;

    foreach (const Compositor::Remove &remove, removes) {
        for (; cacheIndex < remove.cacheIndex; ++cacheIndex)
            incrementIndexes(m_cache.at(cacheIndex), m_groupCount, removed);

        for (int i = 1; i < m_groupCount; ++i) {
            if (remove.inGroup(i)) {
                (*translatedRemoves)[i].append(
                        QQmlChangeSet::Remove(remove.index[i], remove.count, remove.moveId));
                removed[i] -= remove.count;
            }
        }

        if (!remove.inCache())
            continue;

        if (movedItems && remove.isMove()) {
            movedItems->insert(remove.moveId, m_cache.mid(remove.cacheIndex, remove.count));
            QList<QQmlDelegateModelItem *>::iterator begin = m_cache.begin() + remove.cacheIndex;
            QList<QQmlDelegateModelItem *>::iterator end = begin + remove.count;
            m_cache.erase(begin, end);
        } else {
            for (; cacheIndex < remove.cacheIndex + remove.count - removedCache; ++cacheIndex) {
                QQmlDelegateModelItem *cacheItem = m_cache.at(cacheIndex);
                if (remove.inGroup(Compositor::Persisted) && cacheItem->objectRef == 0 && cacheItem->object) {
                    QObject *object = cacheItem->object;
                    cacheItem->destroyObject();
                    if (QQuickPackage *package = qmlobject_cast<QQuickPackage *>(object))
                        emitDestroyingPackage(package);
                    else
                        emitDestroyingItem(object);
                    cacheItem->scriptRef -= 1;
                }
                if (!cacheItem->isReferenced()) {
                    m_compositor.clearFlags(Compositor::Cache, cacheIndex, 1, Compositor::CacheFlag);
                    m_cache.removeAt(cacheIndex);
                    delete cacheItem;
                    --cacheIndex;
                    ++removedCache;
                    Q_ASSERT(m_cache.count() == m_compositor.count(Compositor::Cache));
                } else if (remove.groups() == cacheItem->groups) {
                    cacheItem->groups = 0;
                    if (QQDMIncubationTask *incubationTask = cacheItem->incubationTask) {
                        for (int i = 1; i < m_groupCount; ++i)
                            incubationTask->index[i] = -1;
                    }
                    if (QQmlDelegateModelAttached *attached = cacheItem->attached) {
                        for (int i = 1; i < m_groupCount; ++i)
                            attached->m_currentIndex[i] = -1;
                    }
                } else {
                    if (QQDMIncubationTask *incubationTask = cacheItem->incubationTask) {
                        for (int i = 1; i < m_groupCount; ++i) {
                            if (remove.inGroup(i))
                                incubationTask->index[i] = remove.index[i];
                        }
                    }
                    if (QQmlDelegateModelAttached *attached = cacheItem->attached) {
                        for (int i = 1; i < m_groupCount; ++i) {
                            if (remove.inGroup(i))
                                attached->m_currentIndex[i] = remove.index[i];
                        }
                    }
                    cacheItem->groups &= ~remove.flags;
                }
            }
        }
    }

    for (; cacheIndex < m_cache.count(); ++cacheIndex)
        incrementIndexes(m_cache.at(cacheIndex), m_groupCount, removed);
}

void QQmlDelegateModelPrivate::itemsRemoved(const QVector<Compositor::Remove> &removes)
{
    QVarLengthArray<QVector<QQmlChangeSet::Remove>, Compositor::MaximumGroupCount> translatedRemoves(m_groupCount);
    itemsRemoved(removes, &translatedRemoves);
    Q_ASSERT(m_cache.count() == m_compositor.count(Compositor::Cache));
    if (!m_delegate)
        return;

    for (int i = 1; i < m_groupCount; ++i)
       QQmlDataGroupPrivate::get(m_groups[i])->changeSet.remove(translatedRemoves.at(i));
}

void QQmlDelegateModel::_q_itemsRemoved(int index, int count)
{
    Q_D(QQmlDelegateModel);
    if (count <= 0|| !d->m_complete)
        return;

    d->m_count -= count;

    for (int i = 0, c = d->m_cache.count();  i < c; ++i) {
        QQmlDelegateModelItem *item = d->m_cache.at(i);
        if (item->modelIndex() >= index + count)
            item->setModelIndex(item->modelIndex() - count);
        else  if (item->modelIndex() >= index)
            item->setModelIndex(-1);
    }

    QVector<Compositor::Remove> removes;
    d->m_compositor.listItemsRemoved(&d->m_adaptorModel, index, count, &removes);
    d->itemsRemoved(removes);

    d->emitChanges();
}

void QQmlDelegateModelPrivate::itemsMoved(
        const QVector<Compositor::Remove> &removes, const QVector<Compositor::Insert> &inserts)
{
    QHash<int, QList<QQmlDelegateModelItem *> > movedItems;

    QVarLengthArray<QVector<QQmlChangeSet::Remove>, Compositor::MaximumGroupCount> translatedRemoves(m_groupCount);
    itemsRemoved(removes, &translatedRemoves, &movedItems);

    QVarLengthArray<QVector<QQmlChangeSet::Insert>, Compositor::MaximumGroupCount> translatedInserts(m_groupCount);
    itemsInserted(inserts, &translatedInserts, &movedItems);
    Q_ASSERT(m_cache.count() == m_compositor.count(Compositor::Cache));
    Q_ASSERT(movedItems.isEmpty());
    if (!m_delegate)
        return;

    for (int i = 1; i < m_groupCount; ++i) {
        QQmlDataGroupPrivate::get(m_groups[i])->changeSet.move(
                    translatedRemoves.at(i),
                    translatedInserts.at(i));
    }
}

void QQmlDelegateModel::_q_itemsMoved(int from, int to, int count)
{
    Q_D(QQmlDelegateModel);
    if (count <= 0 || !d->m_complete)
        return;

    const int minimum = qMin(from, to);
    const int maximum = qMax(from, to) + count;
    const int difference = from > to ? count : -count;

    for (int i = 0, c = d->m_cache.count();  i < c; ++i) {
        QQmlDelegateModelItem *item = d->m_cache.at(i);
        if (item->modelIndex() >= from && item->modelIndex() < from + count)
            item->setModelIndex(item->modelIndex() - from + to);
        else if (item->modelIndex() >= minimum && item->modelIndex() < maximum)
            item->setModelIndex(item->modelIndex() + difference);
    }

    QVector<Compositor::Remove> removes;
    QVector<Compositor::Insert> inserts;
    d->m_compositor.listItemsMoved(&d->m_adaptorModel, from, to, count, &removes, &inserts);
    d->itemsMoved(removes, inserts);
    d->emitChanges();
}

template <typename T> v8::Local<v8::Array>
QQmlDelegateModelPrivate::buildChangeList(const QVector<T> &changes)
{
    v8::Local<v8::Array> indexes = v8::Array::New(changes.count());
    v8::Local<v8::String> indexKey = v8::String::New("index");
    v8::Local<v8::String> countKey = v8::String::New("count");
    v8::Local<v8::String> moveIdKey = v8::String::New("moveId");

    for (int i = 0; i < changes.count(); ++i) {
        v8::Local<v8::Object> object = v8::Object::New();
        object->Set(indexKey, v8::Integer::New(changes.at(i).index));
        object->Set(countKey, v8::Integer::New(changes.at(i).count));
        object->Set(moveIdKey, changes.at(i).moveId != -1 ? v8::Integer::New(changes.at(i).count) : v8::Undefined());
        indexes->Set(i, object);
    }
    return indexes;
}

void QQmlDelegateModelPrivate::emitModelUpdated(const QQmlChangeSet &changeSet, bool reset)
{
    Q_Q(QQmlDelegateModel);
    emit q->modelUpdated(changeSet, reset);
    if (changeSet.difference() != 0)
        emit q->countChanged();
}

void QQmlDelegateModelPrivate::emitChanges()
{
    if (m_transaction || !m_complete || !m_context->isValid())
        return;

    m_transaction = true;
    QV8Engine *engine = QQmlEnginePrivate::getV8Engine(m_context->engine());
    for (int i = 1; i < m_groupCount; ++i)
        QQmlDataGroupPrivate::get(m_groups[i])->emitChanges(engine);
    m_transaction = false;

    const bool reset = m_reset;
    m_reset = false;
    for (int i = 1; i < m_groupCount; ++i)
        QQmlDataGroupPrivate::get(m_groups[i])->emitModelUpdated(reset);

    foreach (QQmlDelegateModelItem *cacheItem, m_cache) {
        if (cacheItem->attached)
            cacheItem->attached->emitChanges();
    }
}

void QQmlDelegateModel::_q_modelReset()
{
    Q_D(QQmlDelegateModel);
    if (!d->m_delegate)
        return;

    int oldCount = d->m_count;
    d->m_adaptorModel.rootIndex = QModelIndex();

    if (d->m_complete) {
        d->m_count = d->m_adaptorModel.count();

        for (int i = 0, c = d->m_cache.count();  i < c; ++i) {
            QQmlDelegateModelItem *item = d->m_cache.at(i);
            if (item->modelIndex() != -1)
                item->setModelIndex(-1);
        }

        QVector<Compositor::Remove> removes;
        QVector<Compositor::Insert> inserts;
        if (oldCount)
            d->m_compositor.listItemsRemoved(&d->m_adaptorModel, 0, oldCount, &removes);
        if (d->m_count)
            d->m_compositor.listItemsInserted(&d->m_adaptorModel, 0, d->m_count, &inserts);
        d->itemsMoved(removes, inserts);
        d->m_reset = true;

        if (d->m_adaptorModel.canFetchMore())
            d->m_adaptorModel.fetchMore();

        d->emitChanges();
    }
    emit rootIndexChanged();
}

void QQmlDelegateModel::_q_rowsInserted(const QModelIndex &parent, int begin, int end)
{
    Q_D(QQmlDelegateModel);
    if (parent == d->m_adaptorModel.rootIndex)
        _q_itemsInserted(begin, end - begin + 1);
}

void QQmlDelegateModel::_q_rowsAboutToBeRemoved(const QModelIndex &parent, int begin, int end)
{
    Q_D(QQmlDelegateModel);
    if (!d->m_adaptorModel.rootIndex.isValid())
        return;
    const QModelIndex index = d->m_adaptorModel.rootIndex;
    if (index.parent() == parent && index.row() >= begin && index.row() <= end) {
        const int oldCount = d->m_count;
        d->m_count = 0;
        d->m_adaptorModel.invalidateModel(this);

        if (d->m_complete && oldCount > 0) {
            QVector<Compositor::Remove> removes;
            d->m_compositor.listItemsRemoved(&d->m_adaptorModel, 0, oldCount, &removes);
            d->itemsRemoved(removes);
            d->emitChanges();
        }
    }
}

void QQmlDelegateModel::_q_rowsRemoved(const QModelIndex &parent, int begin, int end)
{
    Q_D(QQmlDelegateModel);
    if (parent == d->m_adaptorModel.rootIndex)
        _q_itemsRemoved(begin, end - begin + 1);
}

void QQmlDelegateModel::_q_rowsMoved(
        const QModelIndex &sourceParent, int sourceStart, int sourceEnd,
        const QModelIndex &destinationParent, int destinationRow)
{
   Q_D(QQmlDelegateModel);
    const int count = sourceEnd - sourceStart + 1;
    if (destinationParent == d->m_adaptorModel.rootIndex && sourceParent == d->m_adaptorModel.rootIndex) {
        _q_itemsMoved(sourceStart, sourceStart > destinationRow ? destinationRow : destinationRow - count, count);
    } else if (sourceParent == d->m_adaptorModel.rootIndex) {
        _q_itemsRemoved(sourceStart, count);
    } else if (destinationParent == d->m_adaptorModel.rootIndex) {
        _q_itemsInserted(destinationRow, count);
    }
}

void QQmlDelegateModel::_q_dataChanged(const QModelIndex &begin, const QModelIndex &end, const QVector<int> &roles)
{
    Q_D(QQmlDelegateModel);
    if (begin.parent() == d->m_adaptorModel.rootIndex)
        _q_itemsChanged(begin.row(), end.row() - begin.row() + 1, roles);
}

void QQmlDelegateModel::_q_layoutChanged()
{
    Q_D(QQmlDelegateModel);
    _q_itemsChanged(0, d->m_count, QVector<int>());
}

QQmlDelegateModelAttached *QQmlDelegateModel::qmlAttachedProperties(QObject *obj)
{
    if (QQmlDelegateModelItem *cacheItem = QQmlDelegateModelItem::dataForObject(obj)) {
        if (cacheItem->object == obj) { // Don't create attached item for child objects.
            cacheItem->attached = new QQmlDelegateModelAttached(cacheItem, obj);
            return cacheItem->attached;
        }
    }
    return new QQmlDelegateModelAttached(obj);
}

bool QQmlDelegateModelPrivate::insert(
        Compositor::insert_iterator &before, const v8::Local<v8::Object> &object, int groups)
{
    if (!m_context->isValid())
        return false;

    QQmlDelegateModelItem *cacheItem = m_adaptorModel.createItem(m_cacheMetaType, m_context->engine(), -1);
    if (!cacheItem)
        return false;

    v8::Local<v8::Array> propertyNames = object->GetPropertyNames();
    for (uint i = 0; i < propertyNames->Length(); ++i) {
        v8::Local<v8::String> propertyName = propertyNames->Get(i)->ToString();
        cacheItem->setValue(
                m_cacheMetaType->v8Engine->toString(propertyName),
                m_cacheMetaType->v8Engine->toVariant(object->Get(propertyName), QVariant::Invalid));
    }

    cacheItem->groups = groups | Compositor::UnresolvedFlag | Compositor::CacheFlag;

    // Must be before the new object is inserted into the cache or its indexes will be adjusted too.
    itemsInserted(QVector<Compositor::Insert>() << Compositor::Insert(before, 1, cacheItem->groups & ~Compositor::CacheFlag));

    before = m_compositor.insert(before, 0, 0, 1, cacheItem->groups);
    m_cache.insert(before.cacheIndex, cacheItem);

    return true;
}

//============================================================================

QQmlDelegateModelItemMetaType::QQmlDelegateModelItemMetaType(
        QV8Engine *engine, QQmlDelegateModel *model, const QStringList &groupNames)
    : model(model)
    , groupCount(groupNames.count() + 1)
    , v8Engine(engine)
    , metaObject(0)
    , groupNames(groupNames)
{
}

QQmlDelegateModelItemMetaType::~QQmlDelegateModelItemMetaType()
{
    if (metaObject)
        metaObject->release();
    qPersistentDispose(constructor);
}

void QQmlDelegateModelItemMetaType::initializeMetaObject()
{
    QMetaObjectBuilder builder;
    builder.setFlags(QMetaObjectBuilder::DynamicMetaObject);
    builder.setClassName(QQmlDelegateModelAttached::staticMetaObject.className());
    builder.setSuperClass(&QQmlDelegateModelAttached::staticMetaObject);

    int notifierId = 0;
    for (int i = 0; i < groupNames.count(); ++i, ++notifierId) {
        QString propertyName = QStringLiteral("in") + groupNames.at(i);
        propertyName.replace(2, 1, propertyName.at(2).toUpper());
        builder.addSignal("__" + propertyName.toUtf8() + "Changed()");
        QMetaPropertyBuilder propertyBuilder = builder.addProperty(
                propertyName.toUtf8(), "bool", notifierId);
        propertyBuilder.setWritable(true);
    }
    for (int i = 0; i < groupNames.count(); ++i, ++notifierId) {
        const QString propertyName = groupNames.at(i) + QStringLiteral("Index");
        builder.addSignal("__" + propertyName.toUtf8() + "Changed()");
        QMetaPropertyBuilder propertyBuilder = builder.addProperty(
                propertyName.toUtf8(), "int", notifierId);
        propertyBuilder.setWritable(true);
    }

    metaObject = new QQmlDelegateModelAttachedMetaObject(this, builder.toMetaObject());
}

void QQmlDelegateModelItemMetaType::initializeConstructor()
{
    v8::HandleScope handleScope;
    v8::Context::Scope contextScope(v8Engine->context());

    QQmlDelegateModelEngineData *data = engineData(v8Engine);

    constructor = qPersistentNew(v8::ObjectTemplate::New());

    constructor->SetHasExternalResource(true);
    constructor->SetAccessor(data->model(), get_model);
    constructor->SetAccessor(data->groups(), get_groups, set_groups);
    constructor->SetAccessor(data->isUnresolved(), get_member, 0, v8::Int32::New(30));
    constructor->SetAccessor(data->inItems(), get_member, set_member, v8::Int32::New(1));
    constructor->SetAccessor(data->inPersistedItems(), get_member, set_member, v8::Int32::New(2));
    constructor->SetAccessor(data->itemsIndex(), get_index, 0, v8::Int32::New(1));
    constructor->SetAccessor(data->persistedItemsIndex(), get_index, 0, v8::Int32::New(2));

    for (int i = 2; i < groupNames.count(); ++i) {
        QString propertyName = QStringLiteral("in") + groupNames.at(i);
        propertyName.replace(2, 1, propertyName.at(2).toUpper());
        constructor->SetAccessor(
                v8Engine->toString(propertyName), get_member, set_member, v8::Int32::New(i + 1));
    }
    for (int i = 2; i < groupNames.count(); ++i) {
        const QString propertyName = groupNames.at(i) + QStringLiteral("Index");
        constructor->SetAccessor(
                v8Engine->toString(propertyName), get_index, 0, v8::Int32::New(i + 1));
    }
}

int QQmlDelegateModelItemMetaType::parseGroups(const QStringList &groups) const
{
    int groupFlags = 0;
    foreach (const QString &groupName, groups) {
        int index = groupNames.indexOf(groupName);
        if (index != -1)
            groupFlags |= 2 << index;
    }
    return groupFlags;
}

int QQmlDelegateModelItemMetaType::parseGroups(const v8::Local<v8::Value> &groups) const
{
    int groupFlags = 0;
    if (groups->IsString()) {
        const QString groupName = v8Engine->toString(groups);
        int index = groupNames.indexOf(groupName);
        if (index != -1)
            groupFlags |= 2 << index;
    } else if (groups->IsArray()) {
        v8::Local<v8::Array> array = v8::Local<v8::Array>::Cast(groups);
        for (uint i = 0; i < array->Length(); ++i) {
            const QString groupName = v8Engine->toString(array->Get(i));
            int index = groupNames.indexOf(groupName);
            if (index != -1)
                groupFlags |= 2 << index;
        }
    }
    return groupFlags;
}

v8::Handle<v8::Value> QQmlDelegateModelItemMetaType::get_model(
        v8::Local<v8::String>, const v8::AccessorInfo &info)
{
    QQmlDelegateModelItem *cacheItem = v8_resource_cast<QQmlDelegateModelItem>(info.This());
    V8ASSERT_TYPE(cacheItem, "Not a valid VisualData object");
    if (!cacheItem->metaType->model)
        return v8::Undefined();

    return cacheItem->get();
}

v8::Handle<v8::Value> QQmlDelegateModelItemMetaType::get_groups(
        v8::Local<v8::String>, const v8::AccessorInfo &info)
{
    QQmlDelegateModelItem *cacheItem = v8_resource_cast<QQmlDelegateModelItem>(info.This());
    V8ASSERT_TYPE(cacheItem, "Not a valid VisualData object");

    QStringList groups;
    for (int i = 1; i < cacheItem->metaType->groupCount; ++i) {
        if (cacheItem->groups & (1 << i))
            groups.append(cacheItem->metaType->groupNames.at(i - 1));
    }

    return cacheItem->engine->fromVariant(groups);
}

void QQmlDelegateModelItemMetaType::set_groups(
        v8::Local<v8::String>, v8::Local<v8::Value> value, const v8::AccessorInfo &info)
{
    QQmlDelegateModelItem *cacheItem = v8_resource_cast<QQmlDelegateModelItem>(info.This());
    V8ASSERT_TYPE_SETTER(cacheItem, "Not a valid VisualData object");

    if (!cacheItem->metaType->model)
        return;
    QQmlDelegateModelPrivate *model = QQmlDelegateModelPrivate::get(cacheItem->metaType->model);

    const int groupFlags = model->m_cacheMetaType->parseGroups(value);
    const int cacheIndex = model->m_cache.indexOf(cacheItem);
    Compositor::iterator it = model->m_compositor.find(Compositor::Cache, cacheIndex);
    model->setGroups(it, 1, Compositor::Cache, groupFlags);
}

v8::Handle<v8::Value> QQmlDelegateModelItemMetaType::get_member(
        v8::Local<v8::String>, const v8::AccessorInfo &info)
{
    QQmlDelegateModelItem *cacheItem = v8_resource_cast<QQmlDelegateModelItem>(info.This());
    V8ASSERT_TYPE(cacheItem, "Not a valid VisualData object");

    return v8::Boolean::New(cacheItem->groups & (1 << info.Data()->Int32Value()));
}

void QQmlDelegateModelItemMetaType::set_member(
        v8::Local<v8::String>, v8::Local<v8::Value> value, const v8::AccessorInfo &info)
{
    QQmlDelegateModelItem *cacheItem = v8_resource_cast<QQmlDelegateModelItem>(info.This());
    V8ASSERT_TYPE_SETTER(cacheItem, "Not a valid VisualData object");

    if (!cacheItem->metaType->model)
        return;
    QQmlDelegateModelPrivate *model = QQmlDelegateModelPrivate::get(cacheItem->metaType->model);

    Compositor::Group group = Compositor::Group(info.Data()->Int32Value());
    const bool member = value->BooleanValue();
    const int groupFlag = (1 << group);
    if (member == ((cacheItem->groups & groupFlag) != 0))
        return;

    const int cacheIndex = model->m_cache.indexOf(cacheItem);
    Compositor::iterator it = model->m_compositor.find(Compositor::Cache, cacheIndex);
    if (member)
        model->addGroups(it, 1, Compositor::Cache, groupFlag);
    else
        model->removeGroups(it, 1, Compositor::Cache, groupFlag);
}

v8::Handle<v8::Value> QQmlDelegateModelItemMetaType::get_index(
        v8::Local<v8::String>, const v8::AccessorInfo &info)
{
    QQmlDelegateModelItem *cacheItem = v8_resource_cast<QQmlDelegateModelItem>(info.This());
    V8ASSERT_TYPE(cacheItem, "Not a valid VisualData object");

    return v8::Integer::New(cacheItem->groupIndex(Compositor::Group(info.Data()->Int32Value())));
}


//---------------------------------------------------------------------------

QQmlDelegateModelItem::QQmlDelegateModelItem(
        QQmlDelegateModelItemMetaType *metaType, int modelIndex)
    : QV8ObjectResource(metaType->v8Engine)
    , metaType(metaType)
    , contextData(0)
    , object(0)
    , attached(0)
    , incubationTask(0)
    , objectRef(0)
    , scriptRef(0)
    , groups(0)
    , index(modelIndex)
{
    metaType->addref();
}

QQmlDelegateModelItem::~QQmlDelegateModelItem()
{
    Q_ASSERT(scriptRef == 0);
    Q_ASSERT(objectRef == 0);
    Q_ASSERT(!object);

    if (incubationTask && metaType->model)
        QQmlDelegateModelPrivate::get(metaType->model)->releaseIncubator(incubationTask);

    metaType->release();

}

void QQmlDelegateModelItem::Dispose()
{
    --scriptRef;
    if (isReferenced())
        return;

    if (metaType->model) {
        QQmlDelegateModelPrivate *model = QQmlDelegateModelPrivate::get(metaType->model);
        model->removeCacheItem(this);
    }
    delete this;
}

/*
    This is essentially a copy of QQmlComponent::create(); except it takes the QQmlContextData
    arguments instead of QQmlContext which means we don't have to construct the rather weighty
    wrapper class for every delegate item.
*/
void QQmlDelegateModelItem::incubateObject(
        QQmlComponent *component,
        QQmlEngine *engine,
        QQmlContextData *context,
        QQmlContextData *forContext)
{
    QQmlIncubatorPrivate *incubatorPriv = QQmlIncubatorPrivate::get(incubationTask);
    QQmlEnginePrivate *enginePriv = QQmlEnginePrivate::get(engine);
    QQmlComponentPrivate *componentPriv = QQmlComponentPrivate::get(component);

    incubatorPriv->compiledData = componentPriv->cc;
    incubatorPriv->compiledData->addref();
    incubatorPriv->vme.init(
            context,
            componentPriv->cc,
            componentPriv->start,
            componentPriv->creationContext);

    enginePriv->incubate(*incubationTask, forContext);
}

void QQmlDelegateModelItem::destroyObject()
{
    Q_ASSERT(object);
    Q_ASSERT(contextData);

    QObjectPrivate *p = QObjectPrivate::get(object);
    Q_ASSERT(p->declarativeData);
    QQmlData *data = static_cast<QQmlData*>(p->declarativeData);
    if (data->ownContext && data->context)
        data->context->clearContext();
    object->deleteLater();

    if (attached) {
        attached->m_cacheItem = 0;
        attached = 0;
    }

    contextData->destroy();
    contextData = 0;
    object = 0;
}

QQmlDelegateModelItem *QQmlDelegateModelItem::dataForObject(QObject *object)
{
    QObjectPrivate *p = QObjectPrivate::get(object);
    QQmlContextData *context = p->declarativeData
            ? static_cast<QQmlData *>(p->declarativeData)->context
            : 0;
    for (context = context ? context->parent : 0; context; context = context->parent) {
        if (QQmlDelegateModelItem *cacheItem = qobject_cast<QQmlDelegateModelItem *>(
                context->contextObject)) {
            return cacheItem;
        }
    }
    return 0;
}

int QQmlDelegateModelItem::groupIndex(Compositor::Group group)
{
    if (QQmlDelegateModelPrivate * const model = metaType->model
            ? QQmlDelegateModelPrivate::get(metaType->model)
            : 0) {
        return model->m_compositor.find(Compositor::Cache, model->m_cache.indexOf(this)).index[group];
    }
    return -1;
}

//---------------------------------------------------------------------------

QQmlDelegateModelAttachedMetaObject::QQmlDelegateModelAttachedMetaObject(
        QQmlDelegateModelItemMetaType *metaType, QMetaObject *metaObject)
    : metaType(metaType)
    , metaObject(metaObject)
    , memberPropertyOffset(QQmlDelegateModelAttached::staticMetaObject.propertyCount())
    , indexPropertyOffset(QQmlDelegateModelAttached::staticMetaObject.propertyCount() + metaType->groupNames.count())
{
    // Don't reference count the meta-type here as that would create a circular reference.
    // Instead we rely the fact that the meta-type's reference count can't reach 0 without first
    // destroying all delegates with attached objects.
    *static_cast<QMetaObject *>(this) = *metaObject;
}

QQmlDelegateModelAttachedMetaObject::~QQmlDelegateModelAttachedMetaObject()
{
    ::free(metaObject);
}

void QQmlDelegateModelAttachedMetaObject::objectDestroyed(QObject *)
{
    release();
}

int QQmlDelegateModelAttachedMetaObject::metaCall(QObject *object, QMetaObject::Call call, int _id, void **arguments)
{
    QQmlDelegateModelAttached *attached = static_cast<QQmlDelegateModelAttached *>(object);
    if (call == QMetaObject::ReadProperty) {
        if (_id >= indexPropertyOffset) {
            Compositor::Group group = Compositor::Group(_id - indexPropertyOffset + 1);
            *static_cast<int *>(arguments[0]) = attached->m_currentIndex[group];
            return -1;
        } else if (_id >= memberPropertyOffset) {
            Compositor::Group group = Compositor::Group(_id - memberPropertyOffset + 1);
            *static_cast<bool *>(arguments[0]) = attached->m_cacheItem->groups & (1 << group);
            return -1;
        }
    } else if (call == QMetaObject::WriteProperty) {
        if (_id >= memberPropertyOffset) {
            if (!metaType->model)
                return -1;
            QQmlDelegateModelPrivate *model = QQmlDelegateModelPrivate::get(metaType->model);
            Compositor::Group group = Compositor::Group(_id - memberPropertyOffset + 1);
            const int groupFlag = 1 << group;
            const bool member = attached->m_cacheItem->groups & groupFlag;
            if (member && !*static_cast<bool *>(arguments[0])) {
                Compositor::iterator it = model->m_compositor.find(
                        group, attached->m_currentIndex[group]);
                model->removeGroups(it, 1, group, groupFlag);
            } else if (!member && *static_cast<bool *>(arguments[0])) {
                for (int i = 1; i < metaType->groupCount; ++i) {
                    if (attached->m_cacheItem->groups & (1 << i)) {
                        Compositor::iterator it = model->m_compositor.find(
                                Compositor::Group(i), attached->m_currentIndex[i]);
                        model->addGroups(it, 1, Compositor::Group(i), groupFlag);
                        break;
                    }
                }
            }
            return -1;
        }
    }
    return attached->qt_metacall(call, _id, arguments);
}

QQmlDelegateModelAttached::QQmlDelegateModelAttached(QObject *parent)
    : m_cacheItem(0)
    , m_previousGroups(0)
{
    QQml_setParent_noEvent(this, parent);
}

QQmlDelegateModelAttached::QQmlDelegateModelAttached(
        QQmlDelegateModelItem *cacheItem, QObject *parent)
    : m_cacheItem(cacheItem)
    , m_previousGroups(cacheItem->groups)
{
    QQml_setParent_noEvent(this, parent);
    if (QQDMIncubationTask *incubationTask = m_cacheItem->incubationTask) {
        for (int i = 1; i < m_cacheItem->metaType->groupCount; ++i)
            m_currentIndex[i] = m_previousIndex[i] = incubationTask->index[i];
    } else {
        QQmlDelegateModelPrivate * const model = QQmlDelegateModelPrivate::get(m_cacheItem->metaType->model);
        Compositor::iterator it = model->m_compositor.find(
                Compositor::Cache, model->m_cache.indexOf(m_cacheItem));
        for (int i = 1; i < m_cacheItem->metaType->groupCount; ++i)
            m_currentIndex[i] = m_previousIndex[i] = it.index[i];
    }

    if (!cacheItem->metaType->metaObject)
        cacheItem->metaType->initializeMetaObject();

    QObjectPrivate::get(this)->metaObject = cacheItem->metaType->metaObject;
    cacheItem->metaType->metaObject->addref();
}

/*!
    \qmlattachedproperty int QtQuick2::VisualDataModel::model

    This attached property holds the visual data model this delegate instance belongs to.

    It is attached to each instance of the delegate.
*/

QQmlDelegateModel *QQmlDelegateModelAttached::model() const
{
    return m_cacheItem ? m_cacheItem->metaType->model : 0;
}

/*!
    \qmlattachedproperty stringlist QtQuick2::VisualDataModel::groups

    This attached property holds the name of VisualDataGroups the item belongs to.

    It is attached to each instance of the delegate.
*/

QStringList QQmlDelegateModelAttached::groups() const
{
    QStringList groups;

    if (!m_cacheItem)
        return groups;
    for (int i = 1; i < m_cacheItem->metaType->groupCount; ++i) {
        if (m_cacheItem->groups & (1 << i))
            groups.append(m_cacheItem->metaType->groupNames.at(i - 1));
    }
    return groups;
}

void QQmlDelegateModelAttached::setGroups(const QStringList &groups)
{
    if (!m_cacheItem)
        return;

    QQmlDelegateModelPrivate *model = QQmlDelegateModelPrivate::get(m_cacheItem->metaType->model);

    const int groupFlags = model->m_cacheMetaType->parseGroups(groups);
    const int cacheIndex = model->m_cache.indexOf(m_cacheItem);
    Compositor::iterator it = model->m_compositor.find(Compositor::Cache, cacheIndex);
    model->setGroups(it, 1, Compositor::Cache, groupFlags);
}

/*!
    \qmlattachedproperty bool QtQuick2::VisualDataModel::isUnresolved

    This attached property holds whether the visual item is bound to a data model index.
    Returns true if the item is not bound to the model, and false if it is.

    An unresolved item can be bound to the data model using the VisualDataGroup::resolve()
    function.

    It is attached to each instance of the delegate.
*/

bool QQmlDelegateModelAttached::isUnresolved() const
{
    if (!m_cacheItem)
        return false;

    return m_cacheItem->groups & Compositor::UnresolvedFlag;
}

/*!
    \qmlattachedproperty int QtQuick2::VisualDataModel::inItems

    This attached property holds whether the item belongs to the default \l items VisualDataGroup.

    Changing this property will add or remove the item from the items group.

    It is attached to each instance of the delegate.
*/

/*!
    \qmlattachedproperty int QtQuick2::VisualDataModel::itemsIndex

    This attached property holds the index of the item in the default \l items VisualDataGroup.

    It is attached to each instance of the delegate.
*/

/*!
    \qmlattachedproperty int QtQuick2::VisualDataModel::inPersistedItems

    This attached property holds whether the item belongs to the \l persistedItems VisualDataGroup.

    Changing this property will add or remove the item from the items group.  Change with caution
    as removing an item from the persistedItems group will destroy the current instance if it is
    not referenced by a model.

    It is attached to each instance of the delegate.
*/

/*!
    \qmlattachedproperty int QtQuick2::VisualDataModel::persistedItemsIndex

    This attached property holds the index of the item in the \l persistedItems VisualDataGroup.

    It is attached to each instance of the delegate.
*/

void QQmlDelegateModelAttached::emitChanges()
{
    const int groupChanges = m_previousGroups ^ m_cacheItem->groups;
    m_previousGroups = m_cacheItem->groups;

    int indexChanges = 0;
    for (int i = 1; i < m_cacheItem->metaType->groupCount; ++i) {
        if (m_previousIndex[i] != m_currentIndex[i]) {
            m_previousIndex[i] = m_currentIndex[i];
            indexChanges |= (1 << i);
        }
    }

    int notifierId = 0;
    const QMetaObject *meta = metaObject();
    for (int i = 1; i < m_cacheItem->metaType->groupCount; ++i, ++notifierId) {
        if (groupChanges & (1 << i))
            QMetaObject::activate(this, meta, notifierId, 0);
    }
    for (int i = 1; i < m_cacheItem->metaType->groupCount; ++i, ++notifierId) {
        if (indexChanges & (1 << i))
            QMetaObject::activate(this, meta, notifierId, 0);
    }

    if (groupChanges)
        emit groupsChanged();
}

//============================================================================

void QQmlDataGroupPrivate::setModel(QQmlDelegateModel *m, Compositor::Group g)
{
    Q_ASSERT(!model);
    model = m;
    group = g;
}

bool QQmlDataGroupPrivate::isChangedConnected()
{
    Q_Q(QQmlDataGroup);
    IS_SIGNAL_CONNECTED(q, QQmlDataGroup, changed, (const QQmlV8Handle &,const QQmlV8Handle &));
}

void QQmlDataGroupPrivate::emitChanges(QV8Engine *engine)
{
    Q_Q(QQmlDataGroup);
    if (isChangedConnected() && !changeSet.isEmpty()) {
        v8::HandleScope handleScope;
        v8::Context::Scope contextScope(engine->context());
        v8::Local<v8::Object> removed  = engineData(engine)->array(engine, changeSet.removes());
        v8::Local<v8::Object> inserted = engineData(engine)->array(engine, changeSet.inserts());
        emit q->changed(QQmlV8Handle::fromHandle(removed), QQmlV8Handle::fromHandle(inserted));
    }
    if (changeSet.difference() != 0)
        emit q->countChanged();
}

void QQmlDataGroupPrivate::emitModelUpdated(bool reset)
{
    for (QQmlDataGroupEmitterList::iterator it = emitters.begin(); it != emitters.end(); ++it)
        it->emitModelUpdated(changeSet, reset);
    changeSet.clear();
}

void QQmlDataGroupPrivate::createdPackage(int index, QQuickPackage *package)
{
    for (QQmlDataGroupEmitterList::iterator it = emitters.begin(); it != emitters.end(); ++it)
        it->createdPackage(index, package);
}

void QQmlDataGroupPrivate::initPackage(int index, QQuickPackage *package)
{
    for (QQmlDataGroupEmitterList::iterator it = emitters.begin(); it != emitters.end(); ++it)
        it->initPackage(index, package);
}

void QQmlDataGroupPrivate::destroyingPackage(QQuickPackage *package)
{
    for (QQmlDataGroupEmitterList::iterator it = emitters.begin(); it != emitters.end(); ++it)
        it->destroyingPackage(package);
}

/*!
    \qmltype VisualDataGroup
    \instantiates QQmlDataGroup
    \inqmlmodule QtQuick 2
    \ingroup qtquick-models
    \brief Encapsulates a filtered set of visual data items

    The VisualDataGroup type provides a means to address the model data of a VisualDataModel's
    delegate items, as well as sort and filter these delegate items.

    The initial set of instantiable delegate items in a VisualDataModel is represented
    by its \l {QtQuick2::VisualDataModel::items}{items} group, which normally directly reflects
    the contents of the model assigned to VisualDataModel::model.  This set can be changed to
    the contents of any other member of VisualDataModel::groups by assigning the  \l name of that
    VisualDataGroup to the VisualDataModel::filterOnGroup property.

    The data of an item in a VisualDataGroup can be accessed using the get() function, which returns
    information about group membership and indexes as well as model data.  In combination
    with the move() function this can be used to implement view sorting, with remove() to filter
    items out of a view, or with setGroups() and \l Package delegates to categorize items into
    different views.

    Data from models can be supplemented by inserting data directly into a VisualDataGroup
    with the insert() function.  This can be used to introduce mock items into a view, or
    placeholder items that are later \l {resolve()}{resolved} to real model data when it becomes
    available.

    Delegate items can also be be instantiated directly from a VisualDataGroup using the
    create() function, making it possible to use VisualDataModel without an accompanying view
    type or to cherry-pick specific items that should be instantiated irregardless of whether
    they're currently within a view's visible area.

    \sa {QML Dynamic View Ordering Tutorial}
*/

QQmlDataGroup::QQmlDataGroup(QObject *parent)
    : QObject(*new QQmlDataGroupPrivate, parent)
{
}

QQmlDataGroup::QQmlDataGroup(
        const QString &name, QQmlDelegateModel *model, int index, QObject *parent)
    : QObject(*new QQmlDataGroupPrivate, parent)
{
    Q_D(QQmlDataGroup);
    d->name = name;
    d->setModel(model, Compositor::Group(index));
}

QQmlDataGroup::~QQmlDataGroup()
{
}

/*!
    \qmlproperty string QtQuick2::VisualDataGroup::name

    This property holds the name of the group.

    Each group in a model must have a unique name starting with a lower case letter.
*/

QString QQmlDataGroup::name() const
{
    Q_D(const QQmlDataGroup);
    return d->name;
}

void QQmlDataGroup::setName(const QString &name)
{
    Q_D(QQmlDataGroup);
    if (d->model)
        return;
    if (d->name != name) {
        d->name = name;
        emit nameChanged();
    }
}

/*!
    \qmlproperty int QtQuick2::VisualDataGroup::count

    This property holds the number of items in the group.
*/

int QQmlDataGroup::count() const
{
    Q_D(const QQmlDataGroup);
    if (!d->model)
        return 0;
    return QQmlDelegateModelPrivate::get(d->model)->m_compositor.count(d->group);
}

/*!
    \qmlproperty bool QtQuick2::VisualDataGroup::includeByDefault

    This property holds whether new items are assigned to this group by default.
*/

bool QQmlDataGroup::defaultInclude() const
{
    Q_D(const QQmlDataGroup);
    return d->defaultInclude;
}

void QQmlDataGroup::setDefaultInclude(bool include)
{
    Q_D(QQmlDataGroup);
    if (d->defaultInclude != include) {
        d->defaultInclude = include;

        if (d->model) {
            if (include)
                QQmlDelegateModelPrivate::get(d->model)->m_compositor.setDefaultGroup(d->group);
            else
                QQmlDelegateModelPrivate::get(d->model)->m_compositor.clearDefaultGroup(d->group);
        }
        emit defaultIncludeChanged();
    }
}

/*!
    \qmlmethod object QtQuick2::VisualDataGroup::get(int index)

    Returns a javascript object describing the item at \a index in the group.

    The returned object contains the same information that is available to a delegate from the
    VisualDataModel attached as well as the model for that item.  It has the properties:

    \list
    \li \b model The model data of the item.  This is the same as the model context property in
    a delegate
    \li \b groups A list the of names of groups the item is a member of.  This property can be
    written to change the item's membership.
    \li \b inItems Whether the item belongs to the \l {QtQuick2::VisualDataModel::items}{items} group.
    Writing to this property will add or remove the item from the group.
    \li \b itemsIndex The index of the item within the \l {QtQuick2::VisualDataModel::items}{items} group.
    \li \b {in<GroupName>} Whether the item belongs to the dynamic group \e groupName.  Writing to
    this property will add or remove the item from the group.
    \li \b {<groupName>Index} The index of the item within the dynamic group \e groupName.
    \li \b isUnresolved Whether the item is bound to an index in the model assigned to
    VisualDataModel::model.  Returns true if the item is not bound to the model, and false if it is.
    \endlist
*/

QQmlV8Handle QQmlDataGroup::get(int index)
{
    Q_D(QQmlDataGroup);
    if (!d->model)
        return QQmlV8Handle::fromHandle(v8::Undefined());;

    QQmlDelegateModelPrivate *model = QQmlDelegateModelPrivate::get(d->model);
    if (!model->m_context->isValid()) {
        return QQmlV8Handle::fromHandle(v8::Undefined());
    } else if (index < 0 || index >= model->m_compositor.count(d->group)) {
        qmlInfo(this) << tr("get: index out of range");
        return QQmlV8Handle::fromHandle(v8::Undefined());
    }

    Compositor::iterator it = model->m_compositor.find(d->group, index);
    QQmlDelegateModelItem *cacheItem = it->inCache()
            ? model->m_cache.at(it.cacheIndex)
            : 0;

    if (!cacheItem) {
        cacheItem = model->m_adaptorModel.createItem(
                model->m_cacheMetaType, model->m_context->engine(), it.modelIndex());
        if (!cacheItem)
            return QQmlV8Handle::fromHandle(v8::Undefined());
        cacheItem->groups = it->flags;

        model->m_cache.insert(it.cacheIndex, cacheItem);
        model->m_compositor.setFlags(it, 1, Compositor::CacheFlag);
    }

    if (model->m_cacheMetaType->constructor.IsEmpty())
        model->m_cacheMetaType->initializeConstructor();
    v8::Local<v8::Object> handle = model->m_cacheMetaType->constructor->NewInstance();
    handle->SetExternalResource(cacheItem);
    ++cacheItem->scriptRef;

    return QQmlV8Handle::fromHandle(handle);
}

bool QQmlDataGroupPrivate::parseIndex(
        const v8::Local<v8::Value> &value, int *index, Compositor::Group *group) const
{
    if (value->IsInt32()) {
        *index = value->Int32Value();
        return true;
    } else if (value->IsObject()) {
        v8::Local<v8::Object> object = value->ToObject();
        QQmlDelegateModelItem * const cacheItem = v8_resource_cast<QQmlDelegateModelItem>(object);
        if (QQmlDelegateModelPrivate *model = cacheItem && cacheItem->metaType->model
                ? QQmlDelegateModelPrivate::get(cacheItem->metaType->model)
                : 0) {
            *index = model->m_cache.indexOf(cacheItem);
            *group = Compositor::Cache;
            return true;
        }
    }
    return false;
}

/*!
    \qmlmethod QtQuick2::VisualDataGroup::insert(int index, jsdict data, array groups = undefined)
    \qmlmethod QtQuick2::VisualDataGroup::insert(jsdict data, var groups = undefined)

    Creates a new entry at \a index in a VisualDataModel with the values from \a data that
    correspond to roles in the model assigned to VisualDataModel::model.

    If no index is supplied the data is appended to the model.

    The optional \a groups parameter identifies the groups the new entry should belong to,
    if unspecified this is equal to the group insert was called on.

    Data inserted into a VisualDataModel can later be merged with an existing entry in
    VisualDataModel::model using the \l resolve() function.  This can be used to create placeholder
    items that are later replaced by actual data.
*/

void QQmlDataGroup::insert(QQmlV8Function *args)
{
    Q_D(QQmlDataGroup);
    QQmlDelegateModelPrivate *model = QQmlDelegateModelPrivate::get(d->model);

    int index = model->m_compositor.count(d->group);
    Compositor::Group group = d->group;

    if (args->Length() == 0)
        return;

    int  i = 0;
    v8::Local<v8::Value> v = (*args)[i];
    if (d->parseIndex(v, &index, &group)) {
        if (index < 0 || index > model->m_compositor.count(group)) {
            qmlInfo(this) << tr("insert: index out of range");
            return;
        }
        if (++i == args->Length())
            return;
        v = (*args)[i];
    }

    Compositor::insert_iterator before = index < model->m_compositor.count(group)
            ? model->m_compositor.findInsertPosition(group, index)
            : model->m_compositor.end();

    int groups = 1 << d->group;
    if (++i < args->Length())
        groups |= model->m_cacheMetaType->parseGroups((*args)[i]);

    if (v->IsArray()) {
        return;
    } else if (v->IsObject()) {
        model->insert(before, v->ToObject(), groups);
        model->emitChanges();
    }
}

/*!
    \qmlmethod QtQuick2::VisualDataGroup::create(int index)
    \qmlmethod QtQuick2::VisualDataGroup::create(int index, jsdict data, array groups = undefined)
    \qmlmethod QtQuick2::VisualDataGroup::create(jsdict data, array groups = undefined)

    Returns a reference to the instantiated item at \a index in the group.

    If a \a data object is provided it will be \l {insert}{inserted} at \a index and an item
    referencing this new entry will be returned.  The optional \a groups parameter identifies
    the groups the new entry should belong to, if unspecified this is equal to the group create()
    was called on.

    All items returned by create are added to the
    \l {QtQuick2::VisualDataModel::persistedItems}{persistedItems} group.  Items in this
    group remain instantiated when not referenced by any view.
*/

void QQmlDataGroup::create(QQmlV8Function *args)
{
    Q_D(QQmlDataGroup);
    if (!d->model)
        return;

    if (args->Length() == 0)
        return;

    QQmlDelegateModelPrivate *model = QQmlDelegateModelPrivate::get(d->model);

    int index = model->m_compositor.count(d->group);
    Compositor::Group group = d->group;

    int  i = 0;
    v8::Local<v8::Value> v = (*args)[i];
    if (d->parseIndex(v, &index, &group))
        ++i;

    if (i < args->Length() && index >= 0 && index <= model->m_compositor.count(group)) {
        v = (*args)[i];
        if (v->IsObject()) {
            int groups = 1 << d->group;
            if (++i < args->Length())
                groups |= model->m_cacheMetaType->parseGroups((*args)[i]);

            Compositor::insert_iterator before = index < model->m_compositor.count(group)
                    ? model->m_compositor.findInsertPosition(group, index)
                    : model->m_compositor.end();

            index = before.index[d->group];
            group = d->group;

            if (!model->insert(before, v->ToObject(), groups)) {
                return;
            }
        }
    }
    if (index < 0 || index >= model->m_compositor.count(group)) {
        qmlInfo(this) << tr("create: index out of range");
        return;
    }

    QObject *object = model->object(group, index, false);
    if (object) {
        QVector<Compositor::Insert> inserts;
        Compositor::iterator it = model->m_compositor.find(group, index);
        model->m_compositor.setFlags(it, 1, d->group, Compositor::PersistedFlag, &inserts);
        model->itemsInserted(inserts);
        model->m_cache.at(it.cacheIndex)->releaseObject();
    }

    args->returnValue(args->engine()->newQObject(object));
    model->emitChanges();
}

/*!
    \qmlmethod QtQuick2::VisualDataGroup::resolve(int from, int to)

    Binds an unresolved item at \a from to an item in VisualDataModel::model at index \a to.

    Unresolved items are entries whose data has been \l {insert()}{inserted} into a VisualDataGroup
    instead of being derived from a VisualDataModel::model index.  Resolving an item will replace
    the item at the target index with the unresolved item. A resolved an item will reflect the data
    of the source model at its bound index and will move when that index moves like any other item.

    If a new item is replaced in the VisualDataGroup onChanged() handler its insertion and
    replacement will be communicated to views as an atomic operation, creating the appearance
    that the model contents have not changed, or if the unresolved and model item are not adjacent
    that the previously unresolved item has simply moved.

*/
void QQmlDataGroup::resolve(QQmlV8Function *args)
{
    Q_D(QQmlDataGroup);
    if (!d->model)
        return;

    QQmlDelegateModelPrivate *model = QQmlDelegateModelPrivate::get(d->model);

    if (args->Length() < 2)
        return;

    int from = -1;
    int to = -1;
    Compositor::Group fromGroup = d->group;
    Compositor::Group toGroup = d->group;

    v8::Local<v8::Value> v = (*args)[0];
    if (d->parseIndex(v, &from, &fromGroup)) {
        if (from < 0 || from >= model->m_compositor.count(fromGroup)) {
            qmlInfo(this) << tr("resolve: from index out of range");
            return;
        }
    } else {
        qmlInfo(this) << tr("resolve: from index invalid");
        return;
    }

    v = (*args)[1];
    if (d->parseIndex(v, &to, &toGroup)) {
        if (to < 0 || to >= model->m_compositor.count(toGroup)) {
            qmlInfo(this) << tr("resolve: to index out of range");
            return;
        }
    } else {
        qmlInfo(this) << tr("resolve: to index invalid");
        return;
    }

    Compositor::iterator fromIt = model->m_compositor.find(fromGroup, from);
    Compositor::iterator toIt = model->m_compositor.find(toGroup, to);

    if (!fromIt->isUnresolved()) {
        qmlInfo(this) << tr("resolve: from is not an unresolved item");
        return;
    }
    if (!toIt->list) {
        qmlInfo(this) << tr("resolve: to is not a model item");
        return;
    }

    const int unresolvedFlags = fromIt->flags;
    const int resolvedFlags = toIt->flags;
    const int resolvedIndex = toIt.modelIndex();
    void * const resolvedList = toIt->list;

    QQmlDelegateModelItem *cacheItem = model->m_cache.at(fromIt.cacheIndex);
    cacheItem->groups &= ~Compositor::UnresolvedFlag;

    if (toIt.cacheIndex > fromIt.cacheIndex)
        toIt.decrementIndexes(1, unresolvedFlags);
    if (!toIt->inGroup(fromGroup) || toIt.index[fromGroup] > from)
        from += 1;

    model->itemsMoved(
            QVector<Compositor::Remove>() << Compositor::Remove(fromIt, 1, unresolvedFlags, 0),
            QVector<Compositor::Insert>() << Compositor::Insert(toIt, 1, unresolvedFlags, 0));
    model->itemsInserted(
            QVector<Compositor::Insert>() << Compositor::Insert(toIt, 1, (resolvedFlags & ~unresolvedFlags) | Compositor::CacheFlag));
    toIt.incrementIndexes(1, resolvedFlags | unresolvedFlags);
    model->itemsRemoved(QVector<Compositor::Remove>() << Compositor::Remove(toIt, 1, resolvedFlags));

    model->m_compositor.setFlags(toGroup, to, 1, unresolvedFlags & ~Compositor::UnresolvedFlag);
    model->m_compositor.clearFlags(fromGroup, from, 1, unresolvedFlags);

    if (resolvedFlags & Compositor::CacheFlag)
        model->m_compositor.insert(Compositor::Cache, toIt.cacheIndex, resolvedList, resolvedIndex, 1, Compositor::CacheFlag);

    Q_ASSERT(model->m_cache.count() == model->m_compositor.count(Compositor::Cache));

    if (!cacheItem->isReferenced()) {
        Q_ASSERT(toIt.cacheIndex == model->m_cache.indexOf(cacheItem));
        model->m_cache.removeAt(toIt.cacheIndex);
        model->m_compositor.clearFlags(Compositor::Cache, toIt.cacheIndex, 1, Compositor::CacheFlag);
        delete cacheItem;
        Q_ASSERT(model->m_cache.count() == model->m_compositor.count(Compositor::Cache));
    } else {
        cacheItem->resolveIndex(model->m_adaptorModel, resolvedIndex);
        if (cacheItem->attached)
            cacheItem->attached->emitUnresolvedChanged();
    }

    model->emitChanges();
}

/*!
    \qmlmethod QtQuick2::VisualDataGroup::remove(int index, int count)

    Removes \a count items starting at \a index from the group.
*/

void QQmlDataGroup::remove(QQmlV8Function *args)
{
    Q_D(QQmlDataGroup);
    if (!d->model)
        return;
    Compositor::Group group = d->group;
    int index = -1;
    int count = 1;

    if (args->Length() == 0)
        return;

    int i = 0;
    v8::Local<v8::Value> v = (*args)[i];
    if (!d->parseIndex(v, &index, &group)) {
        qmlInfo(this) << tr("remove: invalid index");
        return;
    }

    if (++i < args->Length()) {
        v = (*args)[i];
        if (v->IsInt32())
            count = v->Int32Value();
    }

    QQmlDelegateModelPrivate *model = QQmlDelegateModelPrivate::get(d->model);
    if (index < 0 || index >= model->m_compositor.count(group)) {
        qmlInfo(this) << tr("remove: index out of range");
    } else if (count != 0) {
        Compositor::iterator it = model->m_compositor.find(group, index);
        if (count < 0 || count > model->m_compositor.count(d->group) - it.index[d->group]) {
            qmlInfo(this) << tr("remove: invalid count");
        } else {
            model->removeGroups(it, count, d->group, 1 << d->group);
        }
    }
}

bool QQmlDataGroupPrivate::parseGroupArgs(
        QQmlV8Function *args, Compositor::Group *group, int *index, int *count, int *groups) const
{
    if (!model || !QQmlDelegateModelPrivate::get(model)->m_cacheMetaType)
        return false;

    if (args->Length() < 2)
        return false;

    int i = 0;
    v8::Local<v8::Value> v = (*args)[i];
    if (!parseIndex(v, index, group))
        return false;

    v = (*args)[++i];
    if (v->IsInt32()) {
        *count = v->Int32Value();

        if (++i == args->Length())
            return false;
        v = (*args)[i];
    }

    *groups = QQmlDelegateModelPrivate::get(model)->m_cacheMetaType->parseGroups(v);

    return true;
}

/*!
    \qmlmethod QtQuick2::VisualDataGroup::addGroups(int index, int count, stringlist groups)

    Adds \a count items starting at \a index to \a groups.
*/

void QQmlDataGroup::addGroups(QQmlV8Function *args)
{
    Q_D(QQmlDataGroup);
    Compositor::Group group = d->group;
    int index = -1;
    int count = 1;
    int groups = 0;

    if (!d->parseGroupArgs(args, &group, &index, &count, &groups))
        return;

    QQmlDelegateModelPrivate *model = QQmlDelegateModelPrivate::get(d->model);
    if (index < 0 || index >= model->m_compositor.count(group)) {
        qmlInfo(this) << tr("addGroups: index out of range");
    } else if (count != 0) {
        Compositor::iterator it = model->m_compositor.find(group, index);
        if (count < 0 || count > model->m_compositor.count(d->group) - it.index[d->group]) {
            qmlInfo(this) << tr("addGroups: invalid count");
        } else {
            model->addGroups(it, count, d->group, groups);
        }
    }
}

/*!
    \qmlmethod QtQuick2::VisualDataGroup::removeGroups(int index, int count, stringlist groups)

    Removes \a count items starting at \a index from \a groups.
*/

void QQmlDataGroup::removeGroups(QQmlV8Function *args)
{
    Q_D(QQmlDataGroup);
    Compositor::Group group = d->group;
    int index = -1;
    int count = 1;
    int groups = 0;

    if (!d->parseGroupArgs(args, &group, &index, &count, &groups))
        return;

    QQmlDelegateModelPrivate *model = QQmlDelegateModelPrivate::get(d->model);
    if (index < 0 || index >= model->m_compositor.count(group)) {
        qmlInfo(this) << tr("removeGroups: index out of range");
    } else if (count != 0) {
        Compositor::iterator it = model->m_compositor.find(group, index);
        if (count < 0 || count > model->m_compositor.count(d->group) - it.index[d->group]) {
            qmlInfo(this) << tr("removeGroups: invalid count");
        } else {
            model->removeGroups(it, count, d->group, groups);
        }
    }
}

/*!
    \qmlmethod QtQuick2::VisualDataGroup::setGroups(int index, int count, stringlist groups)

    Sets the \a groups \a count items starting at \a index belong to.
*/

void QQmlDataGroup::setGroups(QQmlV8Function *args)
{
    Q_D(QQmlDataGroup);
    Compositor::Group group = d->group;
    int index = -1;
    int count = 1;
    int groups = 0;

    if (!d->parseGroupArgs(args, &group, &index, &count, &groups))
        return;

    QQmlDelegateModelPrivate *model = QQmlDelegateModelPrivate::get(d->model);
    if (index < 0 || index >= model->m_compositor.count(group)) {
        qmlInfo(this) << tr("setGroups: index out of range");
    } else if (count != 0) {
        Compositor::iterator it = model->m_compositor.find(group, index);
        if (count < 0 || count > model->m_compositor.count(d->group) - it.index[d->group]) {
            qmlInfo(this) << tr("setGroups: invalid count");
        } else {
            model->setGroups(it, count, d->group, groups);
        }
    }
}

/*!
    \qmlmethod QtQuick2::VisualDataGroup::setGroups(int index, int count, stringlist groups)

    Sets the \a groups \a count items starting at \a index belong to.
*/

/*!
    \qmlmethod QtQuick2::VisualDataGroup::move(var from, var to, int count)

    Moves \a count at \a from in a group \a to a new position.
*/

void QQmlDataGroup::move(QQmlV8Function *args)
{
    Q_D(QQmlDataGroup);

    if (args->Length() < 2)
        return;

    Compositor::Group fromGroup = d->group;
    Compositor::Group toGroup = d->group;
    int from = -1;
    int to = -1;
    int count = 1;

    if (!d->parseIndex((*args)[0], &from, &fromGroup)) {
        qmlInfo(this) << tr("move: invalid from index");
        return;
    }

    if (!d->parseIndex((*args)[1], &to, &toGroup)) {
        qmlInfo(this) << tr("move: invalid to index");
        return;
    }

    if (args->Length() > 2) {
        v8::Local<v8::Value> v = (*args)[2];
        if (v->IsInt32())
            count = v->Int32Value();
    }

    QQmlDelegateModelPrivate *model = QQmlDelegateModelPrivate::get(d->model);

    if (count < 0) {
        qmlInfo(this) << tr("move: invalid count");
    } else if (from < 0 || from + count > model->m_compositor.count(fromGroup)) {
        qmlInfo(this) << tr("move: from index out of range");
    } else if (!model->m_compositor.verifyMoveTo(fromGroup, from, toGroup, to, count, d->group)) {
        qmlInfo(this) << tr("move: to index out of range");
    } else if (count > 0) {
        QVector<Compositor::Remove> removes;
        QVector<Compositor::Insert> inserts;

        model->m_compositor.move(fromGroup, from, toGroup, to, count, d->group, &removes, &inserts);
        model->itemsMoved(removes, inserts);
        model->emitChanges();
    }

}

/*!
    \qmlsignal QtQuick2::VisualDataGroup::onChanged(array removed, array inserted)

    This handler is called when items have been removed from or inserted into the group.

    Each object in the \a removed and \a inserted arrays has two values; the \e index of the first
    item inserted or removed and a \e count of the number of consecutive items inserted or removed.

    Each index is adjusted for previous changes with all removed items preceding any inserted
    items.
*/

//============================================================================

QQmlPartsModel::QQmlPartsModel(QQmlDelegateModel *model, const QString &part, QObject *parent)
    : QQmlInstanceModel(*new QObjectPrivate, parent)
    , m_model(model)
    , m_part(part)
    , m_compositorGroup(Compositor::Cache)
    , m_inheritGroup(true)
{
    QQmlDelegateModelPrivate *d = QQmlDelegateModelPrivate::get(m_model);
    if (d->m_cacheMetaType) {
        QQmlDataGroupPrivate::get(d->m_groups[1])->emitters.insert(this);
        m_compositorGroup = Compositor::Default;
    } else {
        d->m_pendingParts.insert(this);
    }
}

QQmlPartsModel::~QQmlPartsModel()
{
}

QString QQmlPartsModel::filterGroup() const
{
    if (m_inheritGroup)
        return m_model->filterGroup();
    return m_filterGroup;
}

void QQmlPartsModel::setFilterGroup(const QString &group)
{
    if (QQmlDelegateModelPrivate::get(m_model)->m_transaction) {
        qmlInfo(this) << tr("The group of a VisualDataModel cannot be changed within onChanged");
        return;
    }

    if (m_filterGroup != group || m_inheritGroup) {
        m_filterGroup = group;
        m_inheritGroup = false;
        updateFilterGroup();

        emit filterGroupChanged();
    }
}

void QQmlPartsModel::resetFilterGroup()
{
    if (!m_inheritGroup) {
        m_inheritGroup = true;
        updateFilterGroup();
        emit filterGroupChanged();
    }
}

void QQmlPartsModel::updateFilterGroup()
{
    QQmlDelegateModelPrivate *model = QQmlDelegateModelPrivate::get(m_model);
    if (!model->m_cacheMetaType)
        return;

    if (m_inheritGroup) {
        if (m_filterGroup == model->m_filterGroup)
            return;
        m_filterGroup = model->m_filterGroup;
    }

    QQmlListCompositor::Group previousGroup = m_compositorGroup;
    m_compositorGroup = Compositor::Default;
    QQmlDataGroupPrivate::get(model->m_groups[Compositor::Default])->emitters.insert(this);
    for (int i = 1; i < model->m_groupCount; ++i) {
        if (m_filterGroup == model->m_cacheMetaType->groupNames.at(i - 1)) {
            m_compositorGroup = Compositor::Group(i);
            break;
        }
    }

    QQmlDataGroupPrivate::get(model->m_groups[m_compositorGroup])->emitters.insert(this);
    if (m_compositorGroup != previousGroup) {
        QVector<QQmlChangeSet::Remove> removes;
        QVector<QQmlChangeSet::Insert> inserts;
        model->m_compositor.transition(previousGroup, m_compositorGroup, &removes, &inserts);

        QQmlChangeSet changeSet;
        changeSet.move(removes, inserts);
        if (!changeSet.isEmpty())
            emit modelUpdated(changeSet, false);

        if (changeSet.difference() != 0)
            emit countChanged();
    }
}

void QQmlPartsModel::updateFilterGroup(
        Compositor::Group group, const QQmlChangeSet &changeSet)
{
    if (!m_inheritGroup)
        return;

    m_compositorGroup = group;
    QQmlDataGroupPrivate::get(QQmlDelegateModelPrivate::get(m_model)->m_groups[m_compositorGroup])->emitters.insert(this);

    if (!changeSet.isEmpty())
        emit modelUpdated(changeSet, false);

    if (changeSet.difference() != 0)
        emit countChanged();

    emit filterGroupChanged();
}

int QQmlPartsModel::count() const
{
    QQmlDelegateModelPrivate *model = QQmlDelegateModelPrivate::get(m_model);
    return model->m_delegate
            ? model->m_compositor.count(m_compositorGroup)
            : 0;
}

bool QQmlPartsModel::isValid() const
{
    return m_model->isValid();
}

QObject *QQmlPartsModel::object(int index, bool asynchronous)
{
    QQmlDelegateModelPrivate *model = QQmlDelegateModelPrivate::get(m_model);

    if (!model->m_delegate || index < 0 || index >= model->m_compositor.count(m_compositorGroup)) {
        qWarning() << "VisualDataModel::item: index out range" << index << model->m_compositor.count(m_compositorGroup);
        return 0;
    }

    QObject *object = model->object(m_compositorGroup, index, asynchronous);

    if (QQuickPackage *package = qmlobject_cast<QQuickPackage *>(object)) {
        QObject *part = package->part(m_part);
        if (!part)
            return 0;
        m_packaged.insertMulti(part, package);
        return part;
    }

    model->release(object);
    if (!model->m_delegateValidated) {
        if (object)
            qmlInfo(model->m_delegate) << tr("Delegate component must be Package type.");
        model->m_delegateValidated = true;
    }

    return 0;
}

QQmlInstanceModel::ReleaseFlags QQmlPartsModel::release(QObject *item)
{
    QQmlInstanceModel::ReleaseFlags flags = 0;

    QHash<QObject *, QQuickPackage *>::iterator it = m_packaged.find(item);
    if (it != m_packaged.end()) {
        QQuickPackage *package = *it;
        QQmlDelegateModelPrivate *model = QQmlDelegateModelPrivate::get(m_model);
        flags = model->release(package);
        m_packaged.erase(it);
        if (!m_packaged.contains(item))
            flags &= ~Referenced;
        if (flags & Destroyed)
            QQmlDelegateModelPrivate::get(m_model)->emitDestroyingPackage(package);
    }
    return flags;
}

QString QQmlPartsModel::stringValue(int index, const QString &role)
{
    return QQmlDelegateModelPrivate::get(m_model)->stringValue(m_compositorGroup, index, role);
}

void QQmlPartsModel::setWatchedRoles(QList<QByteArray> roles)
{
    QQmlDelegateModelPrivate *model = QQmlDelegateModelPrivate::get(m_model);
    model->m_adaptorModel.replaceWatchedRoles(m_watchedRoles, roles);
    m_watchedRoles = roles;
}

int QQmlPartsModel::indexOf(QObject *item, QObject *) const
{
    QHash<QObject *, QQuickPackage *>::const_iterator it = m_packaged.find(item);
    if (it != m_packaged.end()) {
        if (QQmlDelegateModelItem *cacheItem = QQmlDelegateModelItem::dataForObject(*it))
            return cacheItem->groupIndex(m_compositorGroup);
    }
    return -1;
}

void QQmlPartsModel::createdPackage(int index, QQuickPackage *package)
{
    emit createdItem(index, package->part(m_part));
}

void QQmlPartsModel::initPackage(int index, QQuickPackage *package)
{
    emit initItem(index, package->part(m_part));
}

void QQmlPartsModel::destroyingPackage(QQuickPackage *package)
{
    QObject *item = package->part(m_part);
    Q_ASSERT(!m_packaged.contains(item));
    emit destroyingItem(item);
}

void QQmlPartsModel::emitModelUpdated(const QQmlChangeSet &changeSet, bool reset)
{
    emit modelUpdated(changeSet, reset);
    if (changeSet.difference() != 0)
        emit countChanged();
}

//============================================================================

v8::Handle<v8::Value> get_change_index(v8::Local<v8::String>, const v8::AccessorInfo &info)
{
    return info.This()->GetInternalField(0);
}

v8::Handle<v8::Value> get_change_count(v8::Local<v8::String>, const v8::AccessorInfo &info)
{
    return info.This()->GetInternalField(1);
}

v8::Handle<v8::Value> get_change_moveId(v8::Local<v8::String>, const v8::AccessorInfo &info)
{
    return info.This()->GetInternalField(2);
}

class QQmlDataGroupChangeArray : public QV8ObjectResource
{
    V8_RESOURCE_TYPE(ChangeSetArrayType)
public:
    QQmlDataGroupChangeArray(QV8Engine *engine)
        : QV8ObjectResource(engine)
    {
    }

    virtual quint32 count() const = 0;
    virtual const QQmlChangeSet::Change &at(int index) const = 0;

    static v8::Handle<v8::Value> get_change(quint32 index, const v8::AccessorInfo &info)
    {
        QQmlDataGroupChangeArray *array = v8_resource_cast<QQmlDataGroupChangeArray>(info.This());
        V8ASSERT_TYPE(array, "Not a valid change array");

        if (index >= array->count())
            return v8::Undefined();

        const QQmlChangeSet::Change &change = array->at(index);

        v8::Local<v8::Object> object = engineData(array->engine)->constructorChange->NewInstance();
        object->SetInternalField(0, v8::Int32::New(change.index));
        object->SetInternalField(1, v8::Int32::New(change.count));
        if (change.isMove())
            object->SetInternalField(2, v8::Int32::New(change.moveId));

        return object;
    }

    static v8::Handle<v8::Value> get_length(v8::Local<v8::String>, const v8::AccessorInfo &info)
    {
        QQmlDataGroupChangeArray *array = v8_resource_cast<QQmlDataGroupChangeArray>(info.This());
        V8ASSERT_TYPE(array, "Not a valid change array");

        return v8::Integer::New(array->count());
    }

    static v8::Local<v8::Function> constructor()
    {
        v8::Local<v8::FunctionTemplate> changeArray = v8::FunctionTemplate::New();
        changeArray->InstanceTemplate()->SetHasExternalResource(true);
        changeArray->InstanceTemplate()->SetIndexedPropertyHandler(get_change);
        changeArray->InstanceTemplate()->SetAccessor(v8::String::New("length"), get_length);
        return changeArray->GetFunction();
    }
};

class QQmlDataGroupRemoveArray : public QQmlDataGroupChangeArray
{
public:
    QQmlDataGroupRemoveArray(QV8Engine *engine, const QVector<QQmlChangeSet::Remove> &changes)
        : QQmlDataGroupChangeArray(engine)
        , changes(changes)
    {
    }

    quint32 count() const { return changes.count(); }
    const QQmlChangeSet::Change &at(int index) const { return changes.at(index); }

private:
    QVector<QQmlChangeSet::Remove> changes;
};

class QQmlDataGroupInsertArray : public QQmlDataGroupChangeArray
{
public:
    QQmlDataGroupInsertArray(QV8Engine *engine, const QVector<QQmlChangeSet::Insert> &changes)
        : QQmlDataGroupChangeArray(engine)
        , changes(changes)
    {
    }

    quint32 count() const { return changes.count(); }
    const QQmlChangeSet::Change &at(int index) const { return changes.at(index); }

private:
    QVector<QQmlChangeSet::Insert> changes;
};

QQmlDelegateModelEngineData::QQmlDelegateModelEngineData(QV8Engine *)
{
    strings = qPersistentNew(v8::Array::New(StringCount));
    strings->Set(Model, v8::String::New("model"));
    strings->Set(Groups, v8::String::New("groups"));
    strings->Set(IsUnresolved, v8::String::New("isUnresolved"));
    strings->Set(ItemsIndex, v8::String::New("itemsIndex"));
    strings->Set(PersistedItemsIndex, v8::String::New("persistedItemsIndex"));
    strings->Set(InItems, v8::String::New("inItems"));
    strings->Set(InPersistedItems, v8::String::New("inPersistedItems"));

    v8::Local<v8::FunctionTemplate> change = v8::FunctionTemplate::New();
    change->InstanceTemplate()->SetAccessor(v8::String::New("index"), get_change_index);
    change->InstanceTemplate()->SetAccessor(v8::String::New("count"), get_change_count);
    change->InstanceTemplate()->SetAccessor(v8::String::New("moveId"), get_change_moveId);
    change->InstanceTemplate()->SetInternalFieldCount(3);
    constructorChange = qPersistentNew(change->GetFunction());
    constructorChangeArray = qPersistentNew(QQmlDataGroupChangeArray::constructor());
}

QQmlDelegateModelEngineData::~QQmlDelegateModelEngineData()
{
    qPersistentDispose(strings);
    qPersistentDispose(constructorChange);
    qPersistentDispose(constructorChangeArray);
}

v8::Local<v8::Object> QQmlDelegateModelEngineData::array(
        QV8Engine *engine, const QVector<QQmlChangeSet::Remove> &changes)
{
    v8::Local<v8::Object> array = constructorChangeArray->NewInstance();
    array->SetExternalResource(new QQmlDataGroupRemoveArray(engine, changes));
    return array;
}

v8::Local<v8::Object> QQmlDelegateModelEngineData::array(
        QV8Engine *engine, const QVector<QQmlChangeSet::Insert> &changes)
{
    v8::Local<v8::Object> array = constructorChangeArray->NewInstance();
    array->SetExternalResource(new QQmlDataGroupInsertArray(engine, changes));
    return array;
}

QT_END_NAMESPACE
