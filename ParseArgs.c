/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Command line parser
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

/* ANSI library files */
#include <stdbool.h>
#include "stdlib.h"
#include <string.h>
#include <assert.h>
#include <ctype.h>

#include "Scheduler.h"
#include "err.h"
#include "msgtrans.h"
#include "strextra.h"
#include "filepaths.h"
#include "FileUtils.h"

#include "Session.h"
#include "ParseArgs.h"
#include "Utils.h"

void parse_arguments(int argc, char *argv[])
{
  /*
   * Look at command-line parameters
   */

  assert(argv != NULL || argc == 0);

  for (int i = 1; i < argc; i++) {
    OS_File_CatalogueInfo catalogue_info;
    EF(os_file_read_cat_no_path(argv[i], &catalogue_info));

    /* Check type of object */
    if (catalogue_info.object_type == ObjectType_NotFound ||
        catalogue_info.object_type == ObjectType_Directory) {
      /* Object not found - generate appropriate error */
      EF(os_file_generate_error(argv[i], catalogue_info.object_type));
    } else {
      /* Check that this file has a type and not load/exec addresses */
      int const file_type = decode_load_exec(catalogue_info.load,
                                             catalogue_info.exec, NULL);
      /* Attempt to load the file, if it is a recognised type */
      char *filename = NULL;
      if (!E(canonicalise(&filename, NULL, NULL, argv[i]))) {
        DataType const data_type = file_type_to_data_type(file_type, filename);
        if (data_type != DataType_Count) {
          Session_open_single_file(filename, data_type);
        } else {
          report_error(SFERROR(BadFileType), filename, "");
        }
        free(filename);
      }
    }
  }
}
