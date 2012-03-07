/****************************************************************************
**
** Copyright (C) 2012 Nokia Corporation and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/
**
** This file is part of the QtQml module of the Qt Toolkit.
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

#include "qqmlcomponent.h"
#include "qqmlcomponent_p.h"
#include "qqmlcomponentattached_p.h"

#include "qqmlcompiler_p.h"
#include "qqmlcontext_p.h"
#include "qqmlengine_p.h"
#include "qqmlvme_p.h"
#include "qqml.h"
#include "qqmlengine.h"
#include "qqmlbinding_p.h"
#include "qqmlbinding_p_p.h"
#include "qqmlglobal_p.h"
#include "qqmlscript_p.h"
#include <private/qqmlprofilerservice_p.h>
#include <private/qqmlenginedebugservice_p.h>
#include "qqmlincubator.h"
#include "qqmlincubator_p.h"

#include <private/qv8engine_p.h>
#include <private/qv8include_p.h>

#include <QStack>
#include <QStringList>
#include <QtCore/qdebug.h>
#include <qqmlinfo.h>

QT_BEGIN_NAMESPACE

class QQmlComponentExtension : public QV8Engine::Deletable
{
public:
    QQmlComponentExtension(QV8Engine *);
    virtual ~QQmlComponentExtension();

    v8::Persistent<v8::Function> incubationConstructor;
    v8::Persistent<v8::Script> initialProperties;
    v8::Persistent<v8::Function> forceCompletion;
};
V8_DEFINE_EXTENSION(QQmlComponentExtension, componentExtension);

/*
    Try to do what's necessary for a reasonable display of the type
    name, but no more (just enough for the client to do more extensive cleanup).

    Should only be called when debugging is enabled.
*/
static inline QString buildTypeNameForDebug(const QMetaObject *metaObject)
{
    static const QString qmlMarker(QLatin1String("_QML"));
    static const QChar underscore(QLatin1Char('_'));
    static const QChar asterisk(QLatin1Char('*'));
    QQmlType *type = QQmlMetaType::qmlType(metaObject);
    QString typeName = type ? type->qmlTypeName() : QString::fromUtf8(metaObject->className());
    if (!type) {
        //### optimize further?
        int marker = typeName.indexOf(qmlMarker);
        if (marker != -1 && marker < typeName.count() - 1) {
            if (typeName[marker + 1] == underscore) {
                const QString className = typeName.left(marker) + asterisk;
                type = QQmlMetaType::qmlType(QMetaType::type(className.toUtf8()));
                if (type)
                    typeName = type->qmlTypeName();
            }
        }
    }
    return typeName;
}

/*!
    \class QQmlComponent
    \since 4.7
    \brief The QQmlComponent class encapsulates a QML component definition.
    \mainclass

    Components are reusable, encapsulated QML elements with well-defined interfaces.
    They are often defined in \l {qdeclarativedocuments.html}{Component Files}.

    A QQmlComponent instance can be created from a QML file.
    For example, if there is a \c main.qml file like this:

    \qml
    import QtQuick 2.0

    Item {
        width: 200
        height: 200
    }
    \endqml

    The following code loads this QML file as a component, creates an instance of
    this component using create(), and then queries the \l Item's \l {Item::}{width}
    value:

    \code
    QQmlEngine *engine = new QQmlEngine;
    QQmlComponent component(engine, QUrl::fromLocalFile("main.qml"));

    QObject *myObject = component.create();
    QQuickItem *item = qobject_cast<QQuickItem*>(myObject);
    int width = item->width();  // width = 200
    \endcode


    \section2 Network Components

    If the URL passed to QQmlComponent is a network resource, or if the QML document references a
    network resource, the QQmlComponent has to fetch the network data before it is able to create
    objects.  In this case, the QQmlComponent will have a \l {QQmlComponent::Loading}{Loading}
    \l {QQmlComponent::status()}{status}.  An application will have to wait until the component
    is \l {QQmlComponent::Ready}{Ready} before calling \l {QQmlComponent::create()}.

    The following example shows how to load a QML file from a network resource.  After creating
    the QQmlComponent, it tests whether the component is loading.  If it is, it connects to the
    QQmlComponent::statusChanged() signal and otherwise calls the \c {continueLoading()} method
    directly. Note that QQmlComponent::isLoading() may be false for a network component if the
    component has been cached and is ready immediately.

    \code
    MyApplication::MyApplication()
    {
        // ...
        component = new QQmlComponent(engine, QUrl("http://www.example.com/main.qml"));
        if (component->isLoading())
            QObject::connect(component, SIGNAL(statusChanged(QQmlComponent::Status)),
                             this, SLOT(continueLoading()));
        else
            continueLoading();
    }

    void MyApplication::continueLoading()
    {
        if (component->isError()) {
            qWarning() << component->errors();
        } else {
            QObject *myObject = component->create();
        }
    }
    \endcode

    \sa {Using QML Bindings in C++ Applications}, {Integrating QML Code with Existing Qt UI Code}
*/

