// Copyright (c) 2012- PPSSPP Project.

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.0 or later versions.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License 2.0 for more details.

// A copy of the GPL 2.0 should have been included with the program.
// If not, see http://www.gnu.org/licenses/

// Official git repository and contact information can be found at
// https://github.com/hrydgard/ppsspp and http://www.ppsspp.org/.

#include <algorithm>

#include "Common/Data/Convert/ColorConv.h"
#include "Common/Profiler/Profiler.h"
#include "Common/GPU/OpenGL/GLCommon.h"
#include "Common/GPU/OpenGL/GLDebugLog.h"
#include "Common/GPU/OpenGL/GLSLProgram.h"
#include "Common/GPU/thin3d.h"
#include "Core/MemMap.h"
#include "Core/Config.h"
#include "Core/ConfigValues.h"
#include "Core/System.h"
#include "Core/Reporting.h"
#include "GPU/ge_constants.h"
#include "GPU/GPUState.h"
#include "GPU/Common/FramebufferManagerCommon.h"
#include "GPU/Common/PresentationCommon.h"
#include "GPU/Common/TextureDecoder.h"
#include "GPU/Debugger/Stepping.h"
#include "GPU/GLES/FramebufferManagerGLES.h"
#include "GPU/GLES/TextureCacheGLES.h"
#include "GPU/GLES/ShaderManagerGLES.h"

FramebufferManagerGLES::FramebufferManagerGLES(Draw::DrawContext *draw) :
	FramebufferManagerCommon(draw)
{
	needBackBufferYSwap_ = true;
	presentation_->SetLanguage(draw_->GetShaderLanguageDesc().shaderLanguage);
}

FramebufferManagerGLES::~FramebufferManagerGLES() {
	delete[] convBuf_;
}

void FramebufferManagerGLES::UpdateDownloadTempBuffer(VirtualFramebuffer *nvfb) {
	_assert_msg_(nvfb->fbo, "Expecting a valid nvfb in UpdateDownloadTempBuffer");

	// Discard the previous contents of this buffer where possible.
	if (gl_extensions.GLES3) {
		draw_->BindFramebufferAsRenderTarget(nvfb->fbo, { Draw::RPAction::DONT_CARE, Draw::RPAction::DONT_CARE, Draw::RPAction::DONT_CARE }, "UpdateDownloadTempBuffer");
	} else if (gl_extensions.IsGLES) {
		draw_->BindFramebufferAsRenderTarget(nvfb->fbo, { Draw::RPAction::CLEAR, Draw::RPAction::CLEAR, Draw::RPAction::CLEAR }, "UpdateDownloadTempBuffer");
		gstate_c.Dirty(DIRTY_BLEND_STATE);
	}
}

void FramebufferManagerGLES::DeviceLost() {
	FramebufferManagerCommon::DeviceLost();
	if (depthDownloadProgram_) {
		GLRenderManager *render = (GLRenderManager *)draw_->GetNativeObject(Draw::NativeObject::RENDER_MANAGER);
		render->DeleteProgram(depthDownloadProgram_);
		depthDownloadProgram_ = nullptr;
	}
}

void FramebufferManagerGLES::NotifyDisplayResized() {
	FramebufferManagerCommon::NotifyDisplayResized();

	GLRenderManager *render = (GLRenderManager *)draw_->GetNativeObject(Draw::NativeObject::RENDER_MANAGER);
	render->Resize(PSP_CoreParameter().pixelWidth, PSP_CoreParameter().pixelHeight);
}

bool FramebufferManagerGLES::GetOutputFramebuffer(GPUDebugBuffer &buffer) {
	int w, h;
	draw_->GetFramebufferDimensions(nullptr, &w, &h);
	buffer.Allocate(w, h, GPU_DBG_FORMAT_888_RGB, true);
	draw_->CopyFramebufferToMemorySync(nullptr, Draw::FB_COLOR_BIT, 0, 0, w, h, Draw::DataFormat::R8G8B8_UNORM, buffer.GetData(), w, "GetOutputFramebuffer");
	return true;
}
