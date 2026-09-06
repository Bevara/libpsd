/*
 *  psd_sdk-facing half of the PSD filter - see psd_decode.h for why it is a
 *  translation unit of its own.
 */

#include <stdlib.h>
#include <string.h>

#include <Psd/Psd.h>
#include <Psd/PsdPlatform.h>
#include <Psd/PsdMallocAllocator.h>
#include <Psd/PsdMemoryFile.h>
#include <Psd/PsdDocument.h>
#include <Psd/PsdColorMode.h>
#include <Psd/PsdImageDataSection.h>
#include <Psd/PsdPlanarImage.h>
#include <Psd/PsdParseDocument.h>
#include <Psd/PsdParseImageDataSection.h>

#include "psd_decode.h"

extern "C" void psd_decode_free(unsigned char *buffer)
{
	free(buffer);
}

extern "C" int psd_decode_rgb(const unsigned char *data, size_t size,
                              unsigned char **out, unsigned int *width, unsigned int *height)
{
	/* "8BPS" plus the version word: 1 for PSD, 2 for PSB. Checked here so a
	 * wrong file is rejected before psd_sdk starts allocating. */
	if (size < 26 || memcmp(data, "8BPS", 4) != 0)
		return PSD_DEC_ERR_BITSTREAM;

	psd::MallocAllocator allocator;
	psd::MemoryFile file(&allocator);
	if (!file.Open(data, (uint64_t)size))
		return PSD_DEC_ERR_BITSTREAM;

	psd::Document *document = psd::CreateDocument(&file, &allocator);
	if (!document)
		return PSD_DEC_ERR_BITSTREAM;

	int ret = PSD_DEC_ERR_MODE;
	unsigned char *rgb = NULL;
	psd::ImageDataSection *imageData = NULL;

	/* Only 8-bit RGB is handled. Indexed, CMYK, Lab and duotone would each need
	 * their own conversion, and 16/32-bit channels a depth reduction with no
	 * single right answer; none of them appears in the test corpus. */
	if (document->colorMode != psd::colorMode::RGB || document->bitsPerChannel != 8)
		goto cleanup;

	/* The merged image only exists when the document was saved with "Maximize
	 * Compatibility"; without it there is nothing to show but the layers, which
	 * would have to be composited. */
	if (document->imageDataSection.length == 0)
	{
		ret = PSD_DEC_ERR_NOMERGED;
		goto cleanup;
	}

	imageData = psd::ParseImageDataSection(document, &file, &allocator);
	if (!imageData || imageData->imageCount < 3)
	{
		ret = PSD_DEC_ERR_BITSTREAM;
		goto cleanup;
	}

	{
		/* images[0..2] are the R, G and B planes, each the full size of the
		 * canvas. A fourth image, when present, is either a transparency mask
		 * or a spot-colour channel; either way it is dropped, since the output
		 * pid is RGB (an RGBA pid has no adaptation path to writegen here). */
		const size_t pixels = (size_t)document->width * document->height;
		const unsigned char *r = (const unsigned char *)imageData->images[0].data;
		const unsigned char *g = (const unsigned char *)imageData->images[1].data;
		const unsigned char *b = (const unsigned char *)imageData->images[2].data;
		if (!r || !g || !b)
		{
			ret = PSD_DEC_ERR_BITSTREAM;
			goto cleanup;
		}

		rgb = (unsigned char *)malloc(pixels * 3);
		if (!rgb)
		{
			ret = PSD_DEC_ERR_MEMORY;
			goto cleanup;
		}
		for (size_t i = 0; i < pixels; i++)
		{
			rgb[i * 3] = r[i];
			rgb[i * 3 + 1] = g[i];
			rgb[i * 3 + 2] = b[i];
		}

		*out = rgb;
		*width = document->width;
		*height = document->height;
		rgb = NULL;
		ret = PSD_DEC_OK;
	}

cleanup:
	free(rgb);
	if (imageData)
		psd::DestroyImageDataSection(imageData, &allocator);
	psd::DestroyDocument(document, &allocator);
	return ret;
}
