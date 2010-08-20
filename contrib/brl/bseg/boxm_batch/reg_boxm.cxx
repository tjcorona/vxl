#include "reg_boxm.h"
#include <bprb/bprb_macros.h>
#include <bprb/bprb_batch_process_manager.h>

// processes
#include <vil_pro/vil_register.h>
#include <vpgl_pro/vpgl_register.h>
#include <sdet_pro/sdet_register.h>
#include <boxm/pro/boxm_register.h>
#include <boxm/algo/pro/boxm_algo_register.h>
#include <boxm/algo/rt/pro/boxm_algo_rt_register.h>
#include <boxm/algo/sp/pro/boxm_algo_sp_register.h>
#if defined(HAS_OPENCL) && HAS_OPENCL
#include <boxm/ocl/pro/boxm_ocl_register.h>
#endif
#include <bvpl/bvpl_octree/pro/bvpl_octree_register.h>
#include <bsta/pro/bsta_register.h>

PyObject *
register_processes(PyObject *self, PyObject *args)
{
  vil_register::register_process();
  vpgl_register::register_process();
  boxm_register::register_process();
  boxm_algo_register::register_process();

  boxm_algo_rt_register::register_process();
  boxm_algo_sp_register::register_process();
  sdet_register::register_process();
#if defined(HAS_OPENCL) && HAS_OPENCL
  boxm_ocl_register::register_process();
#endif
  bvpl_octree_register::register_process();
  Py_INCREF(Py_None);
  return Py_None;
}

PyObject *
register_datatypes(PyObject *self, PyObject *args)
{
  register_basic_datatypes();
  vil_register::register_datatype();
  vpgl_register::register_datatype();
  boxm_register::register_datatype();
  boxm_algo_register::register_process();

  boxm_algo_rt_register::register_datatype();
  boxm_algo_sp_register::register_datatype();
#if defined(HAS_OPENCL) && HAS_OPENCL
  boxm_ocl_register::register_datatype();
#endif
  bvpl_octree_register::register_datatype();
  bsta_register::register_datatype();
  Py_INCREF(Py_None);
  return Py_None;
}

PyMODINIT_FUNC
initboxm_batch(void)
{
  PyMethodDef reg_pro;
  reg_pro.ml_name = "register_processes";
  reg_pro.ml_meth = register_processes;
  reg_pro.ml_doc = "register_processes() create instances of each defined process";
  reg_pro.ml_flags = METH_VARARGS;


  PyMethodDef reg_data;
  reg_data.ml_name = "register_datatypes";
  reg_data.ml_meth = register_datatypes;
  reg_data.ml_doc = "register_datatypes() insert tables in the database for each type";
  reg_data.ml_flags = METH_VARARGS;

  boxm_batch_methods[0]=reg_pro;
  boxm_batch_methods[1]=reg_data;

  for (int i=0; i<METHOD_NUM; ++i) {
    boxm_batch_methods[i+2]=batch_methods[i];
  }

  Py_InitModule("boxm_batch", boxm_batch_methods);
}
