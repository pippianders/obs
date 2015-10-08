#pragma once

#include <obs-module.h>
#include <DWrite.h>
#include <D2D1.h>
#include <Wincodec.h>
#include <util/windows/ComPtr.hpp>
#include <string>

extern ComPtr<IWICImagingFactory> wic;
extern ComPtr<IDWriteFactory>     dwrite;
extern ComPtr<ID2D1Factory>       d2d;

extern std::string hr_msg(HRESULT hr);
