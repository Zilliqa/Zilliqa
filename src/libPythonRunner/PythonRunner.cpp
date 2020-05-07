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
                             const vector<string>& params,
                             const string& outputFileName) {
  LOG_MARKER();
  try {
    setenv("PYTHONPATH", ".", 1);
    Py_Initialize();

    const auto currPath = boost::filesystem::current_path();
    const auto fileName = file + ".py";

    const int argc = params.size() + 1;
    // const auto& fullFileName = file + ".py";
    wchar_t** _argv = (wchar_t**)PyMem_Malloc(sizeof(wchar_t*) * argc);
    for (int i = 0; i < argc; i++) {
      wchar_t* arg;
      if (i == 0) {
        // case for basename of program
        arg = Py_DecodeLocale(fileName.c_str(), NULL);
      } else {
        arg = Py_DecodeLocale(params[i - 1].c_str(), NULL);
      }
      _argv[i] = arg;
    }

    std::string stdOutErr =
        "import sys\n\
class CatchOutput:\n\
  def __init__(self):\n\
    self.value = ''\n\
    self.file = open(\'" +
        outputFileName +
        "\',\'w\')\n\
  def write(self,msg):\n\
    self.value += msg\n\
    self.file.write(msg)\n\
  def close(self):\n\
    self.file.close()\n\
    self.file = None\n\
catchOutput = CatchOutput()\n\
sys.stdout = catchOutput\n\
sys.stderr = catchOutput\n\
  ";

    string closeOutput =
        "catchOutput.close()\n\
";

    LOG_GENERAL(INFO, "stdOut: " << stdOutErr);

    PySys_SetArgvEx(argc, _argv, 0);

    LOG_GENERAL(INFO, "Inside py runner " << file);

    object main = import("__main__");
    object global(main.attr("__dict__"));

    PyRun_SimpleString(stdOutErr.c_str());

    object module = import(file.c_str());

    object exec = module.attr(func.c_str());

    object ret = exec();

    PyObject* catcher = PyObject_GetAttrString(main.ptr(), "catchOutput");
    PyObject* output = PyObject_GetAttrString(catcher, "value");
    PyRun_SimpleString(closeOutput.c_str());  // For cleaning the buffer out

    const string out = extract<string>(output);

    LOG_GENERAL(INFO, "Py Output: \n" << out);

    // change back to original incase Py script modified it

    boost::filesystem::current_path(currPath);

    return extract<bool>(ret);

  }

  catch (const error_already_set&) {
    LOG_GENERAL(INFO, "Py Exception");
    // PyErr_Print();
    if (PyErr_Occurred()) {
      LOG_GENERAL(INFO, "Inside");
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