#pragma once

#include <va/va.h>
#include <stdint.h>
#include <stdbool.h>

#include "vaapi-display.h"
#include "vaapi-encoder.h"

struct vaapi_profile_def {
	const char *name;
	VAProfile va;
	vaapi_profile_t vaapi;
};
typedef struct vaapi_profile_def vaapi_profile_def_t;

struct vaapi_profile_caps {
	vaapi_profile_def_t def;
	VAEntrypoint entrypoint;
	VAConfigAttrib *attribs;
	size_t attribs_cnt;
	uint32_t format;
	uint32_t rc;
	uint32_t max_width;
	uint32_t max_height;
};
typedef struct vaapi_profile_caps vaapi_profile_caps_t;

typedef DARRAY(vaapi_profile_caps_t) vaapi_display_caps_t;

bool vaapi_profile_caps_init(vaapi_display_t *display);

vaapi_profile_caps_t *vaapi_caps_from_profile(vaapi_display_t *display,
		vaapi_profile_t profile);

static const char * vaapi_rc_to_str(uint32_t rc)
{
#define RC_CASE(x) case VA_RC_ ## x: return #x
	switch (rc)
	{
	RC_CASE(NONE);
	RC_CASE(CBR);
	RC_CASE(VBR);
	RC_CASE(VCM);
	RC_CASE(CQP);
	RC_CASE(VBR_CONSTRAINED);
	default: return "Invalid RC";
	}
#undef RC_CASE
}
