/*
 * Copyright (C) 2019 Zilliqa
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "PythonRunner.h"
#include "libUtils/Logger.h"

using namespace boost::python;
using namespace std;

std::string handle_pyerror() {
  using namespace boost;

  PyObject *exc, *val, *tb;
  object formatted_list, formatted;
  PyErr_Fetch(&exc, &val, &tb);
  handle<> hexc(exc), hval(allow_null(val)), htb(allow_null(tb));
  object traceback(import("traceback"));
  if (!tb) {
    object format_exception_only(traceback.attr("format_exception_only"));
    formatted_list = format_exception_only(hexc, hval);
  } else {
    object format_exception(traceback.attr("format_exception"));
    formatted_list = format_exception(hexc, hval, htb);
  }
  formatted = boost::python::str("\n").join(formatted_list);
  return extract<std::string>(formatted);
}

// File is the py file in which the module is
// Func is the function to be run, should return a boolean
// params are the command line argumnents to be passed to the py code

bool PythonRunner::RunPyFunc(const string& file, const string& func,
                             const vector<string>& params) {
  try {
    setenv("PYTHONPATH", ".", 1);

    const int argc = params.size() + 1;
    wchar_t** _argv = (wchar_t**)PyMem_Malloc(sizeof(wchar_t*) * argc);
    for (int i = 0; i < argc; i++) {
      wchar_t* arg;
      if (i == 0) {
        // case for basename of program
        arg = Py_DecodeLocale(file.c_str(), NULL);
      } else {
        arg = Py_DecodeLocale(params[i - 1].c_str(), NULL);
      }
      _argv[i] = arg;
    }

    Py_Initialize();
    PySys_SetArgv(argc, _argv);

    object main = import("__main__");
    object global(main.attr("__dict__"));

    object module = import(file.c_str());

    object exec = module.attr(func.c_str());

    object ret = exec();

    return extract<bool>(ret);

  }

  catch (const error_already_set&) {
    PyErr_Print();
    if (PyErr_Occurred()) {
      auto msg = handle_pyerror();
      LOG_GENERAL(WARNING, msg);
    }
    handle_exception();
    PyErr_Clear();

    return false;
  }

  return true;
}

boost::python::list PythonRunner::VectorToPyList(const vector<string>& str) {
  boost::python::list L;

  for (const auto& s : str) {
    L.append(s);
  }
  return L;
}