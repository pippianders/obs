/*

This file is provided under a dual BSD/GPLv2 license.  When using or
redistributing this file, you may do so under either license.

GPL LICENSE SUMMARY

Copyright(c) Oct. 2015 Intel Corporation.

This program is free software; you can redistribute it and/or modify
it under the terms of version 2 of the GNU General Public License as
published by the Free Software Foundation.

This program is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
General Public License for more details.

Contact Information:

Seung-Woo Kim, seung-woo.kim@intel.com
705 5th Ave S #500, Seattle, WA 98104

BSD LICENSE

Copyright(c) <date> Intel Corporation.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:

* Redistributions of source code must retain the above copyright
notice, this list of conditions and the following disclaimer.

* Redistributions in binary form must reproduce the above copyright
notice, this list of conditions and the following disclaimer in
the documentation and/or other materials provided with the
distribution.

* Neither the name of Intel Corporation nor the names of its
contributors may be used to endorse or promote products derived
from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#pragma once

#include <Windows.h>
#include "mfxstructures.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

	static const char * const qsv_ratecontrol_names[] = { "CBR", "VBR", "VCM", "CQP", "AVBR", "ICQ", "LA_ICQ", "LA", 0 };
	static const char * const qsv_profile_names[] = { "baseline", "main", "high", 0 };
	static const char * const qsv_usage_names[] = { "quality", "balanced", "speed", 0 };

	typedef struct qsv_t qsv_t;

	typedef struct 
	{
		mfxU16 nTargetUsage;	// 1 through 7, 1 being best quality and 7 being the best speed
		mfxU16 nWidth;			// source picture width
		mfxU16 nHeight;			// source picture height
		mfxU16 nAsyncDepth;
		mfxU16 nFpsNum;
		mfxU16 nFpsDen;
		mfxU16 nTargetBitRate;
		mfxU16 nMaxBitRate;
		mfxU16 nCodecProfile;
		mfxU16 nRateControl;
		mfxU16 nAccuracy;
		mfxU16 nConvergence;
		mfxU16 nQPI;
		mfxU16 nQPP;
		mfxU16 nQPB;
		mfxU16 nLADEPTH;
		mfxU16 nKeyIntSec;
		mfxU16 nbFrames;
		mfxU16 nICQQuality;
	} qsv_param_t;

	int qsv_encoder_close(qsv_t *);
	int qsv_param_parse(qsv_param_t *, const char *name, const char *value);
	int qsv_param_apply_profile(qsv_param_t *, const char *profile);
	int qsv_param_default_preset(qsv_param_t *, const char *preset, const char *tune);
	int qsv_encoder_reconfig(qsv_t *, qsv_param_t *);
	
	qsv_t *qsv_encoder_open( qsv_param_t * );
	int qsv_encoder_encode(qsv_t *, uint64_t, uint8_t *, uint8_t *, uint32_t, uint32_t, mfxBitstream **pBS);
	int qsv_encoder_headers(qsv_t *, uint8_t **pSPS, uint8_t **pPPS, uint16_t *pnSPS, uint16_t *pnPPS);

#ifdef __cplusplus
}
#endif