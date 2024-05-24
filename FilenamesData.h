/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Mission filenames data
 *  Copyright (C) 2020 Christopher Bazley
 */

#ifndef FilenamesData_h
#define FilenamesData_h

#include "DataType.h"
#include "Filenames.h"

enum {
  BytesPerFilename = 12,
};

typedef char Filename[BytesPerFilename];

struct FilenamesData {
  Filename names[DataType_Count];
};

#endif