/*!
    \qmlclass Component QQmlComponent
    \ingroup qml-utility-elements
    \since 4.7
    \brief The Component element encapsulates a QML component definition.

    Components are reusable, encapsulated QML elements with well-defined interfaces.

    Components are often defined by \l {qdeclarativedocuments.html}{component files} -
    that is, \c .qml files. The \e Component element essentially allows QML components
    to be defined inline, within a \l {QML Document}{QML document}, rather than as a separate QML file.
    This may be useful for reusing a small component within a QML file, or for defining
    a component that logically belongs with other QML components within a file.

    For example, here is a component that is used by multiple \l Loader objects.
    It contains a single item, a \l Rectangle:

    \snippet doc/src/snippets/qml/component.qml 0

    Notice that while a \l Rectangle by itself would be automatically 
    rendered and displayed, this is not the case for the above rectangle
    because it is defined inside a \c Component. The component encapsulates the
    QML elements within, as if they were defined in a separate QML
    file, and is not loaded until requested (in this case, by the
    two \l Loader objects).

    Defining a \c Component is similar to defining a \l {QML Document}{QML document}.
    A QML document has a single top-level item that defines the behaviors and
    properties of that component, and cannot define properties or behaviors outside
    of that top-level item. In the same way, a \c Component definition contains a single
    top level item (which in the above example is a \l Rectangle) and cannot define any
    data outside of this item, with the exception of an \e id (which in the above example
    is \e redSquare).

    The \c Component element is commonly used to provide graphical components
    for views. For example, the ListView::delegate property requires a \c Component
    to specify how each list item is to be displayed.

    \c Component objects can also be created dynamically using
    \l{QML:Qt::createComponent()}{Qt.createComponent()}.
*/

/*!
    \qmlattachedsignal Component::onCompleted()

    Emitted after component "startup" has completed.  This can be used to
    execute script code at startup, once the full QML environment has been
    established.

    The \c {Component::onCompleted} attached property can be applied to
    any element.  The order of running the \c onCompleted scripts is
    undefined.

    \qml
    Rectangle {
        Component.onCompleted: console.log("Completed Running!")
        Rectangle {
            Component.onCompleted: console.log("Nested Completed Running!")
        }
    }
    \endqml
*/

/*!
    \qmlattachedsignal Component::onDestruction()

    Emitted as the component begins destruction.  This can be used to undo
    work done in the onCompleted signal, or other imperative code in your
    application.

    The \c {Component::onDestruction} attached property can be applied to
    any element.  However, it applies to the destruction of the component as
    a whole, and not the destruction of the specific object.  The order of
    running the \c onDestruction scripts is undefined.

    \qml
    Rectangle {
        Component.onDestruction: console.log("Destruction Beginning!")
        Rectangle {
            Component.onDestruction: console.log("Nested Destruction Beginning!")
        }
    }
    \endqml

    \sa QtQml
*/

/*!
    \enum QQmlComponent::Status
    
    Specifies the loading status of the QQmlComponent.

    \value Null This QQmlComponent has no data.  Call loadUrl() or setData() to add QML content.
    \value Ready This QQmlComponent is ready and create() may be called.
    \value Loading This QQmlComponent is loading network data.
    \value Error An error has occurred.  Call errors() to retrieve a list of \{QQmlError}{errors}.
*/

void QQmlComponentPrivate::typeDataReady(QQmlTypeData *)
{
    Q_Q(QQmlComponent);

    Q_ASSERT(typeData);

    fromTypeData(typeData);
    typeData = 0;

    emit q->statusChanged(q->status());
}

void QQmlComponentPrivate::typeDataProgress(QQmlTypeData *, qreal p)
{
    Q_Q(QQmlComponent);

    progress = p;

    emit q->progressChanged(p);
}

void QQmlComponentPrivate::fromTypeData(QQmlTypeData *data)
{
    url = data->finalUrl();
    QQmlCompiledData *c = data->compiledData();

    if (!c) {
        Q_ASSERT(data->isError());
        state.errors = data->errors();
    } else {
        cc = c;
    }

    data->release();
}

void QQmlComponentPrivate::clear()
{
    if (typeData) {
        typeData->unregisterCallback(this);
        typeData->release();
        typeData = 0;
    }
        
    if (cc) { 
        cc->release();
        cc = 0;
    }
}

/*!
    \internal
*/
QQmlComponent::QQmlComponent(QObject *parent)
    : QObject(*(new QQmlComponentPrivate), parent)
{
}

/*!
    Destruct the QQmlComponent.
*/
QQmlComponent::~QQmlComponent()
{
    Q_D(QQmlComponent);

    if (d->state.completePending) {
        qWarning("QQmlComponent: Component destroyed while completion pending");
        d->completeCreate();
    }

    if (d->typeData) {
        d->typeData->unregisterCallback(d);
        d->typeData->release();
    }
    if (d->cc)
        d->cc->release();
}

/*!
    \qmlproperty enumeration Component::status
    This property holds the status of component loading.  It can be one of:
    \list
    \o Component.Null - no data is available for the component
    \o Component.Ready - the component has been loaded, and can be used to create instances.
    \o Component.Loading - the component is currently being loaded
    \o Component.Error - an error occurred while loading the component.
               Calling errorString() will provide a human-readable description of any errors.
    \endlist
 */

/*!
    \property QQmlComponent::status
    The component's current \l{QQmlComponent::Status} {status}.
 */
