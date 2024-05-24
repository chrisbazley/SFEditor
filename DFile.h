/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Generic file superclass
 *  Copyright (C) 2020 Christopher Bazley
 */

#ifndef DFile_h
#define DFile_h

#include <stdbool.h>
#include "StrDict.h"
#include "SFError.h"

struct Writer;
struct Reader;

typedef struct DFile DFile;

void dfile_write(DFile const *dfile, struct Writer *writer);
SFError dfile_read(DFile const *dfile, struct Reader *reader);
bool dfile_get_modified(DFile const *dfile);
void dfile_set_modified(DFile *dfile);
bool dfile_set_saved(DFile *dfile, char const *name, int const *date);
DFile *dfile_find_shared(StrDict *file_dict,
  char const *filename);
bool dfile_set_shared(DFile *dfile, StrDict *dict);
int const *dfile_get_date(DFile const *dfile);
char *dfile_get_name(DFile const *dfile);
long int dfile_get_min_size(DFile const *dfile);
void dfile_claim(DFile *dfile);
void dfile_release(DFile *dfile);

#endif
