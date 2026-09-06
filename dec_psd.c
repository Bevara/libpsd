/*
 *			GPAC - Multimedia Framework C SDK
 *
 *  This file is part of GPAC / JPEG XR decoder filter, based on jxrlib
 *  (https://github.com/4creators/jxrlib), the reference implementation
 *  Microsoft contributed to ITU-T T.832.
 *
 *  Takes a whole .jxr/.wdp/.hdp file and outputs one raw RGB frame. The
 *  jxrlib calls themselves live in jxr_decode.c, whose headers cannot be
 *  mixed with GPAC's - see psd_decode.h.
 */

#include <gpac/filters.h>
#include <gpac/constants.h>
#include <string.h>
#include <stdlib.h>

#include "psd_decode.h"

typedef struct
{
	GF_FilterPid *ipid, *opid;
	Bool is_playing;
} GF_PSDDecCtx;

static GF_Err psddec_configure_pid(GF_Filter *filter, GF_FilterPid *pid, Bool is_remove)
{
	GF_PSDDecCtx *ctx = (GF_PSDDecCtx *)gf_filter_get_udta(filter);

	if (is_remove)
	{
		if (ctx->opid)
		{
			gf_filter_pid_remove(ctx->opid);
			ctx->opid = NULL;
		}
		ctx->ipid = NULL;
		return GF_OK;
	}
	if (!gf_filter_pid_check_caps(pid))
		return GF_NOT_SUPPORTED;

	ctx->ipid = pid;
	gf_filter_pid_set_framing_mode(pid, GF_TRUE);

	if (!ctx->opid)
		ctx->opid = gf_filter_pid_new(filter);

	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_STREAM_TYPE, &PROP_UINT(GF_STREAM_VISUAL));
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_CODECID, &PROP_UINT(GF_CODECID_RAW));
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_PIXFMT, &PROP_UINT(GF_PIXEL_RGB));

	return GF_OK;
}

static Bool psddec_process_event(GF_Filter *filter, const GF_FilterEvent *evt)
{
	GF_PSDDecCtx *ctx = (GF_PSDDecCtx *)gf_filter_get_udta(filter);
	switch (evt->base.type)
	{
	case GF_FEVT_PLAY:
		ctx->is_playing = GF_TRUE;
		return GF_FALSE;
	case GF_FEVT_STOP:
		ctx->is_playing = GF_FALSE;
		return GF_FALSE;
	default:
		return GF_FALSE;
	}
}

static GF_Err psddec_process(GF_Filter *filter)
{
	GF_FilterPacket *pck, *dst_pck;
	u8 *data, *output;
	u32 size;
	unsigned char *rgb = NULL;
	unsigned int width = 0, height = 0;
	int res;
	GF_PSDDecCtx *ctx = (GF_PSDDecCtx *)gf_filter_get_udta(filter);

	pck = gf_filter_pid_get_packet(ctx->ipid);
	if (!pck)
	{
		if (gf_filter_pid_is_eos(ctx->ipid))
		{
			gf_filter_pid_set_eos(ctx->opid);
			return GF_EOS;
		}
		return GF_OK;
	}
	data = (u8 *)gf_filter_pck_get_data(pck, &size);
	if (!data)
	{
		gf_filter_pid_drop_packet(ctx->ipid);
		return GF_IO_ERR;
	}

	res = psd_decode_rgb(data, size, &rgb, &width, &height);
	gf_filter_pid_drop_packet(ctx->ipid);

	switch (res)
	{
	case PSD_DEC_OK:
		break;
	case PSD_DEC_ERR_MEMORY:
		return GF_OUT_OF_MEM;
	case PSD_DEC_ERR_NOMERGED:
		GF_LOG(GF_LOG_ERROR, GF_LOG_CODEC, ("[PSDDec] No merged image in this document (saved without \"Maximize Compatibility\")\n"));
		return GF_NOT_SUPPORTED;
	case PSD_DEC_ERR_MODE:
		GF_LOG(GF_LOG_ERROR, GF_LOG_CODEC, ("[PSDDec] Unsupported colour mode or bit depth: only 8-bit RGB is handled\n"));
		return GF_NOT_SUPPORTED;
		default:
		GF_LOG(GF_LOG_ERROR, GF_LOG_CODEC, ("[PSDDec] Not a valid PSD document\n"));
		return GF_NON_COMPLIANT_BITSTREAM;
	}

	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_WIDTH, &PROP_UINT(width));
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_HEIGHT, &PROP_UINT(height));
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_STRIDE, &PROP_UINT(width * 3));
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_PIXFMT, &PROP_UINT(GF_PIXEL_RGB));

	dst_pck = gf_filter_pck_new_alloc(ctx->opid, width * height * 3, &output);
	if (!dst_pck)
	{
		psd_decode_free(rgb);
		return GF_OUT_OF_MEM;
	}
	memcpy(output, rgb, width * height * 3);
	psd_decode_free(rgb);

	gf_filter_pck_set_cts(dst_pck, 0);
	gf_filter_pck_set_sap(dst_pck, GF_FILTER_SAP_1);
	gf_filter_pck_send(dst_pck);

	gf_filter_pid_set_eos(ctx->opid);
	return GF_EOS;
}

static void psddec_finalize(GF_Filter *filter)
{
}

static const GF_FilterCapability PSDDecCaps[] =
	{
		CAP_UINT(GF_CAPS_INPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE),
		CAP_STRING(GF_CAPS_INPUT, GF_PROP_PID_FILE_EXT, "psd|psb|pdd"),
		CAP_STRING(GF_CAPS_INPUT, GF_PROP_PID_MIME, "image/vnd.adobe.photoshop|image/x-photoshop|application/x-photoshop"),
		CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_VISUAL),
		CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_CODECID, GF_CODECID_RAW),
};

GF_FilterRegister PSDDecoderRegister = {
	.name = "psddec",
	GF_FS_SET_DESCRIPTION("Photoshop (PSD/PSB) decoder")
		GF_FS_SET_HELP("This filter decodes the merged image of Photoshop PSD/PSB documents using psd_sdk. The document must have been saved with \"Maximize Compatibility\", which is what writes that merged image.")
			.private_size = sizeof(GF_PSDDecCtx),
	SETCAPS(PSDDecCaps),
	.configure_pid = psddec_configure_pid,
	.process = psddec_process,
	.process_event = psddec_process_event,
	.finalize = psddec_finalize,
};

const GF_FilterRegister *EMSCRIPTEN_KEEPALIVE psddec_register(GF_FilterSession *session)
{
	return &PSDDecoderRegister;
}

#include "filter_register.h"
__attribute__((constructor))
void register_psddec(void) {
    gf_filter_auto_register("psddec", psddec_register);
}
