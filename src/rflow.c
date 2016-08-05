/******************************************************************************
* Copyright (C) 2016  Billy Kozak					      *
*									      *
* This file is part of python-rflow						 *
*									      *
* This program is free software: you can redistribute it and/or modify	      *
* it under the terms of the GNU General Public License as published by	      *
* the Free Software Foundation, either version 3 of the License, or	      *
* (at your option) any later version.					      *
*									      *
* This program is distributed in the hope that it will be useful,	      *
* but WITHOUT ANY WARRANTY; without even the implied warranty of	      *
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	See the		      *
* GNU General Public License for more details.				      *
*									      *
* You should have received a copy of the GNU General Public License	      *
* along with this program.  If not, see <http://www.gnu.org/licenses/>.	      *
******************************************************************************/
/******************************************************************************
*				   INCLUDES				      *
******************************************************************************/
#include <Python.h>
#include <lib-rflow.h>
#include <stdlib.h>
#include <assert.h>
#include <stdio.h>

#include "structmember.h"
/******************************************************************************
*				     TYPES				      *
******************************************************************************/
struct cycle_gen {
	PyObject_HEAD

	struct lib_rflow_state *rflow_state;

	struct lib_rflow_list   stored_cycles;
	size_t			stored_index;
	size_t                  stored_unprocessed;

	int			history_over;

	PyObject	       *itr;
};
/******************************************************************************
*				   CONSTANTS				      *
******************************************************************************/
const Py_ssize_t POINTS_PER_GEN = 1024;
/******************************************************************************
*			     FUNCTION DECLARATIONS			      *
******************************************************************************/
static PyObject *cycle_gen_next(struct	cycle_gen *self);
static PyObject *cycle_gen_new(PyTypeObject *type, PyObject *arg,
			       PyObject *kwds);
static int cycle_gen_init(struct cycle_gen *self, PyObject *args,
			  PyObject *kwds);
static void cycle_gen_dealloc(struct cycle_gen *self);
static PyObject *cycle_gen_alloc(PyTypeObject *type, Py_ssize_t nitems);