QQmlComponent::Status QQmlComponent::status() const
{
    Q_D(const QQmlComponent);

    if (d->typeData)
        return Loading;
    else if (!d->state.errors.isEmpty())
        return Error;
    else if (d->engine && d->cc)
        return Ready;
    else
        return Null;
}

/*!
    Returns true if status() == QQmlComponent::Null.
*/
bool QQmlComponent::isNull() const
{
    return status() == Null;
}

/*!
    Returns true if status() == QQmlComponent::Ready.
*/
bool QQmlComponent::isReady() const
{
    return status() == Ready;
}

/*!
    Returns true if status() == QQmlComponent::Error.
*/
bool QQmlComponent::isError() const
{
    return status() == Error;
}

/*!
    Returns true if status() == QQmlComponent::Loading.
*/
bool QQmlComponent::isLoading() const
{
    return status() == Loading;
}

/*!
    \qmlproperty real Component::progress
    The progress of loading the component, from 0.0 (nothing loaded)
    to 1.0 (finished).
*/

/*!
    \property QQmlComponent::progress
    The progress of loading the component, from 0.0 (nothing loaded)
    to 1.0 (finished).
*/
qreal QQmlComponent::progress() const
{
    Q_D(const QQmlComponent);
    return d->progress;
}

/*!
    \fn void QQmlComponent::progressChanged(qreal progress)

    Emitted whenever the component's loading progress changes.  \a progress will be the
    current progress between 0.0 (nothing loaded) and 1.0 (finished).
*/

/*!
    \fn void QQmlComponent::statusChanged(QQmlComponent::Status status)

    Emitted whenever the component's status changes.  \a status will be the
    new status.
*/

/*!
    Create a QQmlComponent with no data and give it the specified
    \a engine and \a parent. Set the data with setData().
*/
QQmlComponent::QQmlComponent(QQmlEngine *engine, QObject *parent)
    : QObject(*(new QQmlComponentPrivate), parent)
{
    Q_D(QQmlComponent);
    d->engine = engine;
}

/*!
    Create a QQmlComponent from the given \a url and give it the
    specified \a parent and \a engine.

    Ensure that the URL provided is full and correct, in particular, use
    \l QUrl::fromLocalFile() when loading a file from the local filesystem.

    \sa loadUrl()
*/
QQmlComponent::QQmlComponent(QQmlEngine *engine, const QUrl &url, QObject *parent)
: QObject(*(new QQmlComponentPrivate), parent)
{
    Q_D(QQmlComponent);
    d->engine = engine;
    loadUrl(url);
}

/*!
    Create a QQmlComponent from the given \a fileName and give it the specified 
    \a parent and \a engine.

    \sa loadUrl()
*/
QQmlComponent::QQmlComponent(QQmlEngine *engine, const QString &fileName, 
                           QObject *parent)
: QObject(*(new QQmlComponentPrivate), parent)
{
    Q_D(QQmlComponent);
    d->engine = engine;
    loadUrl(d->engine->baseUrl().resolved(QUrl::fromLocalFile(fileName)));
}

/*!
    \internal
*/
QQmlComponent::QQmlComponent(QQmlEngine *engine, QQmlCompiledData *cc, int start, QObject *parent)
    : QObject(*(new QQmlComponentPrivate), parent)
{
    Q_D(QQmlComponent);
    d->engine = engine;
    d->cc = cc;
    cc->addref();
    d->start = start;
    d->url = cc->url;
    d->progress = 1.0;
}

/*!
    Sets the QQmlComponent to use the given QML \a data.  If \a url
    is provided, it is used to set the component name and to provide
    a base path for items resolved by this component.
*/
void QQmlComponent::setData(const QByteArray &data, const QUrl &url)
{
    Q_D(QQmlComponent);

    d->clear();

    d->url = url;

    QQmlTypeData *typeData = QQmlEnginePrivate::get(d->engine)->typeLoader.get(data, url);
    
    if (typeData->isCompleteOrError()) {
        d->fromTypeData(typeData);
    } else {
        d->typeData = typeData;
        d->typeData->registerCallback(d);
    }

    d->progress = 1.0;
    emit statusChanged(status());
    emit progressChanged(d->progress);
}

/*!
Returns the QQmlContext the component was created in.  This is only
valid for components created directly from QML.
*/
QQmlContext *QQmlComponent::creationContext() const
{
    Q_D(const QQmlComponent);
    if(d->creationContext)
        return d->creationContext->asQQmlContext();

    return qmlContext(this);
}

/*!
    Load the QQmlComponent from the provided \a url.

    Ensure that the URL provided is full and correct, in particular, use
    \l QUrl::fromLocalFile() when loading a file from the local filesystem.
*/
void QQmlComponent::loadUrl(const QUrl &url)
{
    Q_D(QQmlComponent);

    d->clear();

    if ((url.isRelative() && !url.isEmpty())
    || url.scheme() == QLatin1String("file")) // Workaround QTBUG-11929
        d->url = d->engine->baseUrl().resolved(url);
    else
        d->url = url;

    if (url.isEmpty()) {
        QQmlError error;
        error.setDescription(tr("Invalid empty URL"));
        d->state.errors << error;
        return;
    }

    QQmlTypeData *data = QQmlEnginePrivate::get(d->engine)->typeLoader.get(d->url);

    if (data->isCompleteOrError()) {
        d->fromTypeData(data);
        d->progress = 1.0;
    } else {
        d->typeData = data;
        d->typeData->registerCallback(d);
        d->progress = data->progress();
    }

    emit statusChanged(status());
    emit progressChanged(d->progress);
}

