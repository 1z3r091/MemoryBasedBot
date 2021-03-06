// for removing errors
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS

#include "stdafx.h"
#include "ASM.h"
#include "bytebuffer.h"
#include "Client.h"
#include "Hooks.h"
#include "Macro.h"
#include "lua.hpp"

//#pragma comment(lib, "ws2_32.lib")

#define DEFAULT_BUFLEN 2000
#define SIZE 6

// need for hooking connect function
typedef int (WINAPI *pConnect)(SOCKET, const struct sockaddr*, int);
int WINAPI MyConnect(SOCKET, const struct sockaddr*, int);
void BeginRedirect(LPVOID newFunction);

pConnect pOrigMBAddress = NULL;
BYTE oldBytes[SIZE] = { 0 };
BYTE JMP[SIZE] = { 0 };
DWORD oldProtect, myProtect = PAGE_EXECUTE_READWRITE;
HMODULE g_hMod = NULL;

// create console in dll
void CreateConsole()
{
	FILE *acStreamOut;
	FILE *acStreamIn;
	
	// Allocate a console
	AllocConsole();

	// enable console output
	SetConsoleTitleA("testing...");
	freopen_s(&acStreamOut, "CONOUT$", "w", stdout);
	freopen_s(&acStreamIn, "CONIN$", "r", stdin);
	freopen_s(&acStreamIn, "CONIN$", "r", stderr);
}



void init(void)
{
	CreateConsole();
	Client* client = new Client();

	// send packet hook
	unsigned char Send_Packet_Bytes[] = { 0x8B, 0x55, 0x10, 0x8B, 0x7D, 0x0C, 0x52, 0x57 };
	int Send_Packet_Address = FindPattern(0x004A4E00, 0x004A54B0, Send_Packet_Bytes, 8);
	if (!Send_Packet_Address) { exit(0);  return; };
	ByteBuffer Buffer((LPVOID)(Send_Packet_Address + 9), 4);
	Properties::NetworkClass = Buffer.ReadUint32(0);
	Properties::Send_Packet_Original_Address = Extract_Address(Send_Packet_Address + 10, Send_Packet_Address + 11);

	PatchAddress(0x004A5008, client->hooks->Send_Packet_Hook, 0);

	// recv packet hook
	unsigned char Recv_Packet_Bytes[] = { 0x56, 0x57, 0x50 };//{ 0x8D, 0xB3, 0x08, 0x0E, 0x03, 0x00, 0x8B, 0xCB, 0x56, 0x57 }; //0x50, 0xE8, 0x32, 0x06, 0x00, 0x00};
	int Recv_Packet_Address = FindPattern(0x004A79A8, 0x004A7AE5, Recv_Packet_Bytes, 3);
	if (!Recv_Packet_Address) { exit(0); return; };
	Properties::Recv_Packet_Original_Address = Extract_Address(Recv_Packet_Address + 3, Recv_Packet_Address + 4);

	PatchAddress(0x004A7A89, client->hooks->Recv_Packet_Hook, 0);

	/*
	// connect hook version 1
	unsigned char Con_Packet_Bytes[] = { 0x6A, 0x10, 0x52, 0x50, 0xFF, 0x15 };
	int Con_Packet_Address = FindPattern(0x004A5BD8, 0x004A5C79, Con_Packet_Bytes, 6);
	if (!Con_Packet_Address) { exit(0); return FALSE; };
	printf("---- Con_Packet_Address: %x\n", Con_Packet_Address);
	//Properties::Con_Packet_Original_Address = Extract_Address(Con_Packet_Address + 4, Con_Packet_Address + 5);

	// get 4 bytes from the address 0x0052955C to integer
	ByteBuffer Packet((LPVOID)0x0052955C, 4);
	std::vector<uint8_t> data = Packet.ReadBytes(0, 4);
	unsigned char copied[4] = { "0", };
	int x;
	for (int i = 3; i >= 0; i--) {
	copied[3 - i] = (char)data[3 - i];
	}
	std::memcpy(&x, copied, 4);

	Properties::Con_Packet_Original_Address = x;
	printf("---- Con_Packet_Original_Address: %x\n", Properties::Con_Packet_Original_Address);

	PatchAddress(0x004A5C05, client->hooks->Con_Packet_Hook, 0);
	*/

	// connect hook version 2
	pOrigMBAddress = (pConnect)GetProcAddress(GetModuleHandle(L"WS2_32.dll"), "connect");
	if (pOrigMBAddress != NULL)
		BeginRedirect(MyConnect);
	else
		printf("NULL\n");

	// register lua functions
	Macro::L = luaL_newstate();
	luaL_openlibs(Macro::L);
	lua_register(Macro::L, "pri", (lua_CFunction)Macro::pri);
	lua_register(Macro::L, "pc_x", (lua_CFunction)Macro::pc_x);
	lua_register(Macro::L, "pc_y", (lua_CFunction)Macro::pc_y);
	lua_register(Macro::L, "keypress", (lua_CFunction)Macro::keypress);
	lua_register(Macro::L, "attack", (lua_CFunction)Macro::attack);
	lua_register(Macro::L, "mapname", (lua_CFunction)Macro::mapname);
	lua_register(Macro::L, "mapmove", (lua_CFunction)Macro::cardinal_direction);
	lua_register(Macro::L, "refresh", (lua_CFunction)Macro::refresh);
	lua_register(Macro::L, "hp_charge", (lua_CFunction)Macro::hp_charge);
	lua_register(Macro::L, "mp_charge", (lua_CFunction)Macro::mp_charge);
	lua_register(Macro::L, "return_to_inn", (lua_CFunction)Macro::returnToInn);
	lua_register(Macro::L, "sleep", (lua_CFunction)Macro::sleep);
	lua_register(Macro::L, "showtext", (lua_CFunction)Macro::showtext);
	lua_register(Macro::L, "eat", (lua_CFunction)Macro::eat);
	lua_register(Macro::L, "add_command", (lua_CFunction)Macro::add_command);
	lua_register(Macro::L, "set_interval", (lua_CFunction)Macro::setInterval);
	lua_register(Macro::L, "clear_interval", (lua_CFunction)Macro::clearInterval);
	lua_register(Macro::L, "debug", (lua_CFunction)Macro::debug);
	lua_register(Macro::L, "item_xy", (lua_CFunction)Macro::item_xy);
	lua_register(Macro::L, "pc_hp", (lua_CFunction)Macro::pc_hp);
	lua_register(Macro::L, "pc_mp", (lua_CFunction)Macro::pc_mp);
	lua_register(Macro::L, "pc_exp", (lua_CFunction)Macro::pc_exp);
	lua_register(Macro::L, "sell_exp", (lua_CFunction)Macro::sell_exp);
	lua_register(Macro::L, "bonus_exp_1", (lua_CFunction)Macro::bonus_exp_1);
	lua_register(Macro::L, "bonus_exp_2", (lua_CFunction)Macro::bonus_exp_2);
	lua_register(Macro::L, "bonus_exp_on", (lua_CFunction)Macro::bonus_exp_on);
	lua_register(Macro::L, "get_item_amnt", (lua_CFunction)Macro::get_item_amnt);
	lua_register(Macro::L, "set_item_amnt", (lua_CFunction)Macro::set_item_amnt);
	lua_register(Macro::L, "say", (lua_CFunction)Macro::say);

	printf("starting...\n");

}


BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
	HANDLE hThread = NULL;

	g_hMod = (HMODULE)hinstDLL;

	switch (fdwReason)
	{
	case DLL_PROCESS_ATTACH:
		CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)init, NULL, 0, NULL);
		break;
	}

	return TRUE;
}

void BeginRedirect(LPVOID newFunction)
{
	BYTE tempJMP[SIZE] = { 0xE9, 0x90, 0x90, 0x90, 0x90, 0xC3 };
	memcpy(JMP, tempJMP, SIZE);
	DWORD JMPSize = ((DWORD)newFunction - (DWORD)pOrigMBAddress - 5);
	VirtualProtect((LPVOID)pOrigMBAddress, SIZE,
		PAGE_EXECUTE_READWRITE, &oldProtect);
	memcpy(oldBytes, pOrigMBAddress, SIZE);
	memcpy(&JMP[1], &JMPSize, 4);
	memcpy(pOrigMBAddress, JMP, SIZE);
	VirtualProtect((LPVOID)pOrigMBAddress, SIZE, oldProtect, NULL);
}

int  WINAPI MyConnect(SOCKET sock, const struct sockaddr *addr, int length)
{
	int strLen = 0;
	char nameMsg[DEFAULT_BUFLEN];

	VirtualProtect((LPVOID)pOrigMBAddress, SIZE, myProtect, NULL);
	memcpy(pOrigMBAddress, oldBytes, SIZE);

	int retValue = connect(sock, addr, length);
	Hooks::Con_Packet_Socket = sock;

	memcpy(pOrigMBAddress, JMP, SIZE);
	VirtualProtect((LPVOID)pOrigMBAddress, SIZE, oldProtect, NULL);

	return retValue;
}


