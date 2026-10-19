#include <assert.h>
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))
#define HAVE_POS_MAP 0
#define POS_MAP_COMPLETE 0

struct zmk_key_physical_attrs { int16_t x, y; };
struct zmk_physical_layout {
    const struct zmk_key_physical_attrs *keys;
    size_t keys_len;
};
static const struct zmk_key_physical_attrs source_keys[] = {{0, 0}};
static const struct zmk_key_physical_attrs dest_keys[] = {{0, 0}, {100, 0}, {200, 0}};
static const struct zmk_physical_layout source_layout = {source_keys, ARRAY_SIZE(source_keys)};
static const struct zmk_physical_layout dest_layout = {dest_keys, ARRAY_SIZE(dest_keys)};
static const struct zmk_physical_layout *layouts[] = {&source_layout, &dest_layout};