/*!
    Return the list of errors that occurred during the last compile or create
    operation.  An empty list is returned if isError() is not set.
*/
QList<QQmlError> QQmlComponent::errors() const
{
    Q_D(const QQmlComponent);
    if (isError())
        return d->state.errors;
    else
        return QList<QQmlError>();
}

/*!
    \qmlmethod string Component::errorString()

    Returns a human-readable description of any errors.

    The string includes the file, location, and description of each error.
    If multiple errors are present they are separated by a newline character.

    If no errors are present, an empty string is returned.
*/

/*!
    \internal
    errorString is only meant as a way to get the errors in script
*/
QString QQmlComponent::errorString() const
{
    Q_D(const QQmlComponent);
    QString ret;
    if(!isError())
        return ret;
    foreach(const QQmlError &e, d->state.errors) {
        ret += e.url().toString() + QLatin1Char(':') +
               QString::number(e.line()) + QLatin1Char(' ') +
               e.description() + QLatin1Char('\n');
    }
    return ret;
}

/*!
    \qmlproperty url Component::url
    The component URL.  This is the URL that was used to construct the component.
*/

/*!
    \property QQmlComponent::url
    The component URL.  This is the URL passed to either the constructor,
    or the loadUrl() or setData() methods.
*/
QUrl QQmlComponent::url() const
{
    Q_D(const QQmlComponent);
    return d->url;
}

/*!
    \internal
*/
QQmlComponent::QQmlComponent(QQmlComponentPrivate &dd, QObject *parent)
    : QObject(dd, parent)
{
}

/*!
    Create an object instance from this component.  Returns 0 if creation
    failed.  \a context specifies the context within which to create the object
    instance.  

    If \a context is 0 (the default), it will create the instance in the
    engine' s \l {QQmlEngine::rootContext()}{root context}.
*/
QObject *QQmlComponent::create(QQmlContext *context)
{
    Q_D(QQmlComponent);

    if (!context)
        context = d->engine->rootContext();

    QObject *rv = beginCreate(context);
    completeCreate();
    return rv;
}

/*!
    This method provides more advanced control over component instance creation.
    In general, programmers should use QQmlComponent::create() to create a 
    component.

    Create an object instance from this component.  Returns 0 if creation
    failed.  \a publicContext specifies the context within which to create the object
    instance.  

    When QQmlComponent constructs an instance, it occurs in three steps:
    \list 1
    \i The object hierarchy is created, and constant values are assigned.
    \i Property bindings are evaluated for the the first time.
    \i If applicable, QQmlParserStatus::componentComplete() is called on objects.
    \endlist 
    QQmlComponent::beginCreate() differs from QQmlComponent::create() in that it
    only performs step 1.  QQmlComponent::completeCreate() must be called to 
    complete steps 2 and 3.

    This breaking point is sometimes useful when using attached properties to
    communicate information to an instantiated component, as it allows their
    initial values to be configured before property bindings take effect.
*/
QObject *QQmlComponent::beginCreate(QQmlContext *publicContext)
{
    Q_D(QQmlComponent);

    Q_ASSERT(publicContext);
    QQmlContextData *context = QQmlContextData::get(publicContext);

    return d->beginCreate(context);
}

QObject *
QQmlComponentPrivate::beginCreate(QQmlContextData *context)
{
    Q_Q(QQmlComponent);
    if (!context) {
        qWarning("QQmlComponent: Cannot create a component in a null context");
        return 0;
    }

    if (!context->isValid()) {
        qWarning("QQmlComponent: Cannot create a component in an invalid context");
        return 0;
    }

    if (context->engine != engine) {
        qWarning("QQmlComponent: Must create component in context from the same QQmlEngine");
        return 0;
    }

    if (state.completePending) {
        qWarning("QQmlComponent: Cannot create new component instance before completing the previous");
        return 0;
    }

    if (!q->isReady()) {
        qWarning("QQmlComponent: Component is not ready");
        return 0;
    }

    QQmlEnginePrivate *enginePriv = QQmlEnginePrivate::get(engine);

    if (enginePriv->inProgressCreations == 0) {
        // only track root, since further ones might not be properly nested
        profiler = new QQmlObjectCreatingProfiler();
    }

    enginePriv->inProgressCreations++;
    state.errors.clear();
    state.completePending = true;

    enginePriv->referenceScarceResources();
    state.vme.init(context, cc, start, creationContext);
    QObject *rv = state.vme.execute(&state.errors);
    enginePriv->dereferenceScarceResources();

    if (rv) {
        QQmlData *ddata = QQmlData::get(rv);
        Q_ASSERT(ddata);
        ddata->indestructible = true;
    }

    if (enginePriv->isDebugging && rv) {
        if (!context->isInternal)
            context->asQQmlContextPrivate()->instances.append(rv);
        QQmlEngineDebugService::instance()->objectCreated(engine, rv);

        if (profiler && profiler->enabled) {
            profiler->setTypeName(buildTypeNameForDebug(rv->metaObject()));
            QQmlData *data = QQmlData::get(rv);
            Q_ASSERT(data);
            profiler->setLocation(cc->url, data->lineNumber, data->columnNumber);
        }
    }

    return rv;
}

void QQmlComponentPrivate::beginDeferred(QQmlEnginePrivate *enginePriv,
                                                 QObject *object, ConstructionState *state)
{
    enginePriv->inProgressCreations++;
    state->errors.clear();
    state->completePending = true;

    state->vme.initDeferred(object);
    state->vme.execute(&state->errors);
}

void QQmlComponentPrivate::complete(QQmlEnginePrivate *enginePriv, ConstructionState *state)
{
    if (state->completePending) {
        state->vme.complete();

        state->completePending = false;

        enginePriv->inProgressCreations--;

        if (0 == enginePriv->inProgressCreations) {
            while (enginePriv->erroredBindings) {
                enginePriv->warning(enginePriv->erroredBindings->error);
                enginePriv->erroredBindings->removeError();
            }
        }
    }
}

/*!
    This method provides more advanced control over component instance creation.
    In general, programmers should use QQmlComponent::create() to create a 
    component.

    Complete a component creation begin with QQmlComponent::beginCreate().
*/
void QQmlComponent::completeCreate()
{
    Q_D(QQmlComponent);

    d->completeCreate();
}

void QQmlComponentPrivate::completeCreate()
{
    if (state.completePending) {
        QQmlEnginePrivate *ep = QQmlEnginePrivate::get(engine);
        complete(ep, &state);

        delete profiler;
        profiler = 0;
    }
}

QQmlComponentAttached::QQmlComponentAttached(QObject *parent)
: QObject(parent), prev(0), next(0)
{
}

QQmlComponentAttached::~QQmlComponentAttached()
{
    if (prev) *prev = next;
    if (next) next->prev = prev;
    prev = 0;
    next = 0;
}

/*!
    \internal
*/
QQmlComponentAttached *QQmlComponent::qmlAttachedProperties(QObject *obj)
{
    QQmlComponentAttached *a = new QQmlComponentAttached(obj);

    QQmlEngine *engine = qmlEngine(obj);
    if (!engine)
        return a;

    if (QQmlEnginePrivate::get(engine)->activeVME) { // XXX should only be allowed during begin
        QQmlEnginePrivate *p = QQmlEnginePrivate::get(engine);
        a->add(&p->activeVME->componentAttached);
    } else {
        QQmlData *d = QQmlData::get(obj);
        Q_ASSERT(d);
        Q_ASSERT(d->context);
        a->add(&d->context->componentAttached);
    }

    return a;
}

void QQmlComponent::create(QQmlIncubator &i, QQmlContext *context,
                                   QQmlContext *forContext)
{
    Q_D(QQmlComponent);

    if (!context) 
        context = d->engine->rootContext();

    QQmlContextData *contextData = QQmlContextData::get(context);
    QQmlContextData *forContextData = contextData;
    if (forContext) forContextData = QQmlContextData::get(forContext);

    if (!contextData->isValid()) {
        qWarning("QQmlComponent: Cannot create a component in an invalid context");
        return;
    }

    if (contextData->engine != d->engine) {
        qWarning("QQmlComponent: Must create component in context from the same QQmlEngine");
        return;
    }

    if (!isReady()) {
        qWarning("QQmlComponent: Component is not ready");
        return;
    }

    i.clear();
    QQmlIncubatorPrivate *p = i.d;

    QQmlEnginePrivate *enginePriv = QQmlEnginePrivate::get(d->engine);

    p->component = d->cc; p->component->addref();
    p->vme.init(contextData, d->cc, d->start, d->creationContext);

    enginePriv->incubate(i, forContextData);
}

class QV8IncubatorResource : public QV8ObjectResource,
                             public QQmlIncubator
{
V8_RESOURCE_TYPE(IncubatorType)
public:
    QV8IncubatorResource(QV8Engine *engine, IncubationMode = Asynchronous);

    static v8::Handle<v8::Value> StatusChangedGetter(v8::Local<v8::String>, 
                                                     const v8::AccessorInfo& info);
    static v8::Handle<v8::Value> StatusGetter(v8::Local<v8::String>, 
                                              const v8::AccessorInfo& info);
    static v8::Handle<v8::Value> ObjectGetter(v8::Local<v8::String>, 
                                              const v8::AccessorInfo& info);
    static v8::Handle<v8::Value> ForceCompletionGetter(v8::Local<v8::String>, 
                                                       const v8::AccessorInfo& info);
    static v8::Handle<v8::Value> ForceCompletion(const v8::Arguments &args);

    static void StatusChangedSetter(v8::Local<v8::String>, v8::Local<v8::Value> value, 
                                    const v8::AccessorInfo& info);

    void dispose();

    v8::Persistent<v8::Object> me;
    QQmlGuard<QObject> parent;
    v8::Persistent<v8::Value> valuemap;
    v8::Persistent<v8::Object> qmlGlobal;
protected:
    virtual void statusChanged(Status);
    virtual void setInitialState(QObject *);
};

