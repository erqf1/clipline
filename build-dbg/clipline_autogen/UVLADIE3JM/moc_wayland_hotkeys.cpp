/****************************************************************************
** Meta object code from reading C++ file 'wayland_hotkeys.h'
**
** Created by: The Qt Meta Object Compiler version 68 (Qt 6.4.2)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include <memory>
#include "../../../src/wayland_hotkeys.h"
#include <QtCore/qmetatype.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'wayland_hotkeys.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 68
#error "This file was generated using the moc from 6.4.2. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

#ifndef Q_CONSTINIT
#define Q_CONSTINIT
#endif

QT_BEGIN_MOC_NAMESPACE
QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
namespace {
struct qt_meta_stringdata_WaylandHotkeys_t {
    uint offsetsAndSizes[34];
    char stringdata0[15];
    char stringdata1[8];
    char stringdata2[1];
    char stringdata3[5];
    char stringdata4[12];
    char stringdata5[11];
    char stringdata6[9];
    char stringdata7[8];
    char stringdata8[12];
    char stringdata9[16];
    char stringdata10[8];
    char stringdata11[3];
    char stringdata12[10];
    char stringdata13[8];
    char stringdata14[19];
    char stringdata15[13];
    char stringdata16[4];
};
#define QT_MOC_LITERAL(ofs, len) \
    uint(sizeof(qt_meta_stringdata_WaylandHotkeys_t::offsetsAndSizes) + ofs), len 
Q_CONSTINIT static const qt_meta_stringdata_WaylandHotkeys_t qt_meta_stringdata_WaylandHotkeys = {
    {
        QT_MOC_LITERAL(0, 14),  // "WaylandHotkeys"
        QT_MOC_LITERAL(15, 7),  // "problem"
        QT_MOC_LITERAL(23, 0),  // ""
        QT_MOC_LITERAL(24, 4),  // "text"
        QT_MOC_LITERAL(29, 11),  // "keysChanged"
        QT_MOC_LITERAL(41, 10),  // "onResponse"
        QT_MOC_LITERAL(52, 8),  // "response"
        QT_MOC_LITERAL(61, 7),  // "results"
        QT_MOC_LITERAL(69, 11),  // "onActivated"
        QT_MOC_LITERAL(81, 15),  // "QDBusObjectPath"
        QT_MOC_LITERAL(97, 7),  // "session"
        QT_MOC_LITERAL(105, 2),  // "id"
        QT_MOC_LITERAL(108, 9),  // "timestamp"
        QT_MOC_LITERAL(118, 7),  // "options"
        QT_MOC_LITERAL(126, 18),  // "onShortcutsChanged"
        QT_MOC_LITERAL(145, 12),  // "QDBusMessage"
        QT_MOC_LITERAL(158, 3)   // "msg"
    },
    "WaylandHotkeys",
    "problem",
    "",
    "text",
    "keysChanged",
    "onResponse",
    "response",
    "results",
    "onActivated",
    "QDBusObjectPath",
    "session",
    "id",
    "timestamp",
    "options",
    "onShortcutsChanged",
    "QDBusMessage",
    "msg"
};
#undef QT_MOC_LITERAL
} // unnamed namespace

Q_CONSTINIT static const uint qt_meta_data_WaylandHotkeys[] = {

 // content:
      10,       // revision
       0,       // classname
       0,    0, // classinfo
       5,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       2,       // signalCount

 // signals: name, argc, parameters, tag, flags, initial metatype offsets
       1,    1,   44,    2, 0x06,    1 /* Public */,
       4,    0,   47,    2, 0x06,    3 /* Public */,

 // slots: name, argc, parameters, tag, flags, initial metatype offsets
       5,    2,   48,    2, 0x08,    4 /* Private */,
       8,    4,   53,    2, 0x08,    7 /* Private */,
      14,    1,   62,    2, 0x08,   12 /* Private */,

 // signals: parameters
    QMetaType::Void, QMetaType::QString,    3,
    QMetaType::Void,

 // slots: parameters
    QMetaType::Void, QMetaType::UInt, QMetaType::QVariantMap,    6,    7,
    QMetaType::Void, 0x80000000 | 9, QMetaType::QString, QMetaType::ULongLong, QMetaType::QVariantMap,   10,   11,   12,   13,
    QMetaType::Void, 0x80000000 | 15,   16,

       0        // eod
};

Q_CONSTINIT const QMetaObject WaylandHotkeys::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_meta_stringdata_WaylandHotkeys.offsetsAndSizes,
    qt_meta_data_WaylandHotkeys,
    qt_static_metacall,
    nullptr,
    qt_incomplete_metaTypeArray<qt_meta_stringdata_WaylandHotkeys_t,
        // Q_OBJECT / Q_GADGET
        QtPrivate::TypeAndForceComplete<WaylandHotkeys, std::true_type>,
        // method 'problem'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'keysChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'onResponse'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<uint, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QVariantMap &, std::false_type>,
        // method 'onActivated'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QDBusObjectPath &, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<qulonglong, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QVariantMap &, std::false_type>,
        // method 'onShortcutsChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QDBusMessage &, std::false_type>
    >,
    nullptr
} };

void WaylandHotkeys::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        auto *_t = static_cast<WaylandHotkeys *>(_o);
        (void)_t;
        switch (_id) {
        case 0: _t->problem((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 1: _t->keysChanged(); break;
        case 2: _t->onResponse((*reinterpret_cast< std::add_pointer_t<uint>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QVariantMap>>(_a[2]))); break;
        case 3: _t->onActivated((*reinterpret_cast< std::add_pointer_t<QDBusObjectPath>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[2])),(*reinterpret_cast< std::add_pointer_t<qulonglong>>(_a[3])),(*reinterpret_cast< std::add_pointer_t<QVariantMap>>(_a[4]))); break;
        case 4: _t->onShortcutsChanged((*reinterpret_cast< std::add_pointer_t<QDBusMessage>>(_a[1]))); break;
        default: ;
        }
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        switch (_id) {
        default: *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType(); break;
        case 3:
            switch (*reinterpret_cast<int*>(_a[1])) {
            default: *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType(); break;
            case 0:
                *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType::fromType< QDBusObjectPath >(); break;
            }
            break;
        case 4:
            switch (*reinterpret_cast<int*>(_a[1])) {
            default: *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType(); break;
            case 0:
                *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType::fromType< QDBusMessage >(); break;
            }
            break;
        }
    } else if (_c == QMetaObject::IndexOfMethod) {
        int *result = reinterpret_cast<int *>(_a[0]);
        {
            using _t = void (WaylandHotkeys::*)(const QString & );
            if (_t _q_method = &WaylandHotkeys::problem; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 0;
                return;
            }
        }
        {
            using _t = void (WaylandHotkeys::*)();
            if (_t _q_method = &WaylandHotkeys::keysChanged; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 1;
                return;
            }
        }
    }
}

const QMetaObject *WaylandHotkeys::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *WaylandHotkeys::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_WaylandHotkeys.stringdata0))
        return static_cast<void*>(this);
    return QObject::qt_metacast(_clname);
}

int WaylandHotkeys::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 5)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 5;
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 5)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 5;
    }
    return _id;
}

// SIGNAL 0
void WaylandHotkeys::problem(const QString & _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 0, _a);
}

// SIGNAL 1
void WaylandHotkeys::keysChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 1, nullptr);
}
QT_WARNING_POP
QT_END_MOC_NAMESPACE
