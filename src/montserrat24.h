//DO NOT CHANGE THIS
#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct { uint16_t offset, advance; uint8_t width, height; int8_t x, y; } StrynxMontserratGlyph;
extern const uint8_t strynxMontserrat24Bitmap[];
extern const StrynxMontserratGlyph strynxMontserrat24Glyphs[95];
#ifdef __cplusplus
}
#endif
