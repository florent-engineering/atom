/*-----------------------------------------------------------------------------
| Copyright (c) 2013, Nucleic Development Team.
|
| Distributed under the terms of the Modified BSD License.
|
| The full license is in the file COPYING.txt, distributed with this software.
|----------------------------------------------------------------------------*/
#include "signal.h"
#include "atom.h"

#include <cppy/cppy.h>


#define FREELIST_MAX 128

#define atom_cast( o ) reinterpret_cast<Atom*>( o )
#define signal_cast( o ) reinterpret_cast<Signal*>( o )
#define pyobject_cast( o ) reinterpret_cast<PyObject*>( o )
#define boundsignal_cast( o ) reinterpret_cast<BoundSignal*>( o )


namespace atom
{

namespace
{

int numfree = 0;
BoundSignal* freelist[ FREELIST_MAX ];


void Signal_dealloc( Signal* self )
{
	self->ob_type->tp_free( pyobject_cast( self ) );
}


PyObject* Signal_descr_get( Signal* self, PyObject* obj, PyObject* type )
{
	if( !obj )
	{
		return cppy::incref( pyobject_cast( self ) );
	}
	if( !Atom::TypeCheck( obj ) )
	{
		return cppy::type_error( obj, "Atom" );
	}
	return BoundSignal::Create( self, atom_cast( obj ) );
};


int Signal_descr_set( Signal* self, PyObject* obj, PyObject* value )
{
	cppy::type_error( "can't modify signal" );
	return -1;
};


PyObject* BoundSignal_new( PyTypeObject* type, PyObject* args, PyObject* kwargs )
{
	if( kwargs )
	{
		return cppy::type_error( "BoundSignal() does not take keyword arguments" );
	}
	PyObject* sig;
	PyObject* atom;
	if( !PyArg_ParseTuple( args, "OO", &sig, &atom ) )
	{
		return 0;
	}
	if( !Signal::TypeCheck( sig ) )
	{
		return cppy::type_error( sig, "Signal" );
	}
	if( !Atom::TypeCheck( atom ) )
	{
		return cppy::type_error( atom, "Atom" );
	}
	return BoundSignal::Create( signal_cast( sig ), atom_cast( atom ) );
}


int BoundSignal_clear( BoundSignal* self )
{
	Py_CLEAR( self->m_signal );
	Py_CLEAR( self->m_atom );
	return 0;
}


int BoundSignal_traverse( BoundSignal* self, visitproc visit, void* arg )
{
	Py_VISIT( self->m_signal );
	Py_VISIT( self->m_atom );
	return 0;
}


void BoundSignal_dealloc( BoundSignal* self )
{
	PyObject_GC_UnTrack( self );
	BoundSignal_clear( self );
	if( numfree < FREELIST_MAX )
	{
		freelist[ numfree++ ] = self;
	}
	else
	{
		self->ob_type->tp_free( pyobject_cast( self ) );
	}
}


PyObject* BoundSignal_richcompare( BoundSignal* self, PyObject* rhs, int op )
{
	if( op == Py_EQ || op == Py_NE )
	{
		bool res = false;
		if( BoundSignal::TypeCheck( rhs ) )
		{
			BoundSignal* other = boundsignal_cast( rhs );
			res = self->m_signal == other->m_signal && self->m_atom == other->m_atom;
		}
		if( op == Py_NE )
		{
			res = !res;
		}
		return cppy::incref( res ? Py_True : Py_False );
	}
	return cppy::incref( Py_NotImplemented );
}


PyObject* BoundSignal_call( BoundSignal* self, PyObject* args, PyObject* kwargs )
{
	self->m_atom->emit( self->m_signal, args, kwargs );
	return cppy::incref( Py_None );
}


PyObject* BoundSignal_connect( BoundSignal* self, PyObject* callback )
{
	if( !PyCallable_Check( callback ) )
	{
		return cppy::type_error( callback, "callable" );
	}
	return self->m_atom->connect( self->m_signal, callback );
}


PyObject* BoundSignal_disconnect( BoundSignal* self, PyObject* args )
{
	PyObject* callback = 0;
	if( !PyArg_ParseTuple( args, "|O", &callback ) )
	{
		return 0;
	}
	if( callback && !PyCallable_Check( callback ) )
	{
		return cppy::type_error( callback, "callable" );
	}
	if( !callback )
	{
		self->m_atom->disconnect( self->m_signal );
	}
	else
	{
		self->m_atom->disconnect( self->m_signal, callback );
	}
	return cppy::incref( Py_None );
}


PyMethodDef BoundSignal_methods[] = {
	{ "connect",
	  ( PyCFunction )BoundSignal_connect,
	  METH_O,
	  "connect(callback) connect a callback to the signal" },
	{ "disconnect",
	  ( PyCFunction )BoundSignal_disconnect,
	  METH_VARARGS,
	  "disconnect([callback]) disconnect a callback from the signal" },
	{ 0 } // sentinel
};

} // namespace


PyTypeObject Signal::TypeObject = {
	PyObject_HEAD_INIT( 0 )
	0,                                      /* ob_size */
	"atom.catom.Signal",                    /* tp_name */
	sizeof( Signal ),                       /* tp_basicsize */
	0,                                      /* tp_itemsize */
	(destructor)Signal_dealloc,             /* tp_dealloc */
	(printfunc)0,                           /* tp_print */
	(getattrfunc)0,                         /* tp_getattr */
	(setattrfunc)0,                         /* tp_setattr */
	(cmpfunc)0,                             /* tp_compare */
	(reprfunc)0,                            /* tp_repr */
	(PyNumberMethods*)0,                    /* tp_as_number */
	(PySequenceMethods*)0,                  /* tp_as_sequence */
	(PyMappingMethods*)0,                   /* tp_as_mapping */
	(hashfunc)0,                            /* tp_hash */
	(ternaryfunc)0,                         /* tp_call */
	(reprfunc)0,                            /* tp_str */
	(getattrofunc)0,                        /* tp_getattro */
	(setattrofunc)0,                        /* tp_setattro */
	(PyBufferProcs*)0,                      /* tp_as_buffer */
	Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE, /* tp_flags */
	0,                                      /* Documentation string */
	(traverseproc)0,     										/* tp_traverse */
	(inquiry)0,            								  /* tp_clear */
	(richcmpfunc)0,											    /* tp_richcompare */
	0,                                      /* tp_weaklistoffset */
	(getiterfunc)0,                         /* tp_iter */
	(iternextfunc)0,                        /* tp_iternext */
	(struct PyMethodDef*)0,								  /* tp_methods */
	(struct PyMemberDef*)0,                 /* tp_members */
	0,                                      /* tp_getset */
	0,                                      /* tp_base */
	0,                                      /* tp_dict */
	(descrgetfunc)Signal_descr_get,         /* tp_descr_get */
	(descrsetfunc)Signal_descr_set,         /* tp_descr_set */
	0,                                      /* tp_dictoffset */
	(initproc)0,                            /* tp_init */
	(allocfunc)PyType_GenericAlloc,         /* tp_alloc */
	(newfunc)PyType_GenericNew,             /* tp_new */
	(freefunc)PyObject_Del,                 /* tp_free */
	(inquiry)0,                             /* tp_is_gc */
	0,                                      /* tp_bases */
	0,                                      /* tp_mro */
	0,                                      /* tp_cache */
	0,                                      /* tp_subclasses */
	0,                                      /* tp_weaklist */
	(destructor)0                           /* tp_del */
};


PyTypeObject BoundSignal::TypeObject = {
	PyObject_HEAD_INIT( 0 )
	0,                                      /* ob_size */
	"atom.catom.BoundSignal",               /* tp_name */
	sizeof( BoundSignal ),                  /* tp_basicsize */
	0,                                      /* tp_itemsize */
	(destructor)BoundSignal_dealloc,        /* tp_dealloc */
	(printfunc)0,                           /* tp_print */
	(getattrfunc)0,                         /* tp_getattr */
	(setattrfunc)0,                         /* tp_setattr */
	(cmpfunc)0,                             /* tp_compare */
	(reprfunc)0,                            /* tp_repr */
	(PyNumberMethods*)0,                    /* tp_as_number */
	(PySequenceMethods*)0,                  /* tp_as_sequence */
	(PyMappingMethods*)0,                   /* tp_as_mapping */
	(hashfunc)0,                            /* tp_hash */
	(ternaryfunc)BoundSignal_call,          /* tp_call */
	(reprfunc)0,                            /* tp_str */
	(getattrofunc)0,                        /* tp_getattro */
	(setattrofunc)0,                        /* tp_setattro */
	(PyBufferProcs*)0,                      /* tp_as_buffer */
	Py_TPFLAGS_DEFAULT
	| Py_TPFLAGS_HAVE_GC
	| Py_TPFLAGS_HAVE_VERSION_TAG,  			  /* tp_flags */
	0,                                      /* Documentation string */
	(traverseproc)BoundSignal_traverse,     /* tp_traverse */
	(inquiry)BoundSignal_clear,             /* tp_clear */
	(richcmpfunc)BoundSignal_richcompare,   /* tp_richcompare */
	0,                                      /* tp_weaklistoffset */
	(getiterfunc)0,                         /* tp_iter */
	(iternextfunc)0,                        /* tp_iternext */
	(struct PyMethodDef*)BoundSignal_methods, /* tp_methods */
	(struct PyMemberDef*)0,                 /* tp_members */
	0,                                      /* tp_getset */
	0,                                      /* tp_base */
	0,                                      /* tp_dict */
	(descrgetfunc)0,                        /* tp_descr_get */
	(descrsetfunc)0,                        /* tp_descr_set */
	0,                                      /* tp_dictoffset */
	(initproc)0,                            /* tp_init */
	(allocfunc)PyType_GenericAlloc,         /* tp_alloc */
	(newfunc)BoundSignal_new,               /* tp_new */
	(freefunc)PyObject_GC_Del,              /* tp_free */
	(inquiry)0,                             /* tp_is_gc */
	0,                                      /* tp_bases */
	0,                                      /* tp_mro */
	0,                                      /* tp_cache */
	0,                                      /* tp_subclasses */
	0,                                      /* tp_weaklist */
	(destructor)0                           /* tp_del */
};


bool Signal::Ready()
{
	return PyType_Ready( &TypeObject ) == 0;
}


bool BoundSignal::Ready()
{
	return PyType_Ready( &TypeObject ) == 0;
}


PyObject* BoundSignal::Create( Signal* sig, Atom* atom )
{
	PyObject* pyo;
	if( numfree > 0 )
	{
		pyo = pyobject_cast( freelist[ --numfree ] );
		_Py_NewReference( pyo );
		PyObject_GC_Track( pyo );
	}
	else
	{
		pyo = PyType_GenericAlloc( &BoundSignal::TypeObject, 0 );
		if( !pyo )
		{
			return 0;
		}
	}
	BoundSignal* bsig = boundsignal_cast( pyo );
	bsig->m_signal = cppy::incref( sig );
	bsig->m_atom = cppy::incref( atom );
	return pyo;
}

} // namespace atom
