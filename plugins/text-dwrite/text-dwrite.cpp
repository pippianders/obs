#include <util/util.hpp>
#include <util/platform.h>
#include <thread>
#include <atomic>
#include "text-dwrite.hpp"

#define do_log(level, format, ...) \
	blog(level, "[dwrite text: '%s'] " format, \
			obs_source_get_name(source), ##__VA_ARGS__)

#define warn(format, ...)  do_log(LOG_WARNING, format, ##__VA_ARGS__)
#define info(format, ...)  do_log(LOG_INFO,    format, ##__VA_ARGS__)
#define debug(format, ...) do_log(LOG_DEBUG,   format, ##__VA_ARGS__)

#define warn_hr(str, hr)  warn (str ": %s", hr_msg(hr).c_str())

using namespace std;

struct DWriteTextSource {
	thread                         updateThread;

	obs_source_t                   *source = nullptr;

	ComPtr<IDWriteTextFormat>      format;
	ComPtr<IDWriteTextLayout>      layout;
	ComPtr<IWICBitmap>             bitmap;
	ComPtr<ID2D1RenderTarget>      target;
	ComPtr<ID2D1SolidColorBrush>   brush;


	BPtr<wchar_t>                  text;
	BPtr<wchar_t>                  file;
	BPtr<wchar_t>                  face;

	gs_texture_t                   *texture = nullptr;

	uint32_t                       color = 0xFFFFFFFF;
	int                            fontSize;
	UINT                           length = 0;
	UINT                           cx = 0;
	UINT                           cy = 0;
	UINT                           newCX = 0;
	UINT                           newCY = 0;
	bool                           bold = false;
	bool                           italic = false;
	bool                           underline = false;
	bool                           vertical = false;
	atomic<bool>                   updateReady = false;

	inline DWriteTextSource(obs_source_t *source_);
	inline ~DWriteTextSource()
	{
		if (updateThread.joinable())
			updateThread.join();

		if (texture) {
			obs_enter_graphics();
			gs_texture_destroy(texture);
			obs_leave_graphics();
		}
	}

	inline void Update(obs_data_t *settings);
	inline void Render();

	inline bool Update_Initialize();
	inline void Update_DrawText();
	inline void Update_CreateTexture();
};

inline bool DWriteTextSource::Update_Initialize()
{
	D2D1_RENDER_TARGET_PROPERTIES targetProps = {};
	DWRITE_TEXT_METRICS metrics;
	HRESULT hr;

	hr = dwrite->CreateTextFormat(face, nullptr,
			DWRITE_FONT_WEIGHT_REGULAR,
			DWRITE_FONT_STYLE_NORMAL,
			DWRITE_FONT_STRETCH_NORMAL,
			float(fontSize), L"en-us", &format);
	if (FAILED(hr)) {
		warn_hr("Failed to create text format", hr);
		return false;
	}

	hr = dwrite->CreateTextLayout(text, length, format,
			60000.0f, 60000.0f, &layout);
	if (FAILED(hr)) {
		warn_hr("Failed to create text layout", hr);
		return false;
	}

	hr = layout->GetMetrics(&metrics);
	if (FAILED(hr)) {
		warn_hr("Failed to get metrics", hr);
		return false;
	}

	newCX = UINT(metrics.widthIncludingTrailingWhitespace);
	newCY = UINT(metrics.height);

	hr = format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
	if (FAILED(hr)) {
		warn_hr("Failed to set text alignment", hr);
		return false;
	}

	hr = format->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR);
	if (FAILED(hr)) {
		warn_hr("Failed to set paragraph alignment", hr);
		return false;
	}

	hr = wic->CreateBitmap(newCX, newCY, GUID_WICPixelFormat32bppPBGRA,
			WICBitmapCacheOnDemand, &bitmap);
	if (FAILED(hr)) {
		warn_hr("Failed to create bitmap", hr);
		return false;
	}

	targetProps.pixelFormat.format = DXGI_FORMAT_B8G8R8A8_UNORM;
	targetProps.pixelFormat.alphaMode = D2D1_ALPHA_MODE_PREMULTIPLIED;

	hr = d2d->CreateWicBitmapRenderTarget(bitmap, &targetProps, &target);
	if (FAILED(hr)) {
		warn_hr("Failed to create render target", hr);
		return false;
	}

	uint32_t convertedColor =
		(color & 0xFF) << 16 |
		(color & 0xFF00) |
		(color & 0xFF0000) >> 16;

	D2D1::ColorF colorVal = D2D1::ColorF(convertedColor);
	hr = target->CreateSolidColorBrush(colorVal, &brush);
	if (FAILED(hr)) {
		warn_hr("Failed to create brush", hr);
		return false;
	}

	return true;
}

inline void DWriteTextSource::Update_DrawText()
{
	D2D1_RECT_F rect = D2D1::RectF(0.0f, 0.0f, 60000.0f, 60000.0f);

	target->BeginDraw();
	target->SetTransform(D2D1::IdentityMatrix());
	target->Clear(D2D1::ColorF(D2D1::ColorF::Black, 0.0f));
	target->DrawTextW(text, length, format, rect, brush);
	target->EndDraw();
}

inline void DWriteTextSource::Update_CreateTexture()
{
	WICRect wicRect = {0, 0, newCX, newCY};
	ComPtr<IWICBitmapLock> lock;
	HRESULT hr;
	UINT size;
	BYTE *ptr;

	hr = bitmap->Lock(&wicRect, WICBitmapLockRead, &lock);
	if (FAILED(hr)) return warn_hr("Failed to lock bitmap", hr);

	hr = lock->GetDataPointer(&size, &ptr);
	if (FAILED(hr)) return warn_hr("Failed to get pointer", hr);

	if (texture)
		gs_texture_destroy(texture);
	texture = gs_texture_create(newCX, newCY, GS_BGRA, 1,
			(const uint8_t**)&ptr, 0);

	cx = newCX;
	cy = newCY;
}

