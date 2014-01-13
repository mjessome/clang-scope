#pragma once
#include <assert.h>

#define LOG(level, msg) if (LogLevel >= level) std::cout << msg;

enum ReferenceType {
  ReferenceType_Any = 0,
  Declaration,
  Definition,
  Override,
  Use,
  ReferenceType_Max
};

enum IdentifierType {
  IdentifierType_Any = 0,
  Enum,
  Function,
  Namespace,
  Type,
  Typedef,
  Variable,
  IdentifierType_Max
};
