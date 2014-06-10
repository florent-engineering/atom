/*-----------------------------------------------------------------------------
| Copyright (c) 2014, Nucleic Development Team.
|
| Distributed under the terms of the Modified BSD License.
|
| The full license is in the file COPYING.txt, distributed with this software.
|----------------------------------------------------------------------------*/
#include <algorithm>
#include <string.h>
#include <cppy/cppy.h>
#include <utils/utils.h>
#include "class_map.h"
#include "member.h"


void ClassMap_LookupMember( ClassMap* map,
                            PyStringObject* name,
                            Member** member,
                            uint32_t* index )
{
    uint32_t mask = map->allocated - 1;
    uint32_t hash = utils::pystr_hash( name );
    uint32_t bucket = hash & mask;
    ClassMapEntry* base = map->entries;
    while( true ) // table is never full - always terminates in loop
    {
        ClassMapEntry* entry = base + bucket;
        if( !entry->name )
        {
            return;
        }
        if( utils::pystr_equal( name, entry->name ) )
        {
            *member = entry->member;
            *index = entry->index;
            return;
        }
        // CPython's collision resolution scheme
        bucket = ( ( bucket << 2 ) + bucket + hash + 1 ) & mask;
        hash >>= 5;
    };
}


namespace
{

void insert_member( ClassMap* map, PyStringObject* name, Member* member )
{
    uint32_t mask = map->allocated - 1;
    uint32_t hash = utils::pystr_hash( name );
    uint32_t bucket = hash & mask;
    ClassMapEntry* base = map->entries;
    while( true ) // table is never full - always terminates in loop
    {
        // The table is pre-allocated with guaranteed sufficient space
        // and no two keys will be equal while populating the table.
        ClassMapEntry* entry = base + bucket;
        if( !entry->name )
        {
            entry->name = name;
            entry->member = member;
            entry->index = map->count++;
            Py_INCREF( name );
            Py_INCREF( member );
            return;
        }
        // CPython's collision resolution scheme
        bucket = ( ( bucket << 2 ) + bucket + hash + 1 ) & mask;
        hash >>= 5;
    };
}


PyObject* ClassMap_new( PyTypeObject* type, PyObject* args, PyObject* kwargs )
{
    static char* kwlist[] = {"members", 0};
    PyObject* members;
    if( !PyArg_ParseTupleAndKeywords(
            args, kwargs, "O:__new__", kwlist, &members ) )
    {
        return 0;
    }
    if( !PyDict_CheckExact( members ) )
    {
        return cppy::type_error( members, "dict" );
    }
    cppy::ptr self_ptr( PyType_GenericNew( type, 0, 0 ) );
    if( !self_ptr )
    {
        return 0;
    }
    uint32_t size = static_cast<uint32_t>( PyDict_Size( members ) );
    uint32_t count = std::max( size, static_cast<uint32_t>( 3 ) );
    uint32_t allocated = utils::next_power_of_2( count * 4 / 3 );
    size_t mem_size = sizeof( ClassMapEntry ) * allocated;
    void* entries = PyObject_Malloc( mem_size );
    if( !entries )
    {
        return PyErr_NoMemory();
    }
    memset( entries, 0, mem_size );
    ClassMap* map = ( ClassMap* )self_ptr.get();
    map->entries = ( ClassMapEntry* )entries;
    map->allocated = allocated;
    PyObject* key;
    PyObject* value;
    Py_ssize_t pos = 0;
    while( PyDict_Next( members, &pos, &key, &value ) )
    {
        if( !PyString_Check( key ) )
        {
            return cppy::type_error( key, "str" );
        }
        if( !Member_Check( value ) )
        {
            return cppy::type_error( value, "Member" );
        }
        insert_member( map, ( PyStringObject* )key, ( Member* )value );
    }
    return self_ptr.release();
}


void ClassMap_clear( ClassMap* self )
{
    uint32_t allocated = self->allocated;
    ClassMapEntry* base = self->entries;
    for( uint32_t i = 0; i < allocated; ++i )
    {
        ClassMapEntry* entry = base + i;
        if( entry->name )
        {
            Py_CLEAR( entry->name );
            Py_CLEAR( entry->member );
        }
    }
}


int ClassMap_traverse( ClassMap* self, visitproc visit, void* arg )
{
    uint32_t allocated = self->allocated;
    ClassMapEntry* base = self->entries;
    for( uint32_t i = 0; i < allocated; ++i )
    {
        ClassMapEntry* entry = base + i;
        if( entry->name )
        {
            Py_VISIT( entry->name );
            Py_VISIT( entry->member );
        }
    }
    return 0;
}


void ClassMap_dealloc( ClassMap* self )
{
    PyObject_GC_UnTrack( self );
    ClassMap_clear( self );
    PyObject_Free( self->entries );
    self->ob_type->tp_free( ( PyObject* )self );
}


PyObject* ClassMap_sizeof( ClassMap* self, PyObject* args )
{
    Py_ssize_t size = self->ob_type->tp_basicsize;
    size += sizeof( ClassMapEntry ) * self->allocated;
    return PyInt_FromSsize_t( size );
}


PyMethodDef ClassMap_methods[] = {
    {"__sizeof__",
     ( PyCFunction )ClassMap_sizeof,
     METH_NOARGS,
     "__sizeof__() -> size of object in memory, in bytes"},
    {0} // sentinel
};

} // namespace