static void QQmlComponent_setQmlParent(QObject *me, QObject *parent)
{
    if (parent) {
        me->setParent(parent);
        typedef QQmlPrivate::AutoParentFunction APF;
        QList<APF> functions = QQmlMetaType::parentFunctions();

        bool needParent = false;
        for (int ii = 0; ii < functions.count(); ++ii) {
            QQmlPrivate::AutoParentResult res = functions.at(ii)(me, parent);
            if (res == QQmlPrivate::Parented) {
                needParent = false;
                break;
            } else if (res == QQmlPrivate::IncompatibleParent) {
                needParent = true;
            }
        }
        if (needParent) 
            qWarning("QQmlComponent: Created graphical object was not "
                     "placed in the graphics scene.");
    }
}

/*!
    \qmlmethod object Component::createObject(Item parent, object properties)

    Creates and returns an object instance of this component that will have
    the given \a parent and \a properties. The \a properties argument is optional.
    Returns null if object creation fails.

    The object will be created in the same context as the one in which the component
    was created. This function will always return null when called on components
    which were not created in QML.

    If you wish to create an object without setting a parent, specify \c null for
    the \a parent value. Note that if the returned object is to be displayed, you 
    must provide a valid \a parent value or set the returned object's \l{Item::parent}{parent} 
    property, or else the object will not be visible.

    If a \a parent is not provided to createObject(), a reference to the returned object must be held so that
    it is not destroyed by the garbage collector.  This is true regardless of whether \l{Item::parent} is set afterwards,
    since setting the Item parent does not change object ownership; only the graphical parent is changed.

    As of QtQuick 1.1, this method accepts an optional \a properties argument that specifies a
    map of initial property values for the created object. These values are applied before object
    creation is finalized. (This is more efficient than setting property values after object creation,
    particularly where large sets of property values are defined, and also allows property bindings
    to be set up before the object is created.)

    The \a properties argument is specified as a map of property-value items. For example, the code
    below creates an object with initial \c x and \c y values of 100 and 200, respectively:

    \js
        var component = Qt.createComponent("Button.qml");
        if (component.status == Component.Ready)
            component.createObject(parent, {"x": 100, "y": 100});
    \endjs

    Dynamically created instances can be deleted with the \c destroy() method.
    See \l {Dynamic Object Management in QML} for more information.
*/
void QQmlComponent::createObject(QQmlV8Function *args)
{
    Q_D(QQmlComponent);
    Q_ASSERT(d->engine);
    Q_ASSERT(args);

    QObject *parent = 0;
    v8::Local<v8::Object> valuemap;

    if (args->Length() >= 1) 
        parent = args->engine()->toQObject((*args)[0]);

    if (args->Length() >= 2) {
        v8::Local<v8::Value> v = (*args)[1];
        if (!v->IsObject() || v->IsArray()) {
            qmlInfo(this) << tr("createObject: value is not an object");
            args->returnValue(v8::Null());
            return;
        }
        valuemap = v8::Local<v8::Object>::Cast(v);
    }

    QV8Engine *v8engine = args->engine();

    QQmlContext *ctxt = creationContext();
    if (!ctxt) ctxt = d->engine->rootContext();

    QObject *rv = beginCreate(ctxt);

    if (!rv) {
        args->returnValue(v8::Null());
        return;
    }

    QQmlComponent_setQmlParent(rv, parent);

    v8::Handle<v8::Value> ov = v8engine->newQObject(rv);
    Q_ASSERT(ov->IsObject());
    v8::Handle<v8::Object> object = v8::Handle<v8::Object>::Cast(ov);

    if (!valuemap.IsEmpty()) {
        QQmlComponentExtension *e = componentExtension(v8engine);
        // Try catch isn't needed as the function itself is loaded with try/catch
        v8::Handle<v8::Value> function = e->initialProperties->Run(args->qmlGlobal());
        v8::Handle<v8::Value> args[] = { object, valuemap };
        v8::Handle<v8::Function>::Cast(function)->Call(v8engine->global(), 2, args);
    }

    d->completeCreate();

    Q_ASSERT(QQmlData::get(rv));
    QQmlData::get(rv)->setImplicitDestructible();

    if (!rv)
        args->returnValue(v8::Null());
    else
        args->returnValue(object);
}

