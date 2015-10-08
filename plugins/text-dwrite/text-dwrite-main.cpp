#include "text-dwrite.hpp"
#include <sstream>

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("text-dwrite", "en-US")

void RegisterDWriteTextSource();

ComPtr<IWICImagingFactory> wic;
ComPtr<IDWriteFactory>     dwrite;
ComPtr<ID2D1Factory>       d2d;

std::string hr_msg(HRESULT hr)
{
	std::stringstream msg;
	LPSTR buffer = NULL;

	msg << "(0x" << std::hex << (unsigned long)hr << ")";

	FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM |
			FORMAT_MESSAGE_ALLOCATE_BUFFER |
			FORMAT_MESSAGE_IGNORE_INSERTS,
			NULL, hr, MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US),
			(LPSTR)&buffer, 0, NULL);

	if (buffer) {
		msg << " " << buffer;
		LocalFree(buffer);
	}

	return msg.str();
}

bool obs_module_load(void)
{
	HRESULT hr;

	hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &d2d);
	if (FAILED(hr)) {
		blog(LOG_ERROR, "Could not initialize Direct2D factory: %s",
				hr_msg(hr));
		return true;
	}

	hr = DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED,
			__uuidof(IDWriteFactory), (IUnknown**)&dwrite);
	if (FAILED(hr)) {
		blog(LOG_ERROR, "Could not initialize DirectWrite factory: %s",
				hr_msg(hr));
		return true;
	}

	hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr,
			CLSCTX_INPROC_SERVER, IID_IWICImagingFactory,
			(void**)&wic);
	if (FAILED(hr)) {
		blog(LOG_ERROR, "Could not initialize WIC Imaging factory: %s",
				hr_msg(hr));
		return true;
	}

	RegisterDWriteTextSource();
	return true;
}

void obs_module_unload(void)
{
	dwrite.Clear();
	d2d.Clear();
	wic.Clear();
}
