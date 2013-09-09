#pragma once
#include <assert.h>

#define LOG(level, msg) if (LogLevel >= level) std::cout << msg;

enum ReferenceType {
  Declaration = 0,
  Definition,
  Override,
  Use,
  ReferenceType_Max
};

enum IdentifierType {
  Enum = 0,
  Function,
  Namespace,
  Type,
  Typedef,
  Variable,
  IdentifierType_Max
};
