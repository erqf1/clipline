/****************************************************************************
** Meta object code from reading C++ file 'controller.h'
**
** Created by: The Qt Meta Object Compiler version 68 (Qt 6.4.2)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include <memory>
#include "../../../src/controller.h"
#include <QtCore/qmetatype.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'controller.h' doesn't include <QObject>."
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
struct qt_meta_stringdata_Controller_t {
    uint offsetsAndSizes[28];
    char stringdata0[11];
    char stringdata1[9];
    char stringdata2[1];
    char stringdata3[5];
    char stringdata4[3];
    char stringdata5[12];
    char stringdata6[9];
    char stringdata7[15];
    char stringdata8[2];
    char stringdata9[13];
    char stringdata10[12];
    char stringdata11[11];
    char stringdata12[14];
    char stringdata13[5];
};
#define QT_MOC_LITERAL(ofs, len) \
    uint(sizeof(qt_meta_stringdata_Controller_t::offsetsAndSizes) + ofs), len 
Q_CONSTINIT static const qt_meta_stringdata_Controller_t qt_meta_stringdata_Controller = {
    {
        QT_MOC_LITERAL(0, 10),  // "Controller"
        QT_MOC_LITERAL(11, 8),  // "clipDone"
        QT_MOC_LITERAL(20, 0),  // ""
        QT_MOC_LITERAL(21, 4),  // "path"
        QT_MOC_LITERAL(26, 2),  // "ok"
        QT_MOC_LITERAL(29, 11),  // "showGallery"
        QT_MOC_LITERAL(41, 8),  // "saveClip"
        QT_MOC_LITERAL(50, 14),  // "saveClipButton"
        QT_MOC_LITERAL(65, 1),  // "i"
        QT_MOC_LITERAL(67, 12),  // "openSettings"
        QT_MOC_LITERAL(80, 11),  // "togglePause"
        QT_MOC_LITERAL(92, 10),  // "toggleMute"
        QT_MOC_LITERAL(103, 13),  // "galleryClosed"
        QT_MOC_LITERAL(117, 4)   // "quit"
    },
    "Controller",
    "clipDone",
    "",
    "path",
    "ok",
    "showGallery",
    "saveClip",
    "saveClipButton",
    "i",
    "openSettings",
    "togglePause",
    "toggleMute",
    "galleryClosed",
    "quit"
};
#undef QT_MOC_LITERAL
} // unnamed namespace

Q_CONSTINIT static const uint qt_meta_data_Controller[] = {

 // content:
      10,       // revision
       0,       // classname
       0,    0, // classinfo
       9,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       1,       // signalCount

 // signals: name, argc, parameters, tag, flags, initial metatype offsets
       1,    2,   68,    2, 0x06,    1 /* Public */,

 // slots: name, argc, parameters, tag, flags, initial metatype offsets
       5,    0,   73,    2, 0x0a,    4 /* Public */,
       6,    0,   74,    2, 0x0a,    5 /* Public */,
       7,    1,   75,    2, 0x0a,    6 /* Public */,
       9,    0,   78,    2, 0x0a,    8 /* Public */,
      10,    0,   79,    2, 0x0a,    9 /* Public */,
      11,    0,   80,    2, 0x0a,   10 /* Public */,
      12,    0,   81,    2, 0x0a,   11 /* Public */,
      13,    0,   82,    2, 0x0a,   12 /* Public */,

 // signals: parameters
    QMetaType::Void, QMetaType::QString, QMetaType::Bool,    3,    4,

 // slots: parameters
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, QMetaType::Int,    8,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,

       0        // eod
};

Q_CONSTINIT const QMetaObject Controller::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_meta_stringdata_Controller.offsetsAndSizes,
    qt_meta_data_Controller,
    qt_static_metacall,
    nullptr,
    qt_incomplete_metaTypeArray<qt_meta_stringdata_Controller_t,
        // Q_OBJECT / Q_GADGET
        QtPrivate::TypeAndForceComplete<Controller, std::true_type>,
        // method 'clipDone'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<bool, std::false_type>,
        // method 'showGallery'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'saveClip'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'saveClipButton'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        // method 'openSettings'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'togglePause'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'toggleMute'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'galleryClosed'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'quit'
        QtPrivate::TypeAndForceComplete<void, std::false_type>
    >,
    nullptr
} };

void Controller::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        auto *_t = static_cast<Controller *>(_o);
        (void)_t;
        switch (_id) {
        case 0: _t->clipDone((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<bool>>(_a[2]))); break;
        case 1: _t->showGallery(); break;
        case 2: _t->saveClip(); break;
        case 3: _t->saveClipButton((*reinterpret_cast< std::add_pointer_t<int>>(_a[1]))); break;
        case 4: _t->openSettings(); break;
        case 5: _t->togglePause(); break;
        case 6: _t->toggleMute(); break;
        case 7: _t->galleryClosed(); break;
        case 8: _t->quit(); break;
        default: ;
        }
    } else if (_c == QMetaObject::IndexOfMethod) {
        int *result = reinterpret_cast<int *>(_a[0]);
        {
            using _t = void (Controller::*)(const QString & , bool );
            if (_t _q_method = &Controller::clipDone; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 0;
                return;
            }
        }
    }
}

const QMetaObject *Controller::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *Controller::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_Controller.stringdata0))
        return static_cast<void*>(this);
    return QObject::qt_metacast(_clname);
}

int Controller::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 9)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 9;
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 9)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 9;
    }
    return _id;
}

// SIGNAL 0
void Controller::clipDone(const QString & _t1, bool _t2)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t2))) };
    QMetaObject::activate(this, &staticMetaObject, 0, _a);
}
QT_WARNING_POP
QT_END_MOC_NAMESPACE
