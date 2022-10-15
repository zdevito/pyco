#include "minpybind.h"
#include "aco.h"

#define PYCO_FIX_PYBIND11
#ifdef PYCO_FIX_PYBIND11
// pybind keeps its own mapping of the current thread state
// so if it is being used (like with torch), we need to update it to the right state
// TODO: find this path in the setup script
#include "/raid/zdevito/pytorch/third_party/pybind11/include/pybind11/pybind11.h"
inline void state_switch(PyThreadState* from, PyThreadState* to) {
    PYBIND11_TLS_REPLACE_VALUE(pybind11::detail::get_internals().tstate, to);
}
#else
inline void state_switch(PyThreadState* from, PyThreadState* to) {}
#endif


void co_run();

struct Coroutine : public py::base<Coroutine> {
    Coroutine() {}
    static int init(py::hdl<Coroutine> self,
                      py::tuple_view args,
                      PyObject* kwargs) {
        PY_BEGIN
        if (args.size() != 1) {
            py::raise_error(PyExc_ValueError, "expected single argument");
        }
        self->fn = py::object::borrow(args[0]);
        self->co_state = PyThreadState_New(_PyInterpreterState_Get());
        self->sstk = aco_share_stack_new(0);
        self->this_co = aco_create(self->sstk, 0, co_run, self.ptr());
        return 0;
        PY_END(-1)
    }
    static PyObject* resume_with(py::hdl<Coroutine> self, py::handle arg) {
        PY_BEGIN
        //std::cout << "RESUME WITH " << arg << "\n";
        self->arg = py::object::borrow(arg);
        Py_RETURN_NONE;
        PY_END(nullptr)
    }
    static PyObject* result(py::hdl<Coroutine> self) {
        py::object r;
        do {
            r = py::object::steal(iternext(self));
        } while (r.ptr());
        return py::object::borrow(self->result_).release();
    }
    static PyObject* iter(py::hdl<Coroutine> self) {
        PY_BEGIN
        return py::object::borrow((PyObject*) self.ptr()).release();
        PY_END(nullptr)
    }
    static PyObject* iternext(py::hdl<Coroutine> self) {
        PY_BEGIN
        if (self->this_co->is_end) {
            return nullptr; // StopIteration
        }
        auto main_state = PyThreadState_Swap(self->co_state);
        state_switch(main_state, self->co_state);
        aco_resume(self->this_co);
        PyObject* ptype;
        PyObject* pvalue;
        PyObject* ptraceback;
        PyErr_Fetch(&ptype, &pvalue, &ptraceback); // propagate to error state to other python state.
        auto r = PyThreadState_Swap(main_state);
        state_switch(self->co_state, main_state);

        PYBIND11_TLS_REPLACE_VALUE(pybind11::detail::get_internals().tstate, main_state);

        assert(r == co_state);
        PyErr_Restore(ptype, pvalue, ptraceback);
        if (self->this_co->is_end) {
            self->result_ = std::move(self->arg);
            return nullptr; // StopIteration
        } else {
            return self->arg.release();
        }
        PY_END(nullptr)
    }
    ~Coroutine() {
        if (co_state) {
            PyThreadState_Clear(co_state);
            PyThreadState_Delete(co_state);
        }
        if (this_co) {
            aco_destroy(this_co);
            aco_share_stack_destroy(sstk);
        }
    }
    static PyTypeObject Type;
    py::object fn;
    py::object arg;
    py::object result_;
    PyThreadState* co_state;
    aco_t* this_co;
    aco_share_stack_t* sstk;
};

void co_run() {
    auto co = py::hdl<Coroutine>((Coroutine*) aco_get_arg());
    co->arg = py::object::borrow(PyObject_CallObject(co->fn.ptr(), nullptr));
    aco_exit();
}

static PyMethodDef Coroutine_methods[] = {
    {"resume_with", (PyCFunction) Coroutine::resume_with, METH_O},
    {"result", (PyCFunction) Coroutine::result, METH_NOARGS},
    {NULL, NULL, 0, NULL}        /* Sentinel */
};

PyTypeObject Coroutine::Type = {
    PyVarObject_HEAD_INIT(NULL, 0)
    "Coroutine",                 /* tp_name */
    sizeof(Coroutine),               /* tp_basicsize */
    0,                              /* tp_itemsize */
    Coroutine::dealloc_stub,      /* tp_dealloc */
    0,                              /* tp_vectorcall_offset */
    0,                              /* tp_getattr */
    0,                              /* tp_setattr */
    0,                              /* tp_as_async */
    0,                     /* tp_repr */
    0,                 /* tp_as_number */
    0,                              /* tp_as_sequence */
    0,                              /* tp_as_mapping */
    0,      /* tp_hash */
    0,                              /* tp_call */
    0,                              /* tp_str */
    0,                              /* tp_getattro */
    0,                              /* tp_setattro */
    0,                              /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,  /* tp_flags */
    "Coroutine",                   /* tp_doc */
    0,                              /* tp_traverse */
    0,                              /* tp_clear */
    0,  /* tp_richcompare */
    0,                              /* tp_weaklistoffset */
    (getiterfunc) Coroutine::iter,                /* tp_iter */
    (iternextfunc) Coroutine::iternext,            /* tp_iternext */
    Coroutine_methods,              /* tp_methods */
    0,                              /* tp_members */
    0,                 /* tp_getset */
    0,                              /* tp_base */
    0,                              /* tp_dict */
    0,                              /* tp_descr_get */
    0,                              /* tp_descr_set */
    0,                              /* tp_dictoffset */
    (initproc)(void*) Coroutine::init,  /* tp_init */
    0,                              /* tp_alloc */
    Coroutine::new_stub,                      /* tp_new */
};

static PyObject* co_yield_(PyObject * self_,
                      PyObject *const *args,
                      Py_ssize_t nargs,
                      PyObject *kwnames) {
    PY_BEGIN
    aco_t* current_co = aco_get_co();
    if (aco_is_main_co(current_co)) {
        py::raise_error(PyExc_ValueError, "co_yield called from outside a coroutine");
    }
    auto co = py::hdl<Coroutine>((Coroutine*) current_co->arg);
    if (nargs > 1 || kwnames) {
        py::raise_error(PyExc_ValueError, "expected at most one value to yield");
    }
    co->arg = py::object::borrow(nargs == 1 ? args[0] : Py_None);
    //std::cout << "CO_YIELD " << co->arg << "\n";
    aco_yield();
    auto r = std::move(co->arg);
    if (r.ptr()) {
        //std::cout << "CO_YIELD_END " << r << "\n";
        return r.release();
    } else {
        //std::cout << "CO_YIELD_END None" << "\n";
        Py_RETURN_NONE;
    }
    PY_END(nullptr)
}

static PyMethodDef methods[] = {
    {"co_yield", (PyCFunction) co_yield_, METH_FASTCALL | METH_KEYWORDS},
    {NULL, NULL, 0, NULL}        /* Sentinel */
};
static struct PyModuleDef module_def = {
    PyModuleDef_HEAD_INIT,
    "_pyco",   /* name of module */
    NULL, /* module documentation, may be NULL */
    -1,       /* size of per-interpreter state of the module,
                 or -1 if the module keeps state in global variables. */
    methods
};

extern "C"
PyObject* PyInit__pyco(void) {
    PY_BEGIN
    pybind11::detail::get_internals(); // need to pre-cache the struct??
    py::object mod = py::object::checked_steal(PyModule_Create(&module_def));
    Coroutine::ready(mod, "Coroutine");
    return mod.release();
    PY_END(nullptr)
}
