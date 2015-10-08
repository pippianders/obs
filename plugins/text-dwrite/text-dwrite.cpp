#include <util/util.hpp>
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
	obs_source_t                   *source = nullptr;

	ComPtr<IDWriteTextFormat>      format;
	ComPtr<IDWriteTextLayout>      layout;
	ComPtr<IWICBitmap>             bitmap;
	ComPtr<ID2D1RenderTarget>      target;
	ComPtr<ID2D1SolidColorBrush>   brush;

	BPtr<wchar_t>                  text;
	UINT                           length = 0;

	gs_texture_t                   *texture = nullptr;

	DWORD                          color = 0xFFFFFF;
	int                            size = 0;
	UINT                           cx = 0;
	UINT                           cy = 0;
	UINT                           newCX = 0;
	UINT                           newCY = 0;
	bool                           bold = false;
	bool                           italic = false;
	bool                           underline = false;
	bool                           vertical = false;

	inline DWriteTextSource(obs_source_t *source_);
	inline ~DWriteTextSource()
	{
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

	hr = dwrite->CreateTextFormat(L"Arial", nullptr,
			DWRITE_FONT_WEIGHT_REGULAR,
			DWRITE_FONT_STYLE_NORMAL,
			DWRITE_FONT_STRETCH_NORMAL,
			72.0f, L"en-us", &format);
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

	newCX = UINT(metrics.width);
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

	hr = target->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::White),
			&brush);
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
	UINT stride;
	HRESULT hr;
	UINT size;
	BYTE *ptr;

	hr = bitmap->Lock(&wicRect, WICBitmapLockRead, &lock);
	if (FAILED(hr)) return warn_hr("Failed to lock bitmap", hr);

	hr = lock->GetDataPointer(&size, &ptr);
	if (FAILED(hr)) return warn_hr("Failed to get pointer", hr);

	hr = lock->GetStride(&stride);
	if (FAILED(hr)) return warn_hr("Failed to get stride", hr);

	obs_enter_graphics();
	texture = gs_texture_create(newCX, newCY, GS_BGRA, 1,
			(const uint8_t**)&ptr, 0);
	obs_leave_graphics();

	cx = newCX;
	cy = newCY;
}

inline void DWriteTextSource::Update(obs_data_t *settings)
{
	if (texture) {
		obs_enter_graphics();
		gs_texture_destroy(texture);
		texture = nullptr;
		obs_leave_graphics();
	}


	text = bwstrdup(L"Well what do you know, this test implementation of "
			L"DirectWrite is working.");
	length = text ? UINT(wcslen(text)) : 0;
	cx = cy = 0;

	if (!text || !length)
		return;

	if (Update_Initialize()) {
		Update_DrawText();
		Update_CreateTexture();
	}
}

inline void DWriteTextSource::Render()
{
	if (texture)
		obs_source_draw(texture, 0, 0, 0, 0, false);
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

static void *CreateDWriteText(obs_data_t *settings, obs_source_t *source)
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

static void GetDWriteTextDefaults(obs_data_t *settings)
{
}

static obs_properties_t *GetDWriteTextProperties(void*)
{
	return nullptr;
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
