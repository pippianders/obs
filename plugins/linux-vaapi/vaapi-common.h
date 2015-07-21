#pragma once

#include <obs-module.h>
#include <va/va.h>

#include "vaapi-encoder.h"

#define VA_LOG(level, format, ...) \
	blog(level, "[VAAPI encoder]: " format, ##__VA_ARGS__)

#define VA_LOG_STATUS(level, x, status) \
	VA_LOG(LOG_ERROR, "%s: %s", #x, vaErrorStr(status));

#define CHECK_STATUS_(x, y) \
	do { \
		status = (x); \
		if (status != VA_STATUS_SUCCESS) { \
			VA_LOG_STATUS(LOG_ERROR, #x, status); \
			y; \
		} \
	} while (false)

#define CHECK_STATUS_FAIL(x) \
	CHECK_STATUS_(x, goto fail)

#define CHECK_STATUS_FAILN(x, n) \
	CHECK_STATUS_(x, goto fail ## n)

#define CHECK_STATUS_FALSE(x) \
	CHECK_STATUS_(x, return false)

const char *vaapi_vendor(vaapi_encoder_t *enc);
VADisplay vaapi_get_display();
