#pragma once
#include <obs-module.h>
#include <util/darray.h>
#include <va/va_x11.h>
#include <va/va_drm.h>

struct vaapi_display {
	vaapi_display_type_t type;
	const char *path;
	const char *name;

	VADisplay display;

	union {
		Display *x_display;
		int drm_fd;
	};

	DARRAY(vaapi_profile_caps_t) caps;
};

bool vaapi_display_open(vaapi_display_t *d);
void vaapi_display_close(vaapi_display_t *d);
