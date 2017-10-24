/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the test suite of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:GPL-EXCEPT$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include <qstandardpaths.h>
#include <qtest.h>
#include <qqml.h>
#include <qqmlprivate.h>
#include <qqmlengine.h>
#include <qqmlcomponent.h>

#include <private/qqmlmetatype_p.h>
#include <private/qqmlpropertyvalueinterceptor_p.h>
#include <private/qhashedstring_p.h>
#include "../../shared/util.h"

class tst_qqmlmetatype : public QQmlDataTest
{
    Q_OBJECT
public:
    tst_qqmlmetatype() {}

private slots:
    void initTestCase();

    void qmlParserStatusCast();
    void qmlPropertyValueSourceCast();
    void qmlPropertyValueInterceptorCast();
    void qmlType();
    void invalidQmlTypeName();
    void prettyTypeName();
    void registrationType();
    void compositeType();
    void externalEnums();

    void isList();

    void defaultObject();
};

class TestType : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int foo READ foo)

    Q_CLASSINFO("DefaultProperty", "foo")
public:
    int foo() { return 0; }
};
QML_DECLARE_TYPE(TestType);

class TestType2 : public QObject
{
    Q_OBJECT
};

class TestType3 : public QObject
{
    Q_OBJECT
};

class ExternalEnums : public QObject
{
    Q_OBJECT
    Q_ENUMS(QStandardPaths::StandardLocation QStandardPaths::LocateOptions)
public:
    ExternalEnums(QObject *parent = nullptr) : QObject(parent) {}

    static QObject *create(QQmlEngine *engine, QJSEngine *scriptEngine) {
        Q_UNUSED(scriptEngine);
        return new ExternalEnums(engine);
    }
};
QML_DECLARE_TYPE(ExternalEnums);

QObject *testTypeProvider(QQmlEngine *engine, QJSEngine *scriptEngine)
{
    Q_UNUSED(engine);
    Q_UNUSED(scriptEngine);
    return new TestType();
}

class ParserStatusTestType : public QObject, public QQmlParserStatus
{
    Q_OBJECT
    void classBegin(){}
    void componentComplete(){}
    Q_CLASSINFO("DefaultProperty", "foo") // Missing default property
    Q_INTERFACES(QQmlParserStatus)
};
QML_DECLARE_TYPE(ParserStatusTestType);

class ValueSourceTestType : public QObject, public QQmlPropertyValueSource
{
    Q_OBJECT
    Q_INTERFACES(QQmlPropertyValueSource)
public:
    virtual void setTarget(const QQmlProperty &) {}
};
QML_DECLARE_TYPE(ValueSourceTestType);

class ValueInterceptorTestType : public QObject, public QQmlPropertyValueInterceptor
{
    Q_OBJECT
    Q_INTERFACES(QQmlPropertyValueInterceptor)
public:
    virtual void setTarget(const QQmlProperty &) {}
    virtual void write(const QVariant &) {}
};
QML_DECLARE_TYPE(ValueInterceptorTestType);

void tst_qqmlmetatype::initTestCase()
{
    QQmlDataTest::initTestCase();
    qmlRegisterType<TestType>("Test", 1, 0, "TestType");
    qmlRegisterSingletonType<TestType>("Test", 1, 0, "TestTypeSingleton", testTypeProvider);
    qmlRegisterType<ParserStatusTestType>("Test", 1, 0, "ParserStatusTestType");
    qmlRegisterType<ValueSourceTestType>("Test", 1, 0, "ValueSourceTestType");
    qmlRegisterType<ValueInterceptorTestType>("Test", 1, 0, "ValueInterceptorTestType");

    QUrl testTypeUrl(testFileUrl("CompositeType.qml"));
    qmlRegisterType(testTypeUrl, "Test", 1, 0, "TestTypeComposite");
}

void tst_qqmlmetatype::qmlParserStatusCast()
{
    QVERIFY(!QQmlMetaType::qmlType(QVariant::Int).isValid());
    QVERIFY(QQmlMetaType::qmlType(qMetaTypeId<TestType *>()).isValid());
    QCOMPARE(QQmlMetaType::qmlType(qMetaTypeId<TestType *>()).parserStatusCast(), -1);
    QVERIFY(QQmlMetaType::qmlType(qMetaTypeId<ValueSourceTestType *>()).isValid());
    QCOMPARE(QQmlMetaType::qmlType(qMetaTypeId<ValueSourceTestType *>()).parserStatusCast(), -1);

    QVERIFY(QQmlMetaType::qmlType(qMetaTypeId<ParserStatusTestType *>()).isValid());
    int cast = QQmlMetaType::qmlType(qMetaTypeId<ParserStatusTestType *>()).parserStatusCast();
    QVERIFY(cast != -1);
    QVERIFY(cast != 0);

    ParserStatusTestType t;
    QVERIFY(reinterpret_cast<char *>((QObject *)&t) != reinterpret_cast<char *>((QQmlParserStatus *)&t));

    QQmlParserStatus *status = reinterpret_cast<QQmlParserStatus *>(reinterpret_cast<char *>((QObject *)&t) + cast);
    QCOMPARE(status, (QQmlParserStatus*)&t);
}