static int add_data_points(struct cycle_gen *self);
static PyObject *convert_float(PyObject *item);
static PyObject* rflow_compute_matrix(PyObject  *self, PyObject *args);
/******************************************************************************
*				     DATA				      *
******************************************************************************/
PyDoc_STRVAR(
	cycle_gen_doc,
	"Given a sequence which supplies data points, this generator returns "
	"cycle objects as they are created."
);
/*****************************************************************************/
static PyMethodDef rflow_methods[] = {
	{
		"compute_matrix", rflow_compute_matrix, METH_VARARGS,
		"Compute the Rainflow Matrix over some given data\n\n"
		"Arguments:\n"
		"data -- iterable which returns data points\n"
		"amp_bin_count -- number of amplitude bins\n"
		"mean_bin_count -- number of mean value bins\n"
		"mean_min -- minimum mean value to store in matrix\n"
		"amp_min -- minimum amplitude to store in matrix\n"
		"mean_bin_size --  the size of each mean value bin\n"
		"amp_bin_size -- the size of each amplitude bin\n"
	},
	{NULL, NULL, 0, NULL}
};
/*****************************************************************************/
static PyMethodDef cycle_gen_methods[] = {
	{NULL, NULL, 0, NULL}
};
/*****************************************************************************/
static PyMemberDef cycle_gen_members[] = {
	{NULL}
};
/*****************************************************************************/
static PyTypeObject cycle_gen_type = {
    PyObject_HEAD_INIT(NULL)
    0,                              /* ob_size */
    "rflow.cycle_gen",		    /* tp_name */
    sizeof(struct cycle_gen),	    /* tp_basicsize */
    0,				    /* tp_itemsize */
    (destructor)cycle_gen_dealloc,  /* tp_dealloc */
    0,				    /* tp_print */
    0,				    /* tp_getattr */
    0,				    /* tp_setattr */
    0,				    /* tp_reserved */
    0,				    /* tp_repr */
    0,				    /* tp_as_number */
    0,				    /* tp_as_sequence */
    0,				    /* tp_as_mapping */
    0,				    /* tp_hash */
    0,				    /* tp_call */
    0,				    /* tp_str */
    0,				    /* tp_getattro */
    0,				    /* tp_setattro */
    0,				    /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT,		    /* tp_flags */
    cycle_gen_doc,		    /* tp_doc */
    0,				    /* tp_traverse */
    0,				    /* tp_clear */
    0,				    /* tp_richcompare */
    0,				    /* tp_weaklistoffset */
    PyObject_SelfIter,		    /* tp_iter */
    (iternextfunc)cycle_gen_next,   /* tp_iternext */
    cycle_gen_methods,		    /* tp_methods */
    cycle_gen_members,              /* tp_members */
    0,				    /* tp_getset */
    0,				    /* tp_base */
    0,				    /* tp_dict */
    0,				    /* tp_descr_get */
    0,				    /* tp_descr_set */
    0,				    /* tp_dictoffset */
    (initproc)cycle_gen_init,	    /* tp_init */
    cycle_gen_alloc,		    /* tp_alloc */
    cycle_gen_new 		    /* tp_new */
};
/******************************************************************************
*			     FUNCTION DEFINITIONS			      *
******************************************************************************/
static PyObject *convert_float(PyObject *item)
{
	PyObject *args = PyTuple_Pack(1, item);
	if(args == NULL) {
		return NULL;
	}

	return PyObject_CallObject((PyObject *)&PyFloat_Type, args);
}
/*****************************************************************************/
static int compute_matrix_args(PyObject *args, struct lib_rflow_init *init,
                               PyObject **data)
{
	PyObject *iterable;
	int ok;

	if(PyTuple_Size(args) != 7) {
		PyErr_Format(
			PyExc_TypeError ,
			"compute_matrix takes exactly 7 arguments"
		);
		return 1;
	}

	ok = PyArg_ParseTuple(
		args, "Oiidddd",
		&iterable,
		&init->mode_data.matrix_data.amp_bin_count,
		&init->mode_data.matrix_data.mean_bin_count,
		&init->mode_data.matrix_data.mean_min,
		&init->mode_data.matrix_data.amp_min,
		&init->mode_data.matrix_data.mean_bin_size,
		&init->mode_data.matrix_data.amp_bin_size
	);
	if(ok == 0) {
		return 1;
	}

	if(init->mode_data.matrix_data.mean_bin_count <= 0) {
		PyErr_Format(
			PyExc_RuntimeError,
			"mean_bin_count must be positive"
		);
		return 1;
	}
	if(init->mode_data.matrix_data.amp_bin_count <= 0) {
		PyErr_Format(
			PyExc_RuntimeError,
			"amp_bin_count must be positive"
		);
		return 1;
	}

	if((*data = PyObject_GetIter(iterable)) == NULL) {
		return 1;
	}

	init->opts = LIB_RFLOW_MODE_MATRIX;

	return 0;
}
/*****************************************************************************/
static PyObject* python_matrix(const struct lib_rflow_matrix *m)
{
	PyObject *py_matrix = PyList_New(m->amp_bin_count);
	if(py_matrix == NULL) {
		goto fail;
	}

	for(size_t i = 0; i < m->amp_bin_count; i++) {
		PyObject *cell_val;
		PyObject *row = PyList_New(m->mean_bin_count);
		if(row == NULL) {
			goto fail;
		}

		for(size_t n = 0; n < m->mean_bin_count; n++) {
			cell_val = PyInt_FromLong(
				m->bins[i * m->mean_bin_count + n]
			);
			if(cell_val == NULL) {
				goto fail;
			}
			PyList_SET_ITEM(row, n, cell_val);
		}
		PyList_SET_ITEM(py_matrix, i, row);
	}

	return py_matrix;
fail:
	Py_XDECREF(py_matrix);
	return NULL;
}
/*****************************************************************************/
static PyObject* rflow_compute_matrix(PyObject  *self, PyObject *args)
{
	PyObject *ret = NULL;
	int err;
	const struct lib_rflow_matrix *matrix = NULL;
	struct lib_rflow_init init;
	PyObject *data = NULL;
	struct lib_rflow_state *state = NULL;

	if(compute_matrix_args(args, &init, &data)) {
		goto exit;
	}

	state = lib_rflow_init(&init);
	if(state == NULL) {
		PyErr_Format(
			PyExc_RuntimeError,
			"low level error in lib-rflow"
		);
		goto exit;
	}
	matrix = lib_rflow_get_matrix(state);
	if(matrix == NULL) {
		goto exit;
	}

	for(PyObject *i = PyIter_Next(data); i != NULL; i = PyIter_Next(data)){
		double point;
		PyObject *float_obj;

		if(PyFloat_Check(i)) {
			float_obj = i;
			i = NULL;
		} else {
			float_obj = convert_float(i);
		}

		if(float_obj == NULL) {
			Py_XDECREF(i);
			goto exit;
		}

		point = PyFloat_AS_DOUBLE(float_obj);
		err   = lib_rflow_count(state, &point, 1);


		Py_XDECREF(float_obj);
		Py_XDECREF(i);
		if(err) {
			PyErr_Format(
				PyExc_RuntimeError,
				"low level error in lib-rflow"
			);
			goto exit;
		}

	}
	if(lib_rflow_end_history(state)) {
		goto exit;
	}

	ret = python_matrix(matrix);
exit:
	free((void*)matrix);
	Py_XDECREF(data);
	if(state) {
		lib_rflow_destroy(state);
	}
	return ret;
}
/*****************************************************************************/
static int add_data_points(struct cycle_gen *self)
{
	int err              = 0;
	double point;
	PyObject *item	     = NULL;
	PyObject *float_obj  = NULL;

	for(Py_ssize_t i = 0; i < POINTS_PER_GEN; i++) {
		item = PyIter_Next(self->itr);

		if((item == NULL)) {
			if(PyErr_Occurred()) {
				goto fail;
			} else {
				if(lib_rflow_end_history(self->rflow_state)) {
					PyErr_Format(
						PyExc_RuntimeError,
						"low level error in lib-rflow"
					);
					goto fail;
				}
				self->history_over = 1;
				return 0;
			}
		} else if(PyFloat_Check(item)){
			float_obj = item;
		} else {
			float_obj = convert_float(item);
			Py_DECREF(item);
		}

		if(float_obj == NULL) {
			goto fail;
		}

		point = PyFloat_AS_DOUBLE(float_obj);
		err  = lib_rflow_count(self->rflow_state, &point, 1);
		Py_DECREF(float_obj);

		if(err) {
			PyErr_Format(
				PyExc_RuntimeError,
				"low level error in lib-rflow"
			);
			goto fail;
		}
	}

	return 0;
fail:
	return 1;
}
/*****************************************************************************/
static int store_cycles(struct cycle_gen *self)
{
	int err;

	lib_rflow_free_list(&self->stored_cycles);
	err = lib_rflow_pop_cycles(
		self->rflow_state, &self->stored_cycles
	);
	if(err) {
		PyErr_Format(
			PyExc_RuntimeError,
			"low level error in lib-rflow"
		);
		return -1;
	}
	return 0;
}
/*****************************************************************************/
static PyObject *create_cycle_obj(const struct lib_rflow_cycle *c)
{
	PyObject *start = PyFloat_FromDouble(c->cycle_start);
	PyObject *end	= PyFloat_FromDouble(c->cycle_end);
	PyObject *obj	= NULL;

	if(start == NULL || end == NULL) {
		goto fail;
	}

	obj = PyTuple_Pack(2, start, end);
	if(obj == NULL) {
		goto fail;
	}
	Py_DECREF(start);
	Py_DECREF(end);
	return obj;
fail:
	Py_XDECREF(start);
	Py_XDECREF(end);
	Py_XDECREF(obj);
	return NULL;
}
/*****************************************************************************/
static PyObject *cycle_gen_next(struct cycle_gen *self)
{
	PyObject *cycle;
	int ret	 = 0;
	size_t l_size;

	if(self->stored_index >= self->stored_cycles.num_cycles) {

		if(self->history_over) {
			return NULL;
		}

		self->stored_index = 0;

		do {
			ret = add_data_points(self);
			if(ret) {
				return NULL;
			}

			l_size = lib_rflow_cycle_list_size(self->rflow_state);

		} while((l_size == 0) && !self->history_over);

		if(l_size == 0) {
			return NULL;
		}

		ret = store_cycles(self);
		if(ret) {
			return NULL;
		}
	}

	cycle = create_cycle_obj(
		self->stored_cycles.cycles + self->stored_index
	);
	if(cycle != NULL) {
		self->stored_index += 1;
	}

	return cycle;
}
/*****************************************************************************/
static PyObject *cycle_gen_new(PyTypeObject *type, PyObject *arg,
			       PyObject *kwds)
{
	PyObject *itr;
	PyObject *iterable;
	struct cycle_gen *self;

	if(PyTuple_Size(arg) != 1 || kwds != 0) {
		PyErr_Format(
			PyExc_TypeError,
			"cycle_gen.new() takes exactly one argument."
		);
		return NULL;
	}

	iterable = PyTuple_GetItem(arg, 0);
	if(iterable == NULL) {
		return NULL;
	}
	if((itr = PyObject_GetIter(iterable)) == NULL) {
		return NULL;
	}
	if(!PyIter_Check(itr)) {
		PyErr_Format(PyExc_TypeError, "cycle_gen expects an iterable");
		return NULL;
	}

	self = (struct cycle_gen*)(type->tp_alloc(type, 0));
	if(self == NULL) {
		return NULL;
	}

	self->itr	    = itr;
	lib_rflow_get_empty_list(&self->stored_cycles);
	self->stored_index  = 0;
	self->history_over  = 0;
	return (PyObject *)self;
}
/*****************************************************************************/
static int cycle_gen_init(struct cycle_gen *self, PyObject *args,
			  PyObject *kwds)
{
	return 0;
}
/*****************************************************************************/
static void cycle_gen_dealloc(struct cycle_gen *self)
{
	Py_XDECREF(self->itr);

	lib_rflow_destroy(self->rflow_state);
	lib_rflow_free_list(&self->stored_cycles);

	self->ob_type->tp_free((PyObject*)self);
}
/*****************************************************************************/
static PyObject *cycle_gen_alloc(PyTypeObject *type, Py_ssize_t nitems)
{
	struct lib_rflow_init init = {
		.opts = LIB_RFLOW_MODE_PASSTHROUGH
	};
	PyObject *obj	       = PyType_GenericAlloc(type, 0);
	struct cycle_gen *self = (struct cycle_gen *)obj;

	if(!self) {
		return NULL;
	}

	self->rflow_state = lib_rflow_init(&init);
	return (PyObject *)self;
}
/*****************************************************************************/
PyMODINIT_FUNC initrflow(void)
{
	PyObject* cycle_gen_type_obj = NULL;
	PyObject *module_obj = Py_InitModule3(
		"rflow", rflow_methods,
		"module for running rainflow calculations"
	);

	if(!module_obj) {
		goto fail;
	}
        if(PyType_Ready(&cycle_gen_type) < 0) {
                goto fail;
        }

	cycle_gen_type_obj = (PyObject *)&cycle_gen_type;


	if(PyModule_AddObject(module_obj, "cycle_gen", cycle_gen_type_obj)) {
		goto fail;
	}

	Py_INCREF(&cycle_gen_type);
	return;
fail:
	Py_DECREF(&cycle_gen_type);
	return;
}
/*****************************************************************************/