/*************************************************************************
 * libjson-rpc-cpp
 *************************************************************************
 * @file    procedure.cpp
 * @date    31.12.2012
 * @author  Peter Spiess-Knafl <dev@spiessknafl.at>
 * @license See attached LICENSE.txt
 ************************************************************************/

#include "procedure.h"
#include "errors.h"
#include "exception.h"
#include <cstdarg>
#include <vector>

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wvarargs"
#endif
using namespace std;
using namespace jsonrpc;

Procedure::Procedure()
    : procedureName(""), procedureType(RPC_METHOD), returntype(JSON_BOOLEAN),
      paramDeclaration(PARAMS_BY_NAME) {}

Procedure::Procedure(const string &name, parameterDeclaration_t paramType,
                     jsontype_t returntype, ...) {
  va_list parameters;
  va_start(parameters, returntype);
  const char *paramname = va_arg(parameters, const char *);
  jsontype_t type;
  jsonflags_t flags;
  int arg;
  while (paramname != NULL) {
    arg = va_arg(parameters, int);
    flags = (jsonflags_t)(arg & 0xffff0000);
    type = (jsontype_t)(arg & 0xffff);
    this->AddParameter(paramname, type, flags);
    paramname = va_arg(parameters, const char *);
  }
  va_end(parameters);
  this->procedureName = name;
  this->returntype = returntype;
  this->procedureType = RPC_METHOD;
  this->paramDeclaration = paramType;
}
Procedure::Procedure(const string &name, parameterDeclaration_t paramType,
                     ...) {
  va_list parameters;
  va_start(parameters, paramType);
  const char *paramname = va_arg(parameters, const char *);
  jsontype_t type;
  jsonflags_t flags;
  int arg;
  while (paramname != NULL) {
    arg = va_arg(parameters, int);
    flags = (jsonflags_t)(arg & 0xffff0000);
    type = (jsontype_t)(arg & 0xffff);
    this->AddParameter(paramname, type, flags);
    paramname = va_arg(parameters, const char *);
  }
  va_end(parameters);
  this->procedureName = name;
  this->procedureType = RPC_NOTIFICATION;
  this->paramDeclaration = paramType;
  this->returntype = JSON_BOOLEAN;
}

bool Procedure::ValdiateParameters(const Json::Value &parameters) const {
  if (this->parametersName.empty()) {
    return true;
  }
  if (parameters.isArray() && this->paramDeclaration == PARAMS_BY_POSITION) {
    return this->ValidatePositionalParameters(parameters);
  } else if (parameters.isObject() &&
             this->paramDeclaration == PARAMS_BY_NAME) {
    return this->ValidateNamedParameters(parameters);
  } else {
    return false;
  }
}
const parameterNameList_t &Procedure::GetParameters() const {
  return this->parametersName;
}
procedure_t Procedure::GetProcedureType() const { return this->procedureType; }
const std::string &Procedure::GetProcedureName() const {
  return this->procedureName;
}
parameterDeclaration_t Procedure::GetParameterDeclarationType() const {
  return this->paramDeclaration;
}
jsontype_t Procedure::GetReturnType() const { return this->returntype; }

void Procedure::SetProcedureName(const string &name) {
  this->procedureName = name;
}
void Procedure::SetProcedureType(procedure_t type) {
  this->procedureType = type;
}
void Procedure::SetReturnType(jsontype_t type) { this->returntype = type; }
void Procedure::SetParameterDeclarationType(parameterDeclaration_t type) {
  this->paramDeclaration = type;
}

void Procedure::AddParameter(const string &name, jsontype_t type, jsonflags_t flags) {
  this->parametersName[name] = type;
  this->parametersPosition.push_back(std::make_pair(type, flags));
}
bool Procedure::ValidateNamedParameters(const Json::Value &parameters) const {
  bool ok = parameters.isObject() || parameters.isNull();
  for (map<string, jsontype_t>::const_iterator it =
           this->parametersName.begin();
       ok == true && it != this->parametersName.end(); ++it) {
    if (!parameters.isMember(it->first)) {
      ok = false;
    } else {
      ok = this->ValidateSingleParameter(it->second, parameters[it->first]);
    }
  }
  return ok;
}
bool Procedure::ValidatePositionalParameters(
    const Json::Value &parameters) const {
  bool ok = true;

  // If there are more parameters than we've specified, it's definitely an error
  // (for now)
  if (parameters.size() > this->parametersPosition.size()) {
    return false;
  }

  // Otherwise, go through the parameters we've specified; if we hit the end of the
  // parameters we're given, the rest of the specified parameters must be optional.
  for (unsigned int i = 0; ok && i < this->parametersPosition.size(); i++) {
    if (i < parameters.size()) {
        ok = this->ValidateSingleParameter(this->parametersPosition.at(i).first,
                                           parameters[i]);
      } else {
      // No remaining parameters.
      if (!(this->parametersPosition.at(i).second & jsonflags_t::JSON_FLAG_OPTIONAL)) {
        ok = false;
      }
    }
  }
  return ok;
}
bool Procedure::ValidateSingleParameter(jsontype_t expectedType,
                                        const Json::Value &value) const {
  bool ok = true;
  switch (expectedType) {
  case JSON_STRING:
    if (!value.isString())
      ok = false;
    break;
  case JSON_BOOLEAN:
    if (!value.isBool())
      ok = false;
    break;
  case JSON_INTEGER:
    if (!value.isIntegral())
      ok = false;
    break;
  case JSON_REAL:
    if (!value.isDouble())
      ok = false;
    break;
  case JSON_NUMERIC:
    if (!value.isNumeric())
      ok = false;
    break;
  case JSON_OBJECT:
    if (!value.isObject())
      ok = false;
    break;
  case JSON_ARRAY:
    if (!value.isArray())
      ok = false;
    break;
  }
  return ok;
}

#ifdef __clang__
#pragma clang diagnostic pop
#endif
