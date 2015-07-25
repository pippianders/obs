#include <obs-module.h>
#include <util/darray.h>
#include <util/platform.h>

#include <stdio.h>

#include "vaapi-encoder.h"
#include "vaapi-caps.h"

struct vaapi_enc
{
	uint32_t width;
	uint32_t height;
	uint32_t bitrate;
	bool cbr;
	enum video_format format;
	vaapi_profile_caps_t *caps;
	vaapi_encoder_t *encoder;

	vaapi_slice_type_t packet_slice_type;
	uint64_t packet_pts;
	DARRAY(uint8_t) packet;
};

static void vaapi_enc_video_info(void *data,
		struct video_scale_info *info)
{
	struct vaapi_enc *enc = data;

	info->format = enc->format;
}

static const char *vaapi_enc_getname(void)
{
	return obs_module_text("VaapiH264Encoder");
}

static bool vaapi_enc_update(void *data, obs_data_t *settings)
{
	return true;
}

static bool vaapi_enc_extra_data(void *data, uint8_t **extra_data, size_t *size)
{
	struct vaapi_enc *enc = data;

	return vaapi_encoder_extra_data(enc->encoder, extra_data, size);
}

static bool vaapi_enc_sei(void *data, uint8_t **sei, size_t *size)
{
	return false;
}

void coded_block(void *opaque, coded_block_entry_t *e)
{
	struct vaapi_enc *enc = opaque;
 	if (e->type == VAAPI_SLICE_TYPE_I) {
		size_t extra_data_size;
		uint8_t *extra_data;
		if (vaapi_encoder_extra_data(enc->encoder, &extra_data,
				&extra_data_size)) {
			da_push_back_array(enc->packet, extra_data,
					extra_data_size);
		}
	}
	da_push_back_array(enc->packet, e->data.array, e->data.num);
	enc->packet_slice_type = e->type;
	enc->packet_pts = e->pts;

	da_free(e->data);
}

static bool vaapi_enc_encode(void *data, struct encoder_frame *frame,
		struct encoder_packet *packet, bool *received_packet)
{
	struct vaapi_enc *enc = data;

	da_resize(enc->packet, 0);

	vaapi_encoder_encode(enc->encoder, frame);

	if (enc->packet.num == 0) {
		*received_packet = false;
		return true;
	}

	packet->type = OBS_ENCODER_VIDEO;
	packet->pts = enc->packet_pts;
	packet->dts = enc->packet_pts;
	packet->data = enc->packet.array;
	packet->size = enc->packet.num;
	packet->keyframe = enc->packet_slice_type == VAAPI_SLICE_TYPE_I;
	*received_packet = true;
	return true;
}

static void vaapi_enc_destroy(void *data)
{
	struct vaapi_enc *enc = data;

	if (enc) {
		if (enc->encoder)
			vaapi_encoder_destroy(enc->encoder);
		da_free(enc->packet);
		bfree(enc);
	}
}

static void *vaapi_enc_create(obs_data_t *settings,
		obs_encoder_t *encoder)
{
	struct vaapi_enc *enc = bzalloc(sizeof(struct vaapi_enc));

	video_t *video = obs_encoder_video(encoder);
	const struct video_output_info *voi = video_output_get_info(video);

	enc->width = obs_encoder_get_width(encoder);
	enc->height = obs_encoder_get_height(encoder);

	enc->bitrate = 5000;
	enc->cbr = true;

	vaapi_encoder_attribs_t attribs = {
		.profile = VAAPI_PROFILE_HIGH,
		.width = enc->width,
		.height = enc->height,
		.bitrate = enc->bitrate,
		.cbr = enc->cbr,
		.framerate_num = voi->fps_num,
		.framerate_den = voi->fps_den,
		.keyint = 2,
		.refpic_cnt = 3,
		.surface_cnt = 2,
		.coded_block_cb = coded_block,
		.coded_block_cb_opaque = enc
	};
	enc->format = VIDEO_FORMAT_NV12;
	enc->encoder = vaapi_encoder_create(&attribs);

	if (enc->encoder == NULL)
		goto fail;

	return enc;

fail:
	vaapi_enc_destroy(enc);

	return NULL;
}

#define TEXT_BITRATE    obs_module_text("Bitrate")
#define TEXT_CUSTOM_BUF obs_module_text("CustomBufsize")
#define TEXT_BUF_SIZE   obs_module_text("BufferSize")
#define TEXT_USE_CBR    obs_module_text("UseCBR")
#define TEXT_VFR        obs_module_text("VFR")
#define TEXT_CRF        obs_module_text("CRF")
#define TEXT_KEYINT_SEC obs_module_text("KeyframeIntervalSec")
#define TEXT_PROFILE    obs_module_text("Profile")
#define TEXT_NONE       obs_module_text("None")

static bool use_bufsize_modified(obs_properties_t *ppts, obs_property_t *p,
		obs_data_t *settings)
{
	bool use_bufsize = obs_data_get_bool(settings, "use_bufsize");
	p = obs_properties_get(ppts, "buffer_size");
	obs_property_set_visible(p, use_bufsize);
	return true;
}

static bool use_cbr_modified(obs_properties_t *ppts, obs_property_t *p,
		obs_data_t *settings)
{
	bool cbr = obs_data_get_bool(settings, "cbr");
	p = obs_properties_get(ppts, "crf");
	obs_property_set_visible(p, !cbr);
	return true;
}

static obs_properties_t *vaapi_enc_props(void *unused)
{
	UNUSED_PARAMETER(unused);

	obs_properties_t *props = obs_properties_create();
	obs_property_t *list;
	obs_property_t *p;

	obs_properties_add_int(props, "bitrate", TEXT_BITRATE, 50, 10000000, 1);

	p = obs_properties_add_bool(props, "use_bufsize", TEXT_CUSTOM_BUF);
	obs_property_set_modified_callback(p, use_bufsize_modified);
	obs_properties_add_int(props, "buffer_size", TEXT_BUF_SIZE, 0,
			10000000, 1);

	obs_properties_add_int(props, "keyint_sec", TEXT_KEYINT_SEC, 0, 20, 1);
	p = obs_properties_add_bool(props, "cbr", TEXT_USE_CBR);
	obs_properties_add_int(props, "crf", TEXT_CRF, 0, 51, 1);

	obs_property_set_modified_callback(p, use_cbr_modified);

	list = obs_properties_add_list(props, "profile", TEXT_PROFILE,
			OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);
	obs_property_list_add_string(list, TEXT_NONE, "");
	obs_property_list_add_string(list, "baseline", "baseline");
	obs_property_list_add_string(list, "main", "main");
	obs_property_list_add_string(list, "high", "high");

	obs_properties_add_bool(props, "vfr", TEXT_VFR);

	return props;
}

static void vaapi_enc_defaults(obs_data_t *settings)
{
	obs_data_set_default_int   (settings, "bitrate",     2500);
	obs_data_set_default_bool  (settings, "use_bufsize", false);
	obs_data_set_default_int   (settings, "buffer_size", 2500);
	obs_data_set_default_int   (settings, "keyint_sec",  0);
	obs_data_set_default_int   (settings, "crf",         23);
	obs_data_set_default_bool  (settings, "vfr",         false);
	obs_data_set_default_bool  (settings, "cbr",         true);
	obs_data_set_default_string(settings, "profile",     "");
}

struct obs_encoder_info obs_vaapi_encoder = {
	.id             = "vaapi_h264",
	.type           = OBS_ENCODER_VIDEO,
	.codec          = "h264",
	.get_name       = vaapi_enc_getname,
	.create         = vaapi_enc_create,
	.destroy        = vaapi_enc_destroy,
	.encode         = vaapi_enc_encode,
	.update         = vaapi_enc_update,
	.get_properties = vaapi_enc_props,
	.get_defaults   = vaapi_enc_defaults,
	.get_extra_data = vaapi_enc_extra_data,
	.get_sei_data   = vaapi_enc_sei,
	.get_video_info = vaapi_enc_video_info
};

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("linux-vaapi", "en-US")

bool obs_module_load(void)
{
	vaapi_caps_init();

	obs_register_encoder(&obs_vaapi_encoder);
	return true;
}

void obs_module_unload(void)
{
	vaapi_caps_destroy();
}
