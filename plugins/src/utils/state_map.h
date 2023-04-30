// https://lv2plug.in/book/#_state_map_h
#ifndef STATE_MAP_H
#define	STATE_MAP_H


#include "lv2/atom/atom.h"
#include "lv2/urid/urid.h"

#include <stdarg.h>
#include <stdint.h>
#include <stdlib.h>

#define STATE_MAP_INIT(type, ptr) \
  (LV2_ATOM__##type), (sizeof(*ptr) - sizeof(LV2_Atom)), (ptr)


typedef struct {
  const char* uri;
  LV2_URID    urid;
  LV2_Atom*   value;
} StateMapItem;

static int
state_map_cmp(const void* a, const void* b)
{
  const StateMapItem* ka = (const StateMapItem*)a;
  const StateMapItem* kb = (const StateMapItem*)b;
  if (ka->urid < kb->urid) {
    return -1;
  }

  if (kb->urid < ka->urid) {
    return 1;
  }

  return 0;
}


static void
state_map_init(
  StateMapItem        dict[],
  LV2_URID_Map*       map,
  LV2_URID_Map_Handle handle,
  /* const char* uri, const char* type, uint32_t size, LV2_Atom* value */...)
{
  // Set dict entries from parameters
  unsigned i = 0;
  va_list  args;
  va_start(args, handle);
  for (const char* uri = NULL; (uri = va_arg(args, const char*)); ++i) {
    const char*     type  = va_arg(args, const char*);
    const uint32_t  size  = va_arg(args, uint32_t);
    LV2_Atom* const value = va_arg(args, LV2_Atom*);
    dict[i].uri           = uri;
    dict[i].urid          = map->map(map->handle, uri);
    dict[i].value         = value;
    dict[i].value->size   = size;
    dict[i].value->type   = map->map(map->handle, type);
  }
  va_end(args);

  // Sort for fast lookup by URID by state_map_find()
  qsort(dict, i, sizeof(StateMapItem), state_map_cmp);
}

static StateMapItem*
state_map_find(StateMapItem dict[], uint32_t n_entries, LV2_URID urid)
{
  const StateMapItem key = {NULL, urid, NULL};
  return (StateMapItem*)bsearch(
    &key, dict, n_entries, sizeof(StateMapItem), state_map_cmp);
}
#endif