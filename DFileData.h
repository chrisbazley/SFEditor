/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Generic file superclass data
 *  Copyright (C) 2020 Christopher Bazley
 */

#ifndef DFileData_h
#define DFileData_h

#include <stdbool.h>

#include "StrDict.h"
#include "SFError.h"
#include "DFile.h"

struct Writer;
struct Reader;

typedef SFError DFileReadFn(DFile const *dfile, struct Reader *reader);
typedef void DFileWriteFn(DFile const *dfile, struct Writer *writer);
typedef long int DFileGetMinSizeFn(DFile const *dfile);
typedef void DFileDestroyFn(DFile const *dfile);

void dfile_init(DFile *dfile, DFileReadFn *read,
  DFileWriteFn *write, DFileGetMinSizeFn *get_min_size,
  DFileDestroyFn *destroy);

void dfile_destroy(DFile *dfile);

struct DFile
{
  StrDict *dict;
  int ref_count;
  bool is_modified;
  int date[2];
  char *name;
  DFileReadFn *read;
  DFileWriteFn *write;
  DFileGetMinSizeFn *get_min_size;
  DFileDestroyFn *destroy;
};

#endif
