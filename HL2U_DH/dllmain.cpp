// dllmain.cpp : Определяет точку входа для приложения DLL.
#include "pch.h"
#include "convar.hh"

#include <d3d9.h>
#include "imgui/imgui.h"
#include "imgui_impl_dx9/imgui_impl_dx9.h"
#include "vector.hh"

extern IMGUI_API LRESULT ImGui_ImplDX9_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

HRESULT(STDMETHODCALLTYPE* original_present) (IDirect3DDevice9*, const RECT*, const RECT*, HWND, const RGNDATA*);
HRESULT(STDMETHODCALLTYPE* original_reset) (IDirect3DDevice9*, D3DPRESENT_PARAMETERS*);

class CUserCmd
{
public:
	virtual ~CUserCmd() {}; //Destructor 0 - 4
	int command_number; //4 - 8
	int tick_count; //8 - C
	Vector viewangles; //C - 18
	float forwardmove; //24 - 28
	float sidemove; //28 - 2C
	float upmove; //2C - 30
	int	buttons; //30 - 34
	byte impulse; //34 - 35 
	int weaponselect; //35 - 39
	int weaponsubtype; //39 - 3D
	int random_seed; //3D - 41
	short mousedx; //41 - 43
	short mousedy; //43 - 45
	bool hasbeenpredicted; //45 - 46
	Vector headangles; //46 - 52
	Vector headoffset; //52 - 5E
};
class CClientMode;
class {
public:
	int  speedhack = 0;
	bool tauntslide = false;
	bool bhop = false;
	bool bhop_rage = false;
} settings;

using create_interface_fn = uptr(*)(const char*, u32);
using client_mode_fn = CClientMode * (__stdcall*)();

static engine_client* engine = nullptr;
Cvar* cvar = nullptr;
static CClientMode* client_mode = nullptr;
static EntList* ent_list = nullptr;

std::unique_ptr<hooks::HookFunction<CClientMode, 0>> create_move_hook;

void strafer(CUserCmd* user_cmd) {
	auto local = ent_list->entity(engine->local_player_index());
	auto flags = *reinterpret_cast<u32*>((uptr)local + 0x34C);
	auto velocity = *reinterpret_cast<Vector*>((uptr)local + 0xF0);

	if (!GetAsyncKeyState(VK_SPACE) || velocity.Length2D() < 0.5)
		return;

	if (!(flags & 0x1)) {
		//static ConvarWrapper sidespeed_var("cl_sidepseed");
		static auto cl_sidespeed = 450.f; //sidespeed_var.get_float();
		if (fabsf(user_cmd->mousedx) > 2.f) {
			user_cmd->sidemove = (user_cmd->mousedx < 0.f) ? -cl_sidespeed : cl_sidespeed;
			return;
		}
		if (GetAsyncKeyState('S')) {
			user_cmd->viewangles.y -= 180;
		}
		else if (GetAsyncKeyState('D')) {
			user_cmd->viewangles.y += 90;
		}
		else if (GetAsyncKeyState('A')) {
			user_cmd->viewangles.y -= 90;
		}

		if (!(velocity.Length2D() > 0.5) || velocity.Length2D() == NAN || velocity.Length2D() == INFINITE)
		{
			user_cmd->forwardmove = 400;
			return;
		}

		user_cmd->forwardmove = clamp(5850.f / velocity.Length2D(), -400, 400);
		if ((user_cmd->forwardmove < -400 || user_cmd->forwardmove > 400))
			user_cmd->forwardmove = 0;

		const auto vel = velocity;
		const float y_vel = RAD2DEG(atan2(vel.y, vel.x));
		const float diff_ang = normalize_yaw(user_cmd->viewangles.y - y_vel);

		user_cmd->sidemove = (diff_ang > 0.0) ? -cl_sidespeed : cl_sidespeed;
		user_cmd->viewangles.y = normalize_yaw(user_cmd->viewangles.y - diff_ang);
	}
}

bool __fastcall hooked_create_move(void* instance, [[maybe_unused]] void* edx, float sample_framerate, CUserCmd* user_cmd) {
	auto ret = create_move_hook->call_original<bool>(sample_framerate, user_cmd);

	auto local = ent_list->entity(engine->local_player_index());
	auto flags = *reinterpret_cast<u32*>((uptr)local + 0x34C);

	uptr      ebp_address;
	__asm mov ebp_address, ebp;
	//reinterpret_cast<bool*>(***(uptr * **)ebp_address - 1);
	auto arg0_ptr = reinterpret_cast<float*>(***(uptr * **)ebp_address + 0x8);
	auto arg4_ptr = reinterpret_cast<bool*>(***(uptr * **)ebp_address + 0xC);

	static bool in_speedhack = false;
	if(settings.speedhack && GetAsyncKeyState(VK_TAB) && !in_speedhack) {
		in_speedhack = true;
		using CL_Move = void(*)(float accumulated_extra_samples, bool bFinalTick);
		static auto move = reinterpret_cast<CL_Move>(signature::find_pattern("engine", "55 8B EC 83 EC 34 83 3D"));
		for (u32 i = 0; i < settings.speedhack; i++)
			move(*arg0_ptr, *arg4_ptr);
		in_speedhack = false;
	}
	
	/*if (GetAsyncKeyState(VK_SHIFT))
		user_cmd->tick_count = 16777216;*/

	if(settings.bhop_rage) {
		strafer(user_cmd);
		ret = false;
	}
	if(settings.bhop || settings.bhop_rage) {
		if (user_cmd->buttons & IN_JUMP) {
			user_cmd->buttons &= ~IN_JUMP;
			if (flags & 0x1)
			{
				user_cmd->buttons |= IN_JUMP;
			}
		}
	}

	if(GetAsyncKeyState(VK_NUMPAD0) < 0) { // AirStuck with ViewAngles reset (((Teleport to 0, 0, 0))) (Reversed from some L4D2 hack)
		user_cmd->viewangles.x = 3.4028235e38;
		user_cmd->viewangles.y = 3.4028235e38;
		user_cmd->viewangles.z = 3.4028235e38;
		user_cmd->upmove = 3.4028235e38;
		user_cmd->forwardmove = 3.4028235e38;
		user_cmd->sidemove = 3.4028235e38;
	}

	return ret;
}

// main
HWND game_hwnd = NULL;

// original
WNDPROC game_wndproc = NULL;

BOOL CALLBACK find_game_hwnd(HWND hwnd, LPARAM game_pid) {
	DWORD hwnd_pid = NULL;

	GetWindowThreadProcessId(hwnd, &hwnd_pid);

	if (hwnd_pid != game_pid)
		return TRUE;

	game_hwnd = hwnd;

	return FALSE;
}

LRESULT STDMETHODCALLTYPE user_wndproc(HWND window, UINT message_type, WPARAM w_param, LPARAM l_param) {
	ImGui_ImplDX9_WndProcHandler(window, message_type, w_param, l_param);

	return CallWindowProcA(game_wndproc, window, message_type, w_param, l_param);
}

HRESULT STDMETHODCALLTYPE present(IDirect3DDevice9* thisptr, const RECT* src, const RECT* dest, HWND wnd_override, const RGNDATA* dirty_region) {
	static bool init = false;

	if(!init) {
		EnumWindows(find_game_hwnd, GetCurrentProcessId());

		if (game_hwnd != NULL) {
			game_wndproc = reinterpret_cast<WNDPROC>(
				SetWindowLongPtr(game_hwnd, GWLP_WNDPROC, LONG_PTR(user_wndproc))
				);

			ImGui_ImplDX9_Init(game_hwnd, thisptr);

			init = true;
		}
	} else {
		ImGui_ImplDX9_NewFrame();

		//---
		static bool menu = false;
		if (GetAsyncKeyState(VK_INSERT) & 0x1)
			menu = !menu;
		if (menu) {
			ImGui::Begin("HL2U DH");
			ImGui::SliderInt("SpeedHack", &settings.speedhack, 0, 25);
			//ImGui::Checkbox("TauntSlide", &settings.tauntslide);
			ImGui::Checkbox("BHop", &settings.bhop);
			ImGui::Checkbox("BHop Rage", &settings.bhop_rage);
			ImGui::End();
		}
		//---
		/*if(engine->in_game())
		{
			auto local = ent_list->entity(engine->local_player_index());
			auto taunt = reinterpret_cast<u8*>((uptr)local + 0x2388);
			if (settings.tauntslide)
				*taunt = 0;
				//*taunt &= ~0b1111;
		}*/
		//---

		ImGui::Render();
	}

	return original_present(thisptr, src, dest, wnd_override, dirty_region);
}

HRESULT STDMETHODCALLTYPE reset(IDirect3DDevice9* thisptr, D3DPRESENT_PARAMETERS* params) {
	ImGui_ImplDX9_InvalidateDeviceObjects();
	ImGui_ImplDX9_CreateDeviceObjects();

	return original_reset(thisptr, params);
}

DWORD WINAPI main_thread(LPVOID lpArguments) {
	static auto create_interface_engine = (create_interface_fn)signature::resolve_import(signature::resolve_library("engine"), "CreateInterface");
	//static auto create_interface_vstdlib = (create_interface_fn)signature::resolve_import(signature::resolve_library("vstdlib"), "CreateInterface");
	static auto create_interface_client = (create_interface_fn)signature::resolve_import(signature::resolve_library("client"), "CreateInterface");

	engine = (engine_client*)create_interface_engine("VEngineClient014", NULL);
	//cvar = (Cvar*)create_interface_vstdlib("VEngineCvar007", NULL);
	ent_list = (EntList*)create_interface_client("VClientEntityList003", NULL);

	static auto clientmode_ptr = signature::find_pattern("client", "EB 1A D9 87");
	client_mode = **reinterpret_cast<CClientMode***>(clientmode_ptr + 2 + 0x1a + 2);
	create_move_hook = std::make_unique<hooks::HookFunction<CClientMode, 0>>(client_mode, 21, reinterpret_cast<void*>(&hooked_create_move));

	auto present_addr = signature::find_pattern("gameoverlayrenderer", "FF 15 ? ? ? ? 8B F8 85 DB") + 2;
	auto reset_addr = signature::find_pattern("gameoverlayrenderer", "C7 45 ? ? ? ? ? FF 15 ? ? ? ? 8B F8") + 9;

	original_present = **reinterpret_cast<decltype(&original_present)*>(present_addr);
	original_reset = **reinterpret_cast<decltype(&original_reset)*>(reset_addr);

	**reinterpret_cast<void***>(present_addr) = reinterpret_cast<void*>(&present);
	**reinterpret_cast<void***>(reset_addr) = reinterpret_cast<void*>(&reset);

	return 0;
}

BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
                     )
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
		CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)main_thread, NULL, 0, NULL);
		break;
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}

