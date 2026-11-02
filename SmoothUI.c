/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Smoothing wand implementation
 *  Copyright (C) 2001 Christopher Bazley
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

#include "stdio.h"
#include "stdlib.h"

#include "FilePaths.h"
#include "Hourglass.h"
#include "Utils.h"
#include "Debug.h"
#include "SFError.h"
#include "Smooth.h"

void MapTexGroups_edit(char const *const tiles_set)
{
  edit_file(TILEGROUPS_DIR, tiles_set);
}

void MapTexGroups_load(MapTexGroups *const groups_data, char const *tiles_set,
                       int const ntiles)
{
  _Optional char *const full_path = make_file_path_in_dir(
                          CHOICES_READ_PATH TILEGROUPS_DIR, tiles_set);
  if (!full_path)
  {
    return;
  }

  MapTexGroups_free(groups_data);
  MapTexGroups_init(groups_data);

  char err_buf[MapTexGroupsErrBufferSize] = "";
  SFError err = SFERROR(OK);

  hourglass_on();
  if (file_exists(&*full_path))
  {
    DEBUG("Opening tile groups file '%s'", &*full_path);
    _Optional FILE *const file = fopen(&*full_path, "r");
    if (file == NULL)
    {
      err = SFERROR(OpenInFail);
    }
    else
    {
      err = MapTexGroups_read(&*file, groups_data, ntiles, &err_buf);
      fclose(&*file);
    }
  }
  hourglass_off();

  report_error(err, &*full_path, err_buf);
  free(full_path);
}