void tst_qqmlmetatype::qmlPropertyValueSourceCast()
{
    QVERIFY(!QQmlMetaType::qmlType(QVariant::Int).isValid());
    QVERIFY(QQmlMetaType::qmlType(qMetaTypeId<TestType *>()).isValid());
    QCOMPARE(QQmlMetaType::qmlType(qMetaTypeId<TestType *>()).propertyValueSourceCast(), -1);
    QVERIFY(QQmlMetaType::qmlType(qMetaTypeId<ParserStatusTestType *>()).isValid());
    QCOMPARE(QQmlMetaType::qmlType(qMetaTypeId<ParserStatusTestType *>()).propertyValueSourceCast(), -1);

    QVERIFY(QQmlMetaType::qmlType(qMetaTypeId<ValueSourceTestType *>()).isValid());
    int cast = QQmlMetaType::qmlType(qMetaTypeId<ValueSourceTestType *>()).propertyValueSourceCast();
    QVERIFY(cast != -1);
    QVERIFY(cast != 0);

    ValueSourceTestType t;
    QVERIFY(reinterpret_cast<char *>((QObject *)&t) != reinterpret_cast<char *>((QQmlPropertyValueSource *)&t));

    QQmlPropertyValueSource *source = reinterpret_cast<QQmlPropertyValueSource *>(reinterpret_cast<char *>((QObject *)&t) + cast);
    QCOMPARE(source, (QQmlPropertyValueSource*)&t);
}

void tst_qqmlmetatype::qmlPropertyValueInterceptorCast()
{
    QVERIFY(!QQmlMetaType::qmlType(QVariant::Int).isValid());
    QVERIFY(QQmlMetaType::qmlType(qMetaTypeId<TestType *>()).isValid());
    QCOMPARE(QQmlMetaType::qmlType(qMetaTypeId<TestType *>()).propertyValueInterceptorCast(), -1);
    QVERIFY(QQmlMetaType::qmlType(qMetaTypeId<ParserStatusTestType *>()).isValid());
    QCOMPARE(QQmlMetaType::qmlType(qMetaTypeId<ParserStatusTestType *>()).propertyValueInterceptorCast(), -1);

    QVERIFY(QQmlMetaType::qmlType(qMetaTypeId<ValueInterceptorTestType *>()).isValid());
    int cast = QQmlMetaType::qmlType(qMetaTypeId<ValueInterceptorTestType *>()).propertyValueInterceptorCast();
    QVERIFY(cast != -1);
    QVERIFY(cast != 0);

    ValueInterceptorTestType t;
    QVERIFY(reinterpret_cast<char *>((QObject *)&t) != reinterpret_cast<char *>((QQmlPropertyValueInterceptor *)&t));

    QQmlPropertyValueInterceptor *interceptor = reinterpret_cast<QQmlPropertyValueInterceptor *>(reinterpret_cast<char *>((QObject *)&t) + cast);
    QCOMPARE(interceptor, (QQmlPropertyValueInterceptor*)&t);
}

void tst_qqmlmetatype::qmlType()
{
    QQmlType type = QQmlMetaType::qmlType(QString("ParserStatusTestType"), QString("Test"), 1, 0);
    QVERIFY(type.isValid());
    QVERIFY(type.module() == QLatin1String("Test"));
    QVERIFY(type.elementName() == QLatin1String("ParserStatusTestType"));
    QCOMPARE(type.qmlTypeName(), QLatin1String("Test/ParserStatusTestType"));

    type = QQmlMetaType::qmlType("Test/ParserStatusTestType", 1, 0);
    QVERIFY(type.isValid());
    QVERIFY(type.module() == QLatin1String("Test"));
    QVERIFY(type.elementName() == QLatin1String("ParserStatusTestType"));
    QCOMPARE(type.qmlTypeName(), QLatin1String("Test/ParserStatusTestType"));
}

void tst_qqmlmetatype::invalidQmlTypeName()
{
    QStringList currFailures = QQmlMetaType::typeRegistrationFailures();
    QCOMPARE(qmlRegisterType<TestType>("TestNamespace", 1, 0, "Test$Type"), -1); // should fail due to invalid QML type name.
    QCOMPARE(qmlRegisterType<TestType>("Test", 1, 0, "EndingInSlash/"), -1);
    QStringList nowFailures = QQmlMetaType::typeRegistrationFailures();

    foreach (const QString &f, currFailures)
        nowFailures.removeOne(f);

    QCOMPARE(nowFailures.size(), 2);
    QCOMPARE(nowFailures.at(0), QStringLiteral("Invalid QML element name \"Test$Type\""));
    QCOMPARE(nowFailures.at(1), QStringLiteral("Invalid QML element name \"EndingInSlash/\""));
}

