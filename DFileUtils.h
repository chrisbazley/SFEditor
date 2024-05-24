/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Compressed file utilities
 *  Copyright (C) 2021 Christopher Bazley
 */


#ifndef dfileutils_h
#define dfileutils_h

#include <stdbool.h>
#include "DFile.h"

struct Writer;
struct Reader;

char *get_leaf_name(DFile *dfile);

SFError save_compressed(DFile *dfile, char *fname);
SFError load_compressed(DFile *dfile, char const *fname);

int get_compressed_size(DFile *dfile);

SFError write_compressed(DFile *dfile, struct Writer *writer);
SFError read_compressed(DFile *dfile, struct Reader *reader);

int worst_compressed_size(DFile *dfile);

#endif
