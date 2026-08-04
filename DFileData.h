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

#if !defined(USE_OPTIONAL) && !defined(_Optional)
#define _Optional
#endif

struct Writer;
struct Reader;

typedef SFError DFileReadFn(DFile const *dfile, struct Reader *reader);
typedef void DFileWriteFn(DFile const *dfile, struct Writer *writer);
typedef long int DFileGetMinSizeFn(DFile const *dfile);
typedef void DFileDestroyFn(DFile const *dfile);

void dfile_init(DFile *dfile,
                _Optional DFileReadFn *read,
                _Optional DFileWriteFn *write,
                _Optional DFileGetMinSizeFn *get_min_size,
                _Optional DFileDestroyFn *destroy);

void dfile_destroy(DFile *dfile);

struct DFile
{
  StrDict *dict;
  int ref_count;
  bool is_modified;
  int date[2];
  _Optional char *name;
  _Optional DFileReadFn *read;
  _Optional DFileWriteFn *write;
  _Optional DFileGetMinSizeFn *get_min_size;
  _Optional DFileDestroyFn *destroy;
};

#endif