void tst_qqmlmetatype::prettyTypeName()
{
    TestType2 obj2;
    QCOMPARE(QQmlMetaType::prettyTypeName(&obj2), QString("TestType2"));
    QVERIFY(qmlRegisterType<TestType2>("Test", 1, 0, "") >= 0);
    QCOMPARE(QQmlMetaType::prettyTypeName(&obj2), QString("TestType2"));

    TestType3 obj3;
    QCOMPARE(QQmlMetaType::prettyTypeName(&obj3), QString("TestType3"));
    QVERIFY(qmlRegisterType<TestType3>("Test", 1, 0, "OtherName") >= 0);
    QCOMPARE(QQmlMetaType::prettyTypeName(&obj3), QString("OtherName"));
}

void tst_qqmlmetatype::isList()
{
    QCOMPARE(QQmlMetaType::isList(QVariant::Invalid), false);
    QCOMPARE(QQmlMetaType::isList(QVariant::Int), false);

    QQmlListProperty<TestType> list;

    QCOMPARE(QQmlMetaType::isList(qMetaTypeId<QQmlListProperty<TestType> >()), true);
}

void tst_qqmlmetatype::defaultObject()
{
    QVERIFY(!QQmlMetaType::defaultProperty(&QObject::staticMetaObject).name());
    QVERIFY(!QQmlMetaType::defaultProperty(&ParserStatusTestType::staticMetaObject).name());
    QCOMPARE(QString(QQmlMetaType::defaultProperty(&TestType::staticMetaObject).name()), QString("foo"));

    QObject o;
    TestType t;
    ParserStatusTestType p;

    QVERIFY(QQmlMetaType::defaultProperty((QObject *)0).name() == 0);
    QVERIFY(!QQmlMetaType::defaultProperty(&o).name());
    QVERIFY(!QQmlMetaType::defaultProperty(&p).name());
    QCOMPARE(QString(QQmlMetaType::defaultProperty(&t).name()), QString("foo"));
}

void tst_qqmlmetatype::registrationType()
{
    QQmlType type = QQmlMetaType::qmlType(QString("TestType"), QString("Test"), 1, 0);
    QVERIFY(type.isValid());
    QVERIFY(!type.isInterface());
    QVERIFY(!type.isSingleton());
    QVERIFY(!type.isComposite());

    type = QQmlMetaType::qmlType(QString("TestTypeSingleton"), QString("Test"), 1, 0);
    QVERIFY(type.isValid());
    QVERIFY(!type.isInterface());
    QVERIFY(type.isSingleton());
    QVERIFY(!type.isComposite());

    type = QQmlMetaType::qmlType(QString("TestTypeComposite"), QString("Test"), 1, 0);
    QVERIFY(type.isValid());
    QVERIFY(!type.isInterface());
    QVERIFY(!type.isSingleton());
    QVERIFY(type.isComposite());
}

void tst_qqmlmetatype::compositeType()
{
    QQmlEngine engine;

    //Loading the test file also loads all composite types it imports
    QQmlComponent c(&engine, testFileUrl("testImplicitComposite.qml"));
    QObject* obj = c.create();
    QVERIFY(obj);

    QQmlType type = QQmlMetaType::qmlType(QString("ImplicitType"), QString(""), 1, 0);
    QVERIFY(type.isValid());
    QVERIFY(type.module().isEmpty());
    QCOMPARE(type.elementName(), QLatin1String("ImplicitType"));
    QCOMPARE(type.qmlTypeName(), QLatin1String("ImplicitType"));
    QCOMPARE(type.sourceUrl(), testFileUrl("ImplicitType.qml"));
}

void tst_qqmlmetatype::externalEnums()
{
    QQmlEngine engine;
    qmlRegisterSingletonType<ExternalEnums>("x.y.z", 1, 0, "ExternalEnums", ExternalEnums::create);

    QQmlComponent c(&engine, testFileUrl("testExternalEnums.qml"));
    QObject *obj = c.create();
    QVERIFY(obj);
    QVariant a = obj->property("a");
    QCOMPARE(a.type(), QVariant::Int);
    QCOMPARE(a.toInt(), int(QStandardPaths::DocumentsLocation));
    QVariant b = obj->property("b");
    QCOMPARE(b.type(), QVariant::Int);
    QCOMPARE(b.toInt(), int(QStandardPaths::DocumentsLocation));

}

QTEST_MAIN(tst_qqmlmetatype)

#include "tst_qqmlmetatype.moc"
