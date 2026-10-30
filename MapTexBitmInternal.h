/* SFEditor: diagnostic bitmap dumps shared by loading and drawing. */
#ifndef MapTexBitmInternal_h
#define MapTexBitmInternal_h
#include <stdio.h>
#include "MapTexBDat.h"

#ifndef NDEBUG
static void dump_sprites(const MapTexBitmaps *const tiles, MapAngle const angle, int const level)
{
#define FILENAME_PREFIX "<Wimp$ScrapDir>.tile_sprites"
  SprMem_verify(&tiles->sprites[angle][level].obj);

  if (tiles->count > 0)
  {
    char filename[sizeof(FILENAME_PREFIX) + 16];
    sprintf(filename, FILENAME_PREFIX"%d%d", angle, level);
    SprMem_save(&tiles->sprites[angle][level].obj, filename);
  }
}
#else
#define dump_sprites(tiles, angle, level)
#endif

#endif
