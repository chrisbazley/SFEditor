/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Memory management for a sprite area
 *  Copyright (C) 2019 Christopher Bazley
 */

#ifndef SprMem_h
#define SprMem_h

#include <stdbool.h>
#include "SprFormats.h"
#include "Vertex.h"

typedef struct SprMem {
  void *mem;
} SprMem;

bool SprMem_init(SprMem *sm, int size);

bool SprMem_create_sprite(SprMem *sm, char const *name,
  bool has_palette, Vertex size, int mode);

bool SprMem_create_mask(SprMem *sm, char const *name);

void SprMem_rename(SprMem *sm, char const *old_name,
  char const *new_name);

SpriteHeader *SprMem_get_sprite_address(SprMem *sm,
  char const *name);

void SprMem_put_sprite_address(SprMem *sm,
  SpriteHeader *sprite);

SpriteAreaHeader *SprMem_get_area_address(SprMem *sm);

void SprMem_put_area_address(SprMem *sm);

bool SprMem_output_to_sprite(SprMem *sm, char const *name);

bool SprMem_output_to_mask(SprMem *sm, char const *name);

void SprMem_restore_output(SprMem *sm);

void SprMem_delete(SprMem *sm, char const *name);

void SprMem_plot_sprite(const SprMem *sm,
      char const *name,
      Vertex coords, int action);

void SprMem_flip(const SprMem *sm, char const *name);

void SprMem_plot_scaled_sprite(const SprMem *sm,
      char const *name,
      Vertex coords, int action, ScaleFactors *scale,
      void const *colours);

void SprMem_plot_trans_quad_sprite(const SprMem *sm,
  const char *name, BBox const *src, int action,
  TransformQuad const *quad,
  void const *colours);

void SprMem_plot_trans_matrix_sprite(const SprMem *sm,
  const char *name, BBox const *src, int action,
  TransformMatrix const *matrix,
  void const *colours);

size_t SprMem_get_sprite_count(const SprMem *sm);

void SprMem_minimize(SprMem *sm);

bool SprMem_verify(const SprMem *sm);

bool SprMem_save(const SprMem *sm, char const *filename);

void SprMem_destroy(SprMem *sm);

#endif