inline void DWriteTextSource::Update(obs_data_t *settings)
{
	obs_data_t *fontObj = obs_data_get_obj(settings, "font");
	const char *fontFace = obs_data_get_string(fontObj, "face");

	bool readFromFile = obs_data_get_bool(settings, "read_from_file");
	const char *newText = obs_data_get_string(settings, "text");
	const char *newFile = obs_data_get_string(settings, "file");
	color = (uint32_t)obs_data_get_int(settings, "color");

	if (updateThread.joinable())
		updateThread.join();

	fontSize = (int)obs_data_get_int(fontObj, "size");
	obs_data_release(fontObj);

	if (readFromFile) {
		BPtr<char> fileText;

		os_utf8_to_wcs_ptr(newFile, 0, &file);

		fileText = os_quick_read_utf8_file(newFile);
		length = (UINT)os_utf8_to_wcs_ptr(fileText, 0, &text);
	} else {
		length = (UINT)os_utf8_to_wcs_ptr(newText, 0, &text);
	}

	if (!length)
		text = nullptr;
	cx = cy = 0;

	if (!text || !length)
		return;

	os_utf8_to_wcs_ptr(fontFace, 0, &face);

	auto threadFunc = [this] () {
		if (Update_Initialize()) {
			Update_DrawText();
			updateReady = true;
		}
	};

	updateThread = move(thread(threadFunc));
}

inline void DWriteTextSource::Render()
{
	if (texture)
		obs_source_draw(texture, 0, 0, 0, 0, false);
	if (updateReady) {
		Update_CreateTexture();
		updateReady = false;
	}
}

inline DWriteTextSource::DWriteTextSource(obs_source_t *source_)
	: source(source_)
{
	obs_source_update(source, nullptr);
}

/* ------------------------------------------------------------------------- */

static const char *GetDWriteTextName(void*)
{
	return obs_module_text("ModuleName");
}

static void UpdateDWriteTextSource(void *data, obs_data_t *settings)
{
	reinterpret_cast<DWriteTextSource*>(data)->Update(settings);
}

static void RenderDWriteTextSource(void *data, gs_effect_t*)
{
	reinterpret_cast<DWriteTextSource*>(data)->Render();
}

static void DestroyDWriteTextSource(void *data)
{
	delete reinterpret_cast<DWriteTextSource*>(data);
}

static void *CreateDWriteText(obs_data_t*, obs_source_t *source)
{
	DWriteTextSource *s = new DWriteTextSource(source);
	return s;
}

static uint32_t GetDWriteTextWidth(void *data)
{
	return reinterpret_cast<DWriteTextSource*>(data)->cx;
}

static uint32_t GetDWriteTextHeight(void *data)
{
	return reinterpret_cast<DWriteTextSource*>(data)->cy;
}

#ifdef _WIN32
#define DEFAULT_FACE "Arial"
#elif __APPLE__
#define DEFAULT_FACE "Helvetica"
#else
#define DEFAULT_FACE "Sans Serif"
#endif

static void GetDWriteTextDefaults(obs_data_t *settings)
{
	obs_data_t *fontObj = obs_data_create();
	obs_data_set_default_string(fontObj, "face", DEFAULT_FACE);
	obs_data_set_default_int(fontObj, "size", 32);
	obs_data_set_default_obj(settings, "font", fontObj);
	obs_data_release(fontObj);

	obs_data_set_default_int(settings, "color", 0xFFFFFFFF);
}

static bool read_from_file_changed(obs_properties_t *props, obs_property_t *p,
		obs_data_t *settings)
{
	bool readFromFile = obs_data_get_bool(settings, "read_from_file");

	p = obs_properties_get(props, "text");
	obs_property_set_visible(p, !readFromFile);

	p = obs_properties_get(props, "file");
	obs_property_set_visible(p, readFromFile);

	return true;
}

static obs_properties_t *GetDWriteTextProperties(void*)
{
	obs_properties_t *props = obs_properties_create();
	obs_property_t *p;

	string filter;
	filter += obs_module_text("FileFilter.TextFiles");
	filter += " (*.txt);;";
	filter += obs_module_text("FileFilter.AllFiles");
	filter += " (*.*)";

	obs_properties_add_font(props, "font", obs_module_text("Font"));

	p = obs_properties_add_bool(props, "read_from_file",
			obs_module_text("ReadFromFile"));
	obs_property_set_modified_callback(p, read_from_file_changed);

	obs_properties_add_text(props, "text", obs_module_text("Text"),
			OBS_TEXT_MULTILINE);
	obs_properties_add_path(props, "file", obs_module_text("File"),
			OBS_PATH_FILE, filter.c_str(), nullptr);
	obs_properties_add_color(props, "color", obs_module_text("Color"));
	return props;
}

void RegisterDWriteTextSource()
{
	obs_source_info info = {};
	info.id              = "text_directwrite";
	info.type            = OBS_SOURCE_TYPE_INPUT;
	info.output_flags    = OBS_SOURCE_VIDEO;
	info.get_name        = GetDWriteTextName;
	info.create          = CreateDWriteText;
	info.destroy         = DestroyDWriteTextSource;
	info.update          = UpdateDWriteTextSource;
	info.video_render    = RenderDWriteTextSource;
	info.get_width       = GetDWriteTextWidth;
	info.get_height      = GetDWriteTextHeight;
	info.get_defaults    = GetDWriteTextDefaults;
	info.get_properties  = GetDWriteTextProperties;
	obs_register_source(&info);
}
