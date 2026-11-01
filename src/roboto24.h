//DO NOT CHANGE THIS
#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct { uint16_t offset; uint8_t width, height, advance; int8_t x, y; } StrynxGlyph;
extern const uint8_t strynxRoboto24Bitmap[];
extern const StrynxGlyph strynxRoboto24Glyphs[];
#ifdef __cplusplus
}
#endif