/*!
    \qmlmethod object Component::incubateObject(Item parent, object properties, enum mode)

    Creates an incubator for instance of this component.  Incubators allow new component 
    instances to be instantiated asynchronously and not cause freezes in the UI.

    The \a parent argument specifies the parent the created instance will have.  Omitting the 
    parameter or passing null will create anobject with no parent.  In this case, a reference
    to the created object must be maintained by the application of the object will eventually
    be garbage collected.

    The \a properties argument is specified as a map of property-value items which will be
    set on the created object during its construction.  \a mode may be Qt.Synchronous or 
    Qt.Asynchronous and controls whether the instance is created synchronously or asynchronously. 
    The default is asynchronously.  In some circumstances, even if Qt.Synchronous is specified,
    the incubator may create the object asynchronously.  This happens if the component calling
    incubateObject() is itself being created asynchronously.

    All three arguments are optional.

    If successful, the method returns an incubator, otherwise null.  The incubator has the following
    properties:

    \list
    \i status The status of the incubator.  Valid values are Component.Ready, Component.Loading and
       Component.Error.
    \i object The created object instance.  Will only be available once the incubator is in the 
       Ready status.
    \i onStatusChanged Specifies a callback function to be invoked when the status changes.  The
       status is passed as a parameter to the callback.
    \i forceCompletion() Call to complete incubation synchronously.
    \endlist

    The following example demonstrates how to use an incubator:

    \js
        var component = Qt.createComponent("Button.qml");

        var incubator = component.incubateObject(parent, { x: 10, y: 10 });
        if (incubator.status != Component.Ready) {
            incubator.onStatusChanged = function(status) {
                if (status == Component.Ready) {
                    print ("Object", incubator.object, "is now ready!");
                }
            }
        } else {
            print ("Object", incubator.object, "is ready immediately!");
        }
    \endjs
*/

void QQmlComponent::incubateObject(QQmlV8Function *args)
{
    Q_D(QQmlComponent);
    Q_ASSERT(d->engine);
    Q_UNUSED(d);
    Q_ASSERT(args);

    QObject *parent = 0;
    v8::Local<v8::Object> valuemap;
    QQmlIncubator::IncubationMode mode = QQmlIncubator::Asynchronous;

    if (args->Length() >= 1) 
        parent = args->engine()->toQObject((*args)[0]);

    if (args->Length() >= 2) {
        v8::Local<v8::Value> v = (*args)[1];
        if (v->IsNull()) {
        } else if (!v->IsObject() || v->IsArray()) {
            qmlInfo(this) << tr("createObject: value is not an object");
            args->returnValue(v8::Null());
            return;
        } else {
            valuemap = v8::Local<v8::Object>::Cast(v);
        }
    }

    if (args->Length() >= 3) {
        quint32 v = (*args)[2]->Uint32Value();
        if (v == 0)
            mode = QQmlIncubator::Asynchronous;
        else if (v == 1)
            mode = QQmlIncubator::AsynchronousIfNested;
    }

    QQmlComponentExtension *e = componentExtension(args->engine());
    
    QV8IncubatorResource *r = new QV8IncubatorResource(args->engine(), mode);
    v8::Local<v8::Object> o = e->incubationConstructor->NewInstance();
    o->SetExternalResource(r);

    if (!valuemap.IsEmpty()) {
        r->valuemap = qPersistentNew(valuemap);
        r->qmlGlobal = qPersistentNew(args->qmlGlobal());
    }
    r->parent = parent;
    r->me = qPersistentNew(o);

    create(*r, creationContext());

    if (r->status() == QQmlIncubator::Null) {
        r->dispose();
        args->returnValue(v8::Null());
    } else {
        args->returnValue(o);
    }
}

// XXX used by QSGLoader
void QQmlComponentPrivate::initializeObjectWithInitialProperties(v8::Handle<v8::Object> qmlGlobal, v8::Handle<v8::Object> valuemap, QObject *toCreate)
{
    QQmlEnginePrivate *ep = QQmlEnginePrivate::get(engine);
    QV8Engine *v8engine = ep->v8engine();

    v8::HandleScope handle_scope;
    v8::Context::Scope scope(v8engine->context());
    v8::Handle<v8::Value> ov = v8engine->newQObject(toCreate);
    Q_ASSERT(ov->IsObject());
    v8::Handle<v8::Object> object = v8::Handle<v8::Object>::Cast(ov);

    if (!valuemap.IsEmpty()) {
        QQmlComponentExtension *e = componentExtension(v8engine);
        // Try catch isn't needed as the function itself is loaded with try/catch
        v8::Handle<v8::Value> function = e->initialProperties->Run(qmlGlobal);
        v8::Handle<v8::Value> args[] = { object, valuemap };
        v8::Handle<v8::Function>::Cast(function)->Call(v8engine->global(), 2, args);
    }

    QQmlData *ddata = QQmlData::get(toCreate);
    Q_ASSERT(ddata);
    ddata->setImplicitDestructible();
}


QQmlComponentExtension::QQmlComponentExtension(QV8Engine *engine)
{
    v8::HandleScope handle_scope;
    v8::Context::Scope scope(engine->context());

    forceCompletion = qPersistentNew(V8FUNCTION(QV8IncubatorResource::ForceCompletion, engine));

    {
    v8::Local<v8::FunctionTemplate> ft = v8::FunctionTemplate::New();
    ft->InstanceTemplate()->SetHasExternalResource(true);
    ft->InstanceTemplate()->SetInternalFieldCount(1);
    ft->InstanceTemplate()->SetAccessor(v8::String::New("onStatusChanged"), 
                                        QV8IncubatorResource::StatusChangedGetter, 
                                        QV8IncubatorResource::StatusChangedSetter);
    ft->InstanceTemplate()->SetAccessor(v8::String::New("status"),
                                        QV8IncubatorResource::StatusGetter);
    ft->InstanceTemplate()->SetAccessor(v8::String::New("object"), 
                                        QV8IncubatorResource::ObjectGetter); 
    ft->InstanceTemplate()->SetAccessor(v8::String::New("forceCompletion"), 
                                        QV8IncubatorResource::ForceCompletionGetter); 
    incubationConstructor = qPersistentNew(ft->GetFunction());
    }

    {
#define INITIALPROPERTIES_SOURCE \
        "(function(object, values) {"\
            "try {"\
                "for(var property in values) {" \
                    "try {"\
                        "var properties = property.split(\".\");"\
                        "var o = object;"\
                        "for (var ii = 0; ii < properties.length - 1; ++ii) {"\
                            "o = o[properties[ii]];"\
                        "}"\
                        "o[properties[properties.length - 1]] = values[property];"\
                    "} catch(e) {}"\
                "}"\
            "} catch(e) {}"\
        "})"
    initialProperties = qPersistentNew(engine->qmlModeCompile(QLatin1String(INITIALPROPERTIES_SOURCE)));
#undef INITIALPROPERTIES_SOURCE
    }
}