PyTypeObject ClassMap_Type = {
    PyObject_HEAD_INIT( &PyType_Type )       /* header */
    0,                                       /* ob_size */
    "atom.catom.ClassMap",                   /* tp_name */
    sizeof( ClassMap ),                      /* tp_basicsize */
    0,                                       /* tp_itemsize */
    ( destructor )ClassMap_dealloc,          /* tp_dealloc */
    ( printfunc )0,                          /* tp_print */
    ( getattrfunc )0,                        /* tp_getattr */
    ( setattrfunc )0,                        /* tp_setattr */
    ( cmpfunc )0,                            /* tp_compare */
    ( reprfunc )0,                           /* tp_repr */
    ( PyNumberMethods* )0,                   /* tp_as_number */
    ( PySequenceMethods* )0,                 /* tp_as_sequence */
    ( PyMappingMethods* )0,                  /* tp_as_mapping */
    ( hashfunc )0,                           /* tp_hash */
    ( ternaryfunc )0,                        /* tp_call */
    ( reprfunc )0,                           /* tp_str */
    ( getattrofunc )0,                       /* tp_getattro */
    ( setattrofunc )0,                       /* tp_setattro */
    ( PyBufferProcs* )0,                     /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HAVE_GC, /* tp_flags */
    0,                                       /* Documentation string */
    ( traverseproc )ClassMap_traverse,       /* tp_traverse */
    ( inquiry )ClassMap_clear,               /* tp_clear */
    ( richcmpfunc )0,                        /* tp_richcompare */
    0,                                       /* tp_weaklistoffset */
    ( getiterfunc )0,                        /* tp_iter */
    ( iternextfunc )0,                       /* tp_iternext */
    ( struct PyMethodDef* )ClassMap_methods, /* tp_methods */
    ( struct PyMemberDef* )0,                /* tp_members */
    0,                                       /* tp_getset */
    0,                                       /* tp_base */
    0,                                       /* tp_dict */
    ( descrgetfunc )0,                       /* tp_descr_get */
    ( descrsetfunc )0,                       /* tp_descr_set */
    0,                                       /* tp_dictoffset */
    ( initproc )0,                           /* tp_init */
    ( allocfunc )PyType_GenericAlloc,        /* tp_alloc */
    ( newfunc )ClassMap_new,                 /* tp_new */
    ( freefunc )PyObject_GC_Del,             /* tp_free */
    ( inquiry )0,                            /* tp_is_gc */
    0,                                       /* tp_bases */
    0,                                       /* tp_mro */
    0,                                       /* tp_cache */
    0,                                       /* tp_subclasses */
    0,                                       /* tp_weaklist */
    ( destructor )0                          /* tp_del */
};


int import_class_map()
{
    if( PyType_Ready( &ClassMap_Type ) < 0 )
    {
        return -1;
    }
    return 0;
}