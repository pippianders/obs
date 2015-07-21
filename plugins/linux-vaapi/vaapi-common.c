#include "vaapi-caps.h"
#include "vaapi-common.h"

#include <va/va.h>
#include <va/va_x11.h>

VADisplay vaapi_get_display()
{
	static VADisplay va_display = NULL;
	VAStatus status;
	Display *disp;
	int major_version, minor_version;

	if (va_display != NULL)
		return va_display;

	disp = XOpenDisplay(NULL);
	if (disp == NULL) {
		VA_LOG(LOG_ERROR, "XOpenDisplay failed");
		goto fail;
	}

	va_display = vaGetDisplay(disp);
	if (va_display == NULL) {
		VA_LOG(LOG_ERROR, "vaGetDisplay failed");
		goto fail;
	}

	if (!vaDisplayIsValid(va_display)) {
		VA_LOG(LOG_ERROR, "invalid VADisplay");
		goto fail;
	}

	CHECK_STATUS_FAIL(vaInitialize(va_display, &major_version,
			&minor_version));

	const char *vendor = vaQueryVendorString(va_display);
	VA_LOG(LOG_INFO, "VAAPI Version=%d.%d (%s), Vendor='%s'", major_version,
			minor_version, VA_VERSION_S,
			vendor != NULL ? vendor : "Unknown vendor");

	return va_display;

fail:
	if (disp != NULL)
		XCloseDisplay(disp);
	if (va_display != NULL) {
		vaTerminate(va_display);
		va_display = NULL;
	}

	return NULL;
}
