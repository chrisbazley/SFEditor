/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Mission filenames
 *  Copyright (C) 2020 Christopher Bazley
 */

#ifndef Filenames_h
#define Filenames_h

#include "SFError.h"
#include "DataType.h"

struct Reader;
struct Writer;

typedef struct FilenamesData FilenamesData;

SFError filenames_read(FilenamesData *filenames,
  struct Reader *reader);

void filenames_write(FilenamesData const *filenames,
  struct Writer *writer);

char const *filenames_get(FilenamesData const *filenames, DataType data_type);
void filenames_set(FilenamesData *filenames, DataType data_type, char const *name);

#endif