v8::Handle<v8::Value> QV8IncubatorResource::ObjectGetter(v8::Local<v8::String>, 
                                                          const v8::AccessorInfo& info)
{
    QV8IncubatorResource *r = v8_resource_check<QV8IncubatorResource>(info.This());
    return r->engine->newQObject(r->object());
}

v8::Handle<v8::Value> QV8IncubatorResource::ForceCompletionGetter(v8::Local<v8::String>, 
                                                                  const v8::AccessorInfo& info)
{
    QV8IncubatorResource *r = v8_resource_check<QV8IncubatorResource>(info.This());
    return componentExtension(r->engine)->forceCompletion;
}

v8::Handle<v8::Value> QV8IncubatorResource::ForceCompletion(const v8::Arguments &args) 
{
    QV8IncubatorResource *r = v8_resource_cast<QV8IncubatorResource>(args.This());
    if (!r)
        V8THROW_TYPE("Not an incubator object");

    r->forceCompletion();

    return v8::Undefined();
}

v8::Handle<v8::Value> QV8IncubatorResource::StatusGetter(v8::Local<v8::String>, 
                                                         const v8::AccessorInfo& info)
{
    QV8IncubatorResource *r = v8_resource_check<QV8IncubatorResource>(info.This());
    return v8::Integer::NewFromUnsigned(r->status());
}

v8::Handle<v8::Value> QV8IncubatorResource::StatusChangedGetter(v8::Local<v8::String>, 
                                                                 const v8::AccessorInfo& info)
{
    return info.This()->GetInternalField(0);
}

void QV8IncubatorResource::StatusChangedSetter(v8::Local<v8::String>, v8::Local<v8::Value> value, 
                                                const v8::AccessorInfo& info)
{
    info.This()->SetInternalField(0, value);
}

QQmlComponentExtension::~QQmlComponentExtension()
{
    qPersistentDispose(incubationConstructor);
    qPersistentDispose(initialProperties);
    qPersistentDispose(forceCompletion);
}

QV8IncubatorResource::QV8IncubatorResource(QV8Engine *engine, IncubationMode m)
: QV8ObjectResource(engine), QQmlIncubator(m)
{
}

void QV8IncubatorResource::setInitialState(QObject *o)
{
    QQmlComponent_setQmlParent(o, parent);

    if (!valuemap.IsEmpty()) {
        QQmlComponentExtension *e = componentExtension(engine);

        v8::HandleScope handle_scope;
        v8::Context::Scope scope(engine->context());

        v8::Handle<v8::Value> function = e->initialProperties->Run(qmlGlobal);
        v8::Handle<v8::Value> args[] = { engine->newQObject(o), valuemap };
        v8::Handle<v8::Function>::Cast(function)->Call(engine->global(), 2, args);

        qPersistentDispose(valuemap);
        qPersistentDispose(qmlGlobal);
    }
}
    
void QV8IncubatorResource::dispose()
{
    qPersistentDispose(valuemap);
    qPersistentDispose(qmlGlobal);
    // No further status changes are forthcoming, so we no long need a self reference
    qPersistentDispose(me);
}

void QV8IncubatorResource::statusChanged(Status s)
{
    if (s == Ready) {
        Q_ASSERT(QQmlData::get(object()));
        QQmlData::get(object())->setImplicitDestructible();
    }

    if (!me.IsEmpty()) { // Will be false in synchronous mode
        v8::HandleScope scope;
        v8::Local<v8::Value> callback = me->GetInternalField(0);

        if (!callback.IsEmpty() && !callback->IsUndefined()) {

            if (callback->IsFunction()) {
                v8::Context::Scope context_scope(engine->context());
                v8::Local<v8::Function> f = v8::Local<v8::Function>::Cast(callback);
                v8::Handle<v8::Value> args[] = { v8::Integer::NewFromUnsigned(s) };
                v8::TryCatch tc;
                f->Call(me, 1, args);
                if (tc.HasCaught()) {
                    QQmlError error;
                    QQmlExpressionPrivate::exceptionToError(tc.Message(), error);
                    QQmlEnginePrivate::warning(QQmlEnginePrivate::get(engine->engine()),
                                                       error);
                }
            }
        }
    }

    if (s == Ready || s == Error) 
        dispose();
}

QT_END_NAMESPACE