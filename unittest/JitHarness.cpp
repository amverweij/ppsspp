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

#include "ppsspp_config.h"

#include "Common/System/NativeApp.h"
#include "Common/System/System.h"
#include "Common/TimeUtil.h"
#include "Core/ConfigValues.h"
#include "Core/Debugger/SymbolMap.h"
#include "Core/MIPS/JitCommon/JitCommon.h"
#include "Core/MIPS/JitCommon/JitBlockCache.h"
#include "Core/MIPS/MIPSCodeUtils.h"
#include "Core/MIPS/MIPSDebugInterface.h"
#include "Core/MIPS/MIPSAsm.h"
#include "Core/MIPS/MIPSTables.h"
#include "Core/MIPS/MIPSVFPUUtils.h"
#include "Core/MemMap.h"
#include "Core/Core.h"
#include "Core/CoreTiming.h"
#include "Core/HLE/HLE.h"

// Temporary hacks around annoying linking errors.  Copied from Headless.
void NativeUpdate() { }
void NativeRender(GraphicsContext *graphicsContext) { }
void NativeResized() { }

void System_SendMessage(const char *command, const char *parameter) {}
void System_InputBoxGetString(const std::string &title, const std::string &defaultValue, std::function<void(bool, const std::string &)> cb) { cb(false, ""); }
void System_AskForPermission(SystemPermission permission) {}
PermissionStatus System_GetPermissionStatus(SystemPermission permission) { return PERMISSION_STATUS_GRANTED; }

void UnitTestTerminator() {
	// Bails out of jit so we can time things.
	coreState = CORE_POWERDOWN;
	hleSkipDeadbeef();
}

HLEFunction UnitTestFakeSyscalls[] = {
	{0x1234BEEF, &UnitTestTerminator, "UnitTestTerminator"},
};

double ExecCPUTest() {
	int blockTicks = 1000000;
	int total = 0;
	double st = time_now_d();
	do {
		for (int j = 0; j < 1000; ++j) {
			currentMIPS->pc = PSP_GetUserMemoryBase();
			coreState = CORE_RUNNING;

			while (coreState == CORE_RUNNING) {
				mipsr4k.RunLoopUntil(blockTicks);
			}
			++total;
		}
	}
	while (time_now_d() - st < 0.5);
	double elapsed = time_now_d() - st;

	return total / elapsed;
}

static void SetupJitHarness() {
	// We register a syscall so we have an easy way to finish the test.
	RegisterModule("UnitTestFakeSyscalls", ARRAY_SIZE(UnitTestFakeSyscalls), UnitTestFakeSyscalls);

	// This is pretty much the bare minimum required to setup jit.
	coreState = CORE_POWERUP;
	currentMIPS = &mipsr4k;
	g_symbolMap = new SymbolMap();
	Memory::g_MemorySize = Memory::RAM_NORMAL_SIZE;
	PSP_CoreParameter().cpuCore = CPUCore::INTERPRETER;
	PSP_CoreParameter().fastForward = true;

	Memory::Init();
	mipsr4k.Reset();
	CoreTiming::Init();
	InitVFPUSinCos();
}

static void DestroyJitHarness() {
	// Clear our custom module out to be safe.
	HLEShutdown();
	CoreTiming::Shutdown();
	mipsr4k.Shutdown();
	Memory::Shutdown();
	coreState = CORE_POWERDOWN;
	currentMIPS = nullptr;
	delete g_symbolMap;
}

bool TestJit() {
	SetupJitHarness();

	currentMIPS->pc = PSP_GetUserMemoryBase();
	u32 *p = (u32 *)Memory::GetPointer(currentMIPS->pc);

	// TODO: Smarter way of seeding in the code sequence.
	static const char *lines[] = {
		//"vcrsp.t C000, C100, C200",
		//"vdot.q C000, C100, C200",
		//"vmmul.q M000, M100, M200",
		"lui r1, 0x8910",
		"vmmul.q M000, M100, M200",
		"sv.q C000, 0(r1)",
		"sv.q C000, 16(r1)",
		"sv.q C000, 32(r1)",
		"sv.q C000, 48(r1)",
		/*
		"abs.s f1, f1",
		"cvt.w.s f1, f1",
		"cvt.w.s f3, f1",
		"cvt.w.s f0, f2",
		"cvt.w.s f5, f1",
		"cvt.w.s f6, f5",
		*/
	};

	bool compileSuccess = true;
	u32 addr = currentMIPS->pc;
	DebugInterface *dbg = currentDebugMIPS;
	for (int i = 0; i < 100; ++i) {
		/*
		// VFPU ops aren't supported by MIPSAsm yet.
		*p++ = 0xD03C0000 | (1 << 7) | (1 << 15) | (7 << 8);
		*p++ = 0xD03C0000 | (1 << 7) | (1 << 15);
		*p++ = 0xD03C0000 | (1 << 7) | (1 << 15) | (7 << 8);
		*p++ = 0xD03C0000 | (1 << 7) | (1 << 15) | (7 << 8);
		*p++ = 0xD03C0000 | (1 << 7) | (1 << 15) | (7 << 8);
		*p++ = 0xD03C0000 | (1 << 7) | (1 << 15) | (7 << 8);
		*p++ = 0xD03C0000 | (1 << 7) | (1 << 15) | (7 << 8);
		*/
		for (size_t j = 0; j < ARRAY_SIZE(lines); ++j) {
			p++;
			if (!MIPSAsm::MipsAssembleOpcode(lines[j], currentDebugMIPS, addr)) {
				printf("ERROR: %s\n", MIPSAsm::GetAssembleError().c_str());
				compileSuccess = false;
			}
			addr += 4;
		}
	}

	*p++ = MIPS_MAKE_SYSCALL("UnitTestFakeSyscalls", "UnitTestTerminator");
	*p++ = MIPS_MAKE_BREAK(1);

	// Dogfood.
	addr = currentMIPS->pc;
	for (size_t j = 0; j < ARRAY_SIZE(lines); ++j) {
		char line[512];
		MIPSDisAsm(Memory::Read_Instruction(addr), addr, line, true);
		addr += 4;
		printf("%s\n", line);
	}

	printf("\n");

	double jit_speed = 0.0, interp_speed = 0.0;
	if (compileSuccess) {
		interp_speed = ExecCPUTest();
		mipsr4k.UpdateCore(CPUCore::JIT);
		jit_speed = ExecCPUTest();

		// Disassemble
		JitBlockCache *cache = MIPSComp::jit->GetBlockCache();
		JitBlock *block = cache->GetBlock(0);  // Should only be one block.
#if PPSSPP_ARCH(ARM)
		std::vector<std::string> lines = DisassembleArm2(block->normalEntry, block->codeSize);
#elif PPSSPP_ARCH(ARM64)
		std::vector<std::string> lines = DisassembleArm64(block->normalEntry, block->codeSize);
#elif PPSSPP_ARCH(X86) || PPSSPP_ARCH(AMD64)
		std::vector<std::string> lines = DisassembleX86(block->normalEntry, block->codeSize);
#else
		std::vector<std::string> lines;
#endif
		// Cut off at 25 due to the repetition above. Might need tweaking for large instructions.
		const int cutoff = 25;
		for (int i = 0; i < std::min((int)lines.size(), cutoff); i++) {
			printf("%s\n", lines[i].c_str());
		}
		if (lines.size() > cutoff)
			printf("...\n");
		printf("Jit was %fx faster than interp.\n\n", jit_speed / interp_speed);
	}

	printf("\n");

	DestroyJitHarness();

	return jit_speed >= interp_speed;
}
