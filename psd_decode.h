/*
 *  Minimal C surface over psd_sdk.
 *
 *  psd_sdk is C++ with its own namespace and allocator objects, while the
 *  filter is plain C against the GPAC API; keeping them in separate
 *  translation units means the filter never sees a C++ type.
 */

#ifndef _PSD_DECODE_H_
#define _PSD_DECODE_H_

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Decodes the merged (flattened) image of a PSD/PSB document to a tightly
 * packed 24-bit RGB buffer.
 *
 * On success returns 0, stores a malloc()'d buffer in *out (released with
 * psd_decode_free) and its dimensions in *width and *height. */
int psd_decode_rgb(const unsigned char *data, size_t size,
                   unsigned char **out, unsigned int *width, unsigned int *height);

void psd_decode_free(unsigned char *buffer);

#define PSD_DEC_OK              0
#define PSD_DEC_ERR_MEMORY     -1
#define PSD_DEC_ERR_BITSTREAM  -2   /* not a PSD file, or a broken one */
#define PSD_DEC_ERR_MODE       -3   /* colour mode or bit depth not handled */
#define PSD_DEC_ERR_NOMERGED   -4   /* no merged image section in the document */

#ifdef __cplusplus
}
#endif

#endif
