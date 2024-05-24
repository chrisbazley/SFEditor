/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Generic file superclass
 *  Copyright (C) 2020 Christopher Bazley
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public Licence as published by
 *  the Free Software Foundation; either version 2 of the Licence, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public Licence for more details.
 *
 *  You should have received a copy of the GNU General Public Licence
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "stdlib.h"
#include <stdbool.h>
#include <limits.h>

#include "Macros.h"
#include "Debug.h"
#include "Writer.h"
#include "WriterNull.h"
#include "Reader.h"
#include "StrExtra.h"

#include "SFError.h"
#include "DFile.h"
#include "DFileData.h"

SFError dfile_read(DFile const *const dfile, Reader *const reader)
{
  assert(dfile);
  DEBUGF("Reading dfile %p from %s\n", (void *)dfile,
         dfile->name ? dfile->name : "");

#ifdef FORTIFY
  Fortify_CheckAllMemory();
#endif

  SFError const err = dfile->read ?
                      dfile->read(dfile, reader) :
                      SFERROR(OK);
#ifdef FORTIFY
  Fortify_CheckAllMemory();
#endif
  return err;
}

void dfile_write(DFile const *const dfile, Writer *const writer)
{
  assert(dfile);
  DEBUGF("Writing dfile %p to %s\n", (void *)dfile,
         dfile->name ? dfile->name : "");

  if (dfile->write)
  {
    dfile->write(dfile, writer);
  }
}

bool dfile_get_modified(DFile const *const dfile)
{
  assert(dfile);
  return dfile->is_modified;
}

void dfile_set_modified(DFile *const dfile)
{
  assert(dfile);
  assert(!dfile->dict);
  dfile->is_modified = true;
  DEBUGF("Modified dfile %p from %s\n", (void *)dfile,
         dfile->name ? dfile->name : "");
}

bool dfile_set_saved(DFile *const dfile, char const *const name,
  int const *const date)
{
  assert(dfile);
  assert(!dfile->dict); // changing name invalidates dictionary
  DEBUGF("Saved dfile %p from %s as %s\n", (void *)dfile,
         dfile->name ? dfile->name : "", name ? name : "");
  assert(date);
  char *const dup = strdup(name);
  if (dup || !name)
  {
    for (size_t i = 0; i < ARRAY_SIZE(dfile->date); ++i)
    {
      dfile->date[i] = date[i];
    }
    dfile->is_modified = (name == NULL); /* untitled? */
    free(dfile->name);
    dfile->name = dup;
  }
  return dup || !name;
}

bool dfile_set_shared(DFile *const dfile, StrDict *const dict)
{
  assert(dfile);
  assert(dfile->name);
  assert(!dfile->is_modified);
  assert(!dfile->dict);

  // Careful! Key string isn't copied on insertion.
  if (!strdict_insert(dict, dfile->name, dfile, NULL)) {
    return false;
  }
  dfile->dict = dict;
  return true;
}

DFile *dfile_find_shared(StrDict *const file_dict,
  char const *const filename)
{
  DFile *const dfile = strdict_find_value(file_dict, filename, NULL);
  if (dfile) {
    assert(stricmp(filename, dfile->name) == 0);
    dfile_claim(dfile);
  }

  DEBUGF("Got shared data %p for %s\n", (void *)dfile, filename);
  return dfile;
}

int const *dfile_get_date(DFile const *const dfile)
{
  assert(dfile);
  return dfile->date;
}

char *dfile_get_name(DFile const *const dfile)
{
  assert(dfile);
  return dfile->name;
}

long int dfile_get_min_size(DFile const *const dfile)
{
  assert(dfile);

  if (dfile->get_min_size)
  {
    return dfile->get_min_size(dfile);
  }

  Writer null;
  writer_null_init(&null);

  if (dfile->write)
  {
    dfile->write(dfile, &null);
  }

  return writer_destroy(&null);
}

void dfile_init(DFile *const dfile,
  DFileReadFn *const read, DFileWriteFn *const write,
  DFileGetMinSizeFn *const get_min_size,
  DFileDestroyFn *const destroy)
{
  assert(dfile);
  *dfile = (DFile){
    .is_modified = false,
    .name = NULL, /* untitled */
    .read = read,
    .write = write,
    .get_min_size = get_min_size,
    .destroy = destroy,
    .ref_count = 1};
}

void dfile_destroy(DFile *const dfile)
{
  assert(dfile);
  if (dfile->dict)
  {
    DFile *const removed = strdict_remove_value(dfile->dict, dfile->name, NULL);
    assert(removed == dfile);
    NOT_USED(removed);
  }
  free(dfile->name);
}

void dfile_claim(DFile *const dfile)
{
  assert(dfile);
  assert(dfile->ref_count > 0);
  assert(dfile->ref_count < INT_MAX);
  ++dfile->ref_count;
  DEBUGF("Add reference (count %d) to dfile %p from %s\n",
         dfile->ref_count, (void *)dfile,
         dfile->name ? dfile->name : "<untitled>");
}

void dfile_release(DFile *const dfile)
{
  assert(dfile);
  assert(dfile->ref_count > 0);
  dfile->ref_count--;
  DEBUGF("Release reference (count %d) to dfile %p from %s\n",
         dfile->ref_count, (void *)dfile,
         dfile->name ? dfile->name : "<untitled>");

  if (dfile->ref_count == 0)
  {
    if (dfile->destroy)
    {
      dfile->destroy(dfile);
    }
    else
    {
      dfile_destroy(dfile);
    }
  }
#ifdef FORTIFY
  Fortify_CheckAllMemory();
#endif
}
