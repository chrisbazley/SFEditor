/*
 * SFeditor - Star Fighter 3000 map/mission editor
 * © Chris Bazley, 2001
 *
 * Proglet to fix animations data
 */

#include "stdlib.h"
#include "stdio.h"
#include <string.h>
#include <stdbool.h>

#include "kernel.h"
#include "swis.h"
#include "flex.h"

#include "hourglass.h"
#include "strcaseins.h"
#include "Macros.h"
#include "FileUtils.h"

#include "filepaths.h"

enum {
  CopyObjects = 26
};

void check_error(_kernel_oserror *e)
{
  if (e == NULL)
    return;
  fprintf(stderr, "  Fatal error: %s\n",e->errmess);
  exit(EXIT_FAILURE);
}

void load_compressed(char *filepath, flex_ptr buffer)
{
  /* Allocate buffer and load compressed Fednet datafile */
  FILE *readfile;
  int buffer_size;

  /* get (decompressed) memory requirements */
  readfile = fopen(filepath, "r");
  if (readfile == NULL) {
    fprintf(stderr, "Could not open file %s\n", filepath);
    exit(EXIT_FAILURE);
  }
  if (fread(&buffer_size, sizeof(int), 1, readfile) != 1) {
    fclose(readfile);
    fprintf(stderr, "Error reading from file %s\n", filepath);
    exit(EXIT_FAILURE);
  }
  fclose(readfile);

  /* Allocate buffer for data */
  if (!flex_alloc(buffer, buffer_size)) {
    fprintf(stderr, "Cannot claim memory\n");
    exit(EXIT_FAILURE);
  }

  /* Construct CLI command */
  char command[16+strlen(filepath)+1];
  sprintf(command, "Cload %s &%X", filepath, (int)*buffer);

  /* Decompress file */
  hourglass_on();
  if (_kernel_oscli(command) == _kernel_ERROR) {
    hourglass_off();
    flex_free(buffer);
    check_error(_kernel_last_oserror());
  }
  hourglass_off();
}

_kernel_oserror *save_compressed(char *filepath, int filetype, flex_ptr buffer)
{
  /* Save the specified memory as a compressed Fednet file */

  /* Construct CLI command */
  char command[25 + strlen(filepath) + 1];
  sprintf(command, "CSave %s &%X &%X", filepath, (int)*buffer, ((int)*buffer + flex_size(buffer)));

  /* Compress file */
  hourglass_on();
  {
    int err = _kernel_oscli(command);
    hourglass_off();
    if (err == _kernel_ERROR)
      check_error(_kernel_last_oserror());
  }

  /* Set file type */
  check_error(set_file_type(filepath, filetype));

  return NULL; /* success */
}

bool file_exists(const char *filepath)
{
  OS_File_CatalogueInfo catalogue_info;
  if (os_file_read_cat_no_path(filepath, &catalogue_info) != NULL)
    return false; /* if error then assume object doesn't exist */

  return (catalogue_info.object_type != ObjectType_NotFound);
}

static char dest_buffer[256], filepath_buffer[256];

bool check_prostitute(SFMission *mission_data, const char *component_dir, const char *level_id, const char *descr, char *filename)
{
  if (stricmp(filename, level_id) != 0 &&
  stricmp(filename, "Blank") != 0) {
    snprintf(dest_buffer, sizeof(dest_buffer), "%s.%s.%s", FIXED_GAME_DIR, component_dir, level_id);
    snprintf(filepath_buffer, sizeof(filepath_buffer), "%s.%s.%s", FIXED_GAME_DIR, component_dir, filename);
    printf("Copying [%s] to [%s]\n", filepath_buffer, dest_buffer);
    check_error(_swix(OS_FSControl, _INR(0,3), CopyObjects, filepath_buffer, dest_buffer, 1));

    printf("Changing %s file [%s] to [%s]\n", descr, filename, level_id);
    strcpy(filename, level_id);
    return true;
  } else
    return false;
}

int main(int argc, char *argv[])
{
  _kernel_osfile_block kosfb;
  SFMission *mission_data; /* pointer to decompressed file */
  char pyr_prefix[5]="";
  int ch, l, p;
  bool save;

  flex_init("AnimsFix", 0, 0); /* (use Wimpslot and own messages) */
  flex_set_budge(1); /* (cause flex store to be moved up if the C library needs to extend the heap) */

  printf("Found SF3000 at [%s]\n", getenv("Star3000$Dir"));
  printf("Found FednetRes at [%s]\n", getenv("FednetRes$Path"));

  printf("Loading compression modules\n");
  check_error(_swix(OS_Module, _INR(0,1), 1, "FednetRes:DeComp"));
  check_error(_swix(OS_Module, _INR(0,1), 1, "FednetRes:Comp"));

/*  check_error(load_compressed("<Star3000$Dir>.Landscapes.Missions.E.E_01", (void **)&mission_data, &data_size));
  descramble_mission_filenames(mission_data);
  scramble_mission_filenames(mission_data);
  check_error(save_compressed("<Star3000$Dir>.Landscapes.Missions.E.E_01_out", FILETYPE_MISSION, mission_data, sizeof(SFMission)));
  exit(EXIT_SUCCESS);
*/
  printf("About to scan game files, amending animations data\n\n");
  printf("Press ENTER to continue, or ESCAPE to quit\n");
  ch = getchar();

  snprintf(filepath_buffer, sizeof(filepath_buffer), "%s.%s.E.E_01", FIXED_GAME_DIR, LEVELANIMS_DIR);
  printf("Deleting [%s]\n", filepath_buffer);
  if (remove(filepath_buffer)) {
    fprintf(stderr, "  Warning: Could not remove %s\n", filepath_buffer);
  }

  snprintf(filepath_buffer, sizeof(filepath_buffer), "%s.%s.E.E_23", FIXED_GAME_DIR, LEVELANIMS_DIR);
  printf("Deleting [%s]\n", filepath_buffer);
  if (remove(filepath_buffer)) {
    fprintf(stderr, "  Warning: Could not remove %s\n", filepath_buffer);
  }

  kosfb.start = 0; /* default number of entries */
  snprintf(filepath_buffer, sizeof(filepath_buffer), "%s.%s", FIXED_GAME_DIR, BASEANIMS_DIR);
  check_error(os_file_create_dir(filepath_buffer,
                                 OS_File_CreateDir_DefaultNoOfEntries));

  snprintf(filepath_buffer, sizeof(filepath_buffer), "%s.%s.E.E_07", FIXED_GAME_DIR, LEVELANIMS_DIR);
  snprintf(dest_buffer, sizeof(dest_buffer), "%s.%s.Academy1", FIXED_GAME_DIR, BASEANIMS_DIR);
  printf("Copying [%s] to [%s]\n", filepath_buffer, dest_buffer);
  check_error(_swix(OS_FSControl, _INR(0,3), CopyObjects, filepath_buffer, dest_buffer, 1));

  printf("\nScanning mission files...\n");
  for (p=0;p<3;p++) {
    switch (p) {
      case 0:
        strcpy(pyr_prefix, "E.E_");
        break;
      case 1:
        strcpy(pyr_prefix, "M.M_");
        break;
      case 2:
        strcpy(pyr_prefix, "H.H_");
        break
        ;
    }
    for (l=1;l<=36;l++) {
      char level_id[7];
      snprintf(level_id, sizeof(level_id), "%s%02d", pyr_prefix, l);

      snprintf(filepath_buffer, sizeof(filepath_buffer), "%s.%s.%s", FIXED_GAME_DIR, MISSION_DIR, level_id);
      printf("Loading mission data from  [%s]\n", filepath_buffer);
      load_compressed(filepath_buffer, (flex_ptr)&mission_data);
      descramble_mission_filenames(&mission_data);
      save = false;

      /* Check for mixing components from different base maps  */
      if (stricmp(mission_data->map_tiles_basemap, mission_data->ground_objects_basemap) != 0 &&
         stricmp(mission_data->map_tiles_basemap, "Blank") != 0 &&
         stricmp(mission_data->ground_objects_basemap, "Blank") != 0) {
        fprintf(stderr,"  Informational: mixes base map components (objects from %s and tiles from %s)\n", mission_data->ground_objects_basemap, mission_data->map_tiles_basemap);
      }

      /* Check for refering to deleted E.E_01 animations file */
      if (stricmp(mission_data->animations, "E.E_01") == 0) {
        printf("Changing animations filename [%s.E.E_01] to [%s.Blank]\n", LEVELANIMS_DIR, LEVELANIMS_DIR);
        sprintf(mission_data->animations, "Blank");
        save = true;
      }

      /* Check for prostitution of mission files */
      save = save || check_prostitute(mission_data, LEVELMAP_DIR, level_id, "tiles map overlay", mission_data->map_tiles_levmap);
      save = save || check_prostitute(mission_data, LEVELGRID_DIR, level_id, "objects grid overlay", mission_data->ground_objects_levmap);
      save = save || check_prostitute(mission_data, LEVELANIMS_DIR, level_id, "animations", mission_data->animations);

      /* Save mission file */
      if (save) {
        printf("Saving mission file\n\n");
        scramble_mission_filenames(&mission_data);
        snprintf(filepath_buffer, sizeof(filepath_buffer), "%s.%s.%s", FIXED_GAME_DIR, MISSION_DIR, level_id);
        save_compressed(filepath_buffer, FILETYPE_MISSION, (flex_ptr)&mission_data);
      }
      else
        printf("Ignoring mission\n\n");
      flex_free((flex_ptr)&mission_data);
    }
  }
  printf("\nFinished!\n");
}


