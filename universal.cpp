#include <Windows.h>
#include <vector>
#include <d3d11.h>
//#include <d3dx11.h>
#pragma comment(lib, "d3d11.lib")
//#pragma comment(lib, "D3DX11.lib")

#include "MinHook/include/MinHook.h" //detour x86&x64
//add all minhook files to your project
#include "FW1FontWrapper/FW1FontWrapper.h" //font
//add all fontwrapper files to your project

typedef HRESULT(__stdcall *D3D11PresentHook) (IDXGISwapChain* pSwapChain, UINT SyncInterval, UINT Flags);
typedef void (__stdcall *D3D11DrawIndexedHook) (ID3D11DeviceContext* pContext, UINT IndexCount, UINT StartIndexLocation, INT BaseVertexLocation);
typedef void(__stdcall *D3D11MapHook) (ID3D11DeviceContext* pContext, ID3D11Resource *pResource, UINT Subresource, D3D11_MAP MapType, UINT MapFlags, D3D11_MAPPED_SUBRESOURCE *pMappedResource);
typedef void(__stdcall *D3D11VSSetConstantBuffersHook) (ID3D11DeviceContext* pContext, UINT StartSlot, UINT NumBuffers, ID3D11Buffer *const *ppConstantBuffers);


D3D11PresentHook phookD3D11Present = NULL;
D3D11DrawIndexedHook phookD3D11DrawIndexed = NULL;
D3D11MapHook phookD3D11Map = NULL;
D3D11VSSetConstantBuffersHook phookD3D11VSSetConstantBuffers = NULL;

ID3D11Device *pDevice = NULL;
ID3D11DeviceContext *pContext = NULL;

DWORD_PTR* pSwapChainVtable = NULL;
DWORD_PTR* pContextVTable = NULL;
DWORD_PTR* pDeviceVTable = NULL;

IFW1Factory *pFW1Factory = NULL;
IFW1FontWrapper *pFontWrapper = NULL;

//==========================================================================================================================

//init only once
bool firstTime = true;

//stride
ID3D11Buffer *vBuffer;
UINT Stride = 0;
UINT vBufferOffset = 0;

//wallhack
ID3D11DepthStencilState *depthStencilState;
ID3D11DepthStencilState *depthStencilStateDisable;

//rendertarget
ID3D11Texture2D* pRenderTargetTexture;
ID3D11RenderTargetView* g_pRenderTargetView = NULL;

//shader
ID3D11PixelShader* RedShader = NULL;
ID3D11PixelShader* GreenShader = NULL;

//used for logging/cycling through values
bool logger = false;
int countnum = -1;
char szString[64];

//ret addr
#pragma intrinsic(_ReturnAddress)
bool								g_Init = false;
int									g_Index = -1;
std::vector<void*>					g_Vector;
void*								g_SelectedAddress = NULL;

//==========================================================================================================================
//get dir
using namespace std;
#include <fstream>
char dlldir[320];
char *GetDirectoryFile(char *filename)
{
	static char path[320];
	strcpy_s(path, dlldir);
	strcat_s(path, filename);
	return path;
}

//log
void Log(const char *fmt, ...)
{
	if (!fmt)	return;

	char		text[4096];
	va_list		ap;
	va_start(ap, fmt);
	vsprintf_s(text, fmt, ap);
	va_end(ap);

	ofstream logfile(GetDirectoryFile("log.txt"), ios::app);
	if (logfile.is_open() && text)	logfile << text << endl;
	logfile.close();
}

//retaddr
bool IsAddressPresent(void* Address)
{
	for (auto it = g_Vector.begin(); it != g_Vector.end(); ++it)
	{
		if (*it == Address)
			return true;
	}
	return false;
}

//==========================================================================================================================

HRESULT __stdcall hookD3D11Present(IDXGISwapChain* pSwapChain, UINT SyncInterval, UINT Flags)
{
	if (firstTime)
	{
		//get device and context
		if (SUCCEEDED(pSwapChain->GetDevice(__uuidof(ID3D11Device), (void **)&pDevice)))
		{
			pSwapChain->GetDevice(__uuidof(pDevice), (void**)&pDevice);
			pDevice->GetImmediateContext(&pContext);
		}

		//create depthstencilstate
		D3D11_DEPTH_STENCIL_DESC depthStencilDesc;
		ZeroMemory(&depthStencilDesc, sizeof(depthStencilDesc));
		depthStencilDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
		depthStencilDesc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
		depthStencilDesc.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
		depthStencilDesc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_INCR;
		depthStencilDesc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
		depthStencilDesc.FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
		depthStencilDesc.BackFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
		depthStencilDesc.BackFace.StencilDepthFailOp = D3D11_STENCIL_OP_DECR;
		depthStencilDesc.BackFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
		depthStencilDesc.BackFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
		depthStencilDesc.DepthEnable = true; //<- method 1
		//depthStencilDesc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL; //<-method 2
		depthStencilDesc.StencilEnable = true;
		depthStencilDesc.StencilReadMask = 0xFF;
		depthStencilDesc.StencilWriteMask = 0xFF;
		pDevice->CreateDepthStencilState(&depthStencilDesc, &depthStencilState);

		//create disabled depthstencilstate
		D3D11_DEPTH_STENCIL_DESC depthDisabledStencilDesc;
		ZeroMemory(&depthDisabledStencilDesc, sizeof(depthDisabledStencilDesc));
		depthDisabledStencilDesc.DepthEnable = false; //<- method 1
		//depthStencilDesc.DepthFunc = D3D11_COMPARISON_ALWAYS; //<-method 2
		depthDisabledStencilDesc.StencilEnable = false;
		pDevice->CreateDepthStencilState(&depthDisabledStencilDesc, &depthStencilStateDisable);

		//create font
		HRESULT hResult = FW1CreateFactory(FW1_VERSION, &pFW1Factory);
		hResult = pFW1Factory->CreateFontWrapper(pDevice, L"Tahoma", &pFontWrapper);
		pFW1Factory->Release();

		// use the back buffer address to create the render target
		if (SUCCEEDED(pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&pRenderTargetTexture)))
		{
			pDevice->CreateRenderTargetView(pRenderTargetTexture, NULL, &g_pRenderTargetView);
			pRenderTargetTexture->Release();
		}

		firstTime = false;
	}

	//call before you draw
	pContext->OMSetRenderTargets(1, &g_pRenderTargetView, NULL);
	//draw
	pFontWrapper->DrawString(pContext, L"D3D11 Hook", 30, 16.0f, 16.0f, 0xffff1612, FW1_RESTORESTATE);

	//logger
	if ((GetAsyncKeyState(VK_MENU)) && (GetAsyncKeyState(VK_CONTROL)) && (GetAsyncKeyState(0x4C) & 1)) //ALT + CTRL + L toggles logger
		logger = !logger;
	if (logger) //&& countnum >= 0)
	{
		wchar_t reportValue[256];
		swprintf_s(reportValue, L"countnum = %d", countnum);
		pFontWrapper->DrawString(pContext, reportValue, 20.0f, 220.0f, 100.0f, 0xffffffff, FW1_RESTORESTATE);
		pFontWrapper->DrawString(pContext, L"hold P to +", 20.0f, 220.0f, 120.0f, 0xfff11111, FW1_RESTORESTATE);
		pFontWrapper->DrawString(pContext, L"hold O to -", 20.0f, 220.0f, 140.0f, 0xfaf22222, FW1_RESTORESTATE);
		pFontWrapper->DrawString(pContext, L"press I to log", 20.0f, 220.0f, 160.0f, 0xfff99999, FW1_RESTORESTATE);
	}

	/*
	//retaddr finder
	if (GetAsyncKeyState(VK_RIGHT) & 1)
	{
		if (g_Index != g_Vector.size() - 1)
		{
			g_Index++;
			g_SelectedAddress = g_Vector[g_Index];
		}
	}
	else if (GetAsyncKeyState(VK_LEFT) & 1)
	{
		if (g_Index >= 0)
		{
			g_Index--;
			g_SelectedAddress = g_Vector[g_Index];
			if (g_Index == -1)
				g_SelectedAddress = NULL;
		}
	}

	wchar_t reportVectorSize[256];
	swprintf_s(reportVectorSize, L"Vector size: %d", g_Vector.size());
	wchar_t reportSelectedIndex[256];
	swprintf_s(reportSelectedIndex, L"Selected Index: %d", g_Index);
	wchar_t reportSelectedAddress[256];
	swprintf_s(reportSelectedAddress, L"Selected Address: 0x%X", g_SelectedAddress);
	pFontWrapper->DrawString(pContext, reportVectorSize, 20.0f, 20.0f, 100.0f, 0xffffffff, FW1_RESTORESTATE);
	pFontWrapper->DrawString(pContext, reportSelectedIndex, 20.0f, 20.0f, 120.0f, 0xffffffff, FW1_RESTORESTATE);
	pFontWrapper->DrawString(pContext, reportSelectedAddress, 20.0f, 20.0f, 140.0f, 0xffffffff, FW1_RESTORESTATE);
	pFontWrapper->DrawString(pContext, L"press VK_RIGHT to +", 20.0f, 20.0f, 160.0f, 0xfff11111, FW1_RESTORESTATE);
	pFontWrapper->DrawString(pContext, L"press VK_LEFT to -", 20.0f, 20.0f, 180.0f, 0xfaf22222, FW1_RESTORESTATE);
	*/

	return phookD3D11Present(pSwapChain, SyncInterval, Flags);
}

//==========================================================================================================================

void __stdcall hookD3D11DrawIndexed(ID3D11DeviceContext* pContext, UINT IndexCount, UINT StartIndexLocation, INT BaseVertexLocation)
{
	//get stride
	pContext->IAGetVertexBuffers(0, 1, &vBuffer, &Stride, &vBufferOffset);
	if (vBuffer != NULL){ vBuffer->Release(); vBuffer = NULL; }
	
	//wallhack example
	if(Stride == 32) //&& IndexCount == 7821) //models
	{
		pContext->OMSetDepthStencilState(depthStencilStateDisable, 1); //wallhack on behind walls
		phookD3D11DrawIndexed(pContext, IndexCount, StartIndexLocation, BaseVertexLocation);
		pContext->OMSetDepthStencilState(depthStencilState, 1); //wallhack off
	}
	
	//retaddr example
	void* ReturnAddress = _ReturnAddress();
	if (!IsAddressPresent(ReturnAddress))
		g_Vector.push_back(ReturnAddress);
	/*
	if (ReturnAddress != NULL && g_SelectedAddress != NULL && ReturnAddress == g_SelectedAddress)
	////if((DWORD)ReturnAddress == 0x7cf37a)
	{
		pContext->OMSetDepthStencilState(depthStencilStateDisable, 1); //wallhack on behind walls
		phookD3D11DrawIndexed(pContext, IndexCount, StartIndexLocation, BaseVertexLocation);
		pContext->OMSetDepthStencilState(depthStencilState, 1); //wallhack off
	}
	*/
	
	//small bruteforce logger
	//ALT + CTRL + L toggles logger
	if (logger)
	{
		//hold down P key until a texture is wallhacked, press I to log values of those textures
		if (GetAsyncKeyState('O') & 1) //-
			countnum--;
		if (GetAsyncKeyState('P') & 1) //+
			countnum++;
		if ((GetAsyncKeyState(VK_MENU)) && (GetAsyncKeyState('9') & 1)) //reset, set to -1
			countnum = -1;
		if (countnum == Stride)
			if (GetAsyncKeyState('I') & 1)
				Log("Stride == %d && IndexCount == %d && StartIndexLocation == %d && BaseVertexLocation == %d && (DWORD)ReturnAddress == %x", Stride, IndexCount, StartIndexLocation, BaseVertexLocation, (DWORD)ReturnAddress);
		
		if (countnum == Stride)
		{
			pContext->OMSetDepthStencilState(depthStencilStateDisable, 1);
			phookD3D11DrawIndexed(pContext, IndexCount, StartIndexLocation, BaseVertexLocation);
			pContext->OMSetDepthStencilState(depthStencilState, 1);
		}
	}

    return phookD3D11DrawIndexed(pContext, IndexCount, StartIndexLocation, BaseVertexLocation);
}

//==========================================================================================================================

void __stdcall hookD3D11Map(ID3D11DeviceContext* pContext, ID3D11Resource *pResource, UINT Subresource, D3D11_MAP MapType, UINT MapFlags, D3D11_MAPPED_SUBRESOURCE *pMappedResource)
{
	return phookD3D11Map(pContext, pResource, Subresource, MapType, MapFlags, pMappedResource);
}

//==========================================================================================================================

void __stdcall hookD3D11VSSetConstantBuffers(ID3D11DeviceContext* pContext, UINT StartSlot, UINT NumBuffers, ID3D11Buffer *const *ppConstantBuffers)
{
	return phookD3D11VSSetConstantBuffers(pContext, StartSlot, NumBuffers, ppConstantBuffers);
}

//==========================================================================================================================

const int MultisampleCount = 1; // Set to 1 to disable multisampling
LRESULT CALLBACK DXGIMsgProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam){ return DefWindowProc(hwnd, uMsg, wParam, lParam); }
DWORD __stdcall InitializeHook(LPVOID)
{
	HMODULE hD3D11DLL = 0;
	do
	{
		hD3D11DLL = GetModuleHandle("d3d11.dll");
		Sleep(100);
	} while (!hD3D11DLL);
	Sleep(1000);

    IDXGISwapChain* pSwapChain;

	WNDCLASSEXA wc = { sizeof(WNDCLASSEX), CS_CLASSDC, DXGIMsgProc, 0L, 0L, GetModuleHandleA(NULL), NULL, NULL, NULL, NULL, "DX", NULL };
	RegisterClassExA(&wc);
	HWND hWnd = CreateWindowA("DX", NULL, WS_OVERLAPPEDWINDOW, 100, 100, 300, 300, NULL, NULL, wc.hInstance, NULL);

	D3D_FEATURE_LEVEL requestedLevels[] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_1 };
	D3D_FEATURE_LEVEL obtainedLevel;
	ID3D11Device* d3dDevice = nullptr;
	ID3D11DeviceContext* d3dContext = nullptr;

	DXGI_SWAP_CHAIN_DESC scd;
	ZeroMemory(&scd, sizeof(scd));
	scd.BufferCount = 1;
	scd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	scd.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
	scd.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
	scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;

	scd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
	scd.OutputWindow = hWnd;
	scd.SampleDesc.Count = MultisampleCount;
	scd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
	scd.Windowed = ((GetWindowLongPtr(hWnd, GWL_STYLE) & WS_POPUP) != 0) ? false : true;

	// LibOVR 0.4.3 requires that the width and height for the backbuffer is set even if
	// you use windowed mode, despite being optional according to the D3D11 documentation.
	scd.BufferDesc.Width = 1;
	scd.BufferDesc.Height = 1;
	scd.BufferDesc.RefreshRate.Numerator = 0;
	scd.BufferDesc.RefreshRate.Denominator = 1;

	UINT createFlags = 0;
#ifdef _DEBUG
	// This flag gives you some quite wonderful debug text. Not wonderful for performance, though!
	createFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

	IDXGISwapChain* d3dSwapChain = 0;

	if (FAILED(D3D11CreateDeviceAndSwapChain(
		nullptr,
		D3D_DRIVER_TYPE_HARDWARE,
		nullptr,
		createFlags,
		requestedLevels,
		sizeof(requestedLevels) / sizeof(D3D_FEATURE_LEVEL),
		D3D11_SDK_VERSION,
		&scd,
		&pSwapChain,
		&pDevice,
		&obtainedLevel,
		&pContext)))
	{
		MessageBox(hWnd, "Failed to create directX device and swapchain!", "Error", MB_ICONERROR);
		return NULL;
	}


    pSwapChainVtable = (DWORD_PTR*)pSwapChain;
    pSwapChainVtable = (DWORD_PTR*)pSwapChainVtable[0];

    pContextVTable = (DWORD_PTR*)pContext;
    pContextVTable = (DWORD_PTR*)pContextVTable[0];

	pDeviceVTable = (DWORD_PTR*)pDevice;
	pDeviceVTable = (DWORD_PTR*)pDeviceVTable[0];

	if (MH_Initialize() != MH_OK) { return 1; }
	if (MH_CreateHook((DWORD_PTR*)pSwapChainVtable[8], hookD3D11Present, reinterpret_cast<void**>(&phookD3D11Present)) != MH_OK) { return 1; }
	if (MH_EnableHook((DWORD_PTR*)pSwapChainVtable[8]) != MH_OK) { return 1; }
	if (MH_CreateHook((DWORD_PTR*)pContextVTable[12], hookD3D11DrawIndexed, reinterpret_cast<void**>(&phookD3D11DrawIndexed)) != MH_OK) { return 1; }
	if (MH_EnableHook((DWORD_PTR*)pContextVTable[12]) != MH_OK) { return 1; }
	//if (MH_CreateHook((DWORD_PTR*)pContextVTable[14], hookD3D11Map, reinterpret_cast<void**>(&phookD3D11Map)) != MH_OK) { return 1; }
	//if (MH_EnableHook((DWORD_PTR*)pContextVTable[14]) != MH_OK) { return 1; }
	//if (MH_CreateHook((DWORD_PTR*)pContextVTable[7], hookD3D11VSSetConstantBuffers, reinterpret_cast<void**>(&phookD3D11VSSetConstantBuffers)) != MH_OK) { return 1; }
	//if (MH_EnableHook((DWORD_PTR*)pContextVTable[7]) != MH_OK) { return 1; }

    DWORD dwOld;
    VirtualProtect(phookD3D11Present, 2, PAGE_EXECUTE_READWRITE, &dwOld);

	while (true) {
		Sleep(10);
	}

	pDevice->Release();
	pContext->Release();
	pSwapChain->Release();

    return NULL;
}

//==========================================================================================================================

BOOL __stdcall DllMain(HINSTANCE hModule, DWORD dwReason, LPVOID lpReserved)
{ 
	switch (dwReason)
	{
	case DLL_PROCESS_ATTACH: // A process is loading the DLL.
		DisableThreadLibraryCalls(hModule);
		GetModuleFileName(hModule, dlldir, 512);
		for (int i = strlen(dlldir); i > 0; i--) { if (dlldir[i] == '\\') { dlldir[i + 1] = 0; break; } }
		CreateThread(NULL, 0, InitializeHook, NULL, 0, NULL);
		break;

	case DLL_PROCESS_DETACH: // A process unloads the DLL.
		if (MH_Uninitialize() != MH_OK) { return 1; }
		if (MH_DisableHook((DWORD_PTR*)pSwapChainVtable[8]) != MH_OK) { return 1; }
		if (MH_DisableHook((DWORD_PTR*)pContextVTable[12]) != MH_OK) { return 1; }
		//if (MH_DisableHook((DWORD_PTR*)pContextVTable[14]) != MH_OK) { return 1; }
		//if (MH_DisableHook((DWORD_PTR*)pContextVTable[7]) != MH_OK) { return 1; }
		break;
	}
	return TRUE;
}

/*
dx11 vtable CONTEXT index
Index: 0 | QueryInterface
Index: 1 | Addref
Index: 2 | Release
Index: 3 | GetDevice
Index: 4 | GetPrivateData
Index: 5 | SetPrivateData
Index: 6 | SetPrivateDataInterface
Index: 7 | VSSetConstantBuffers
Index: 8 | PSSetShaderResources
Index: 9 | PSSetShader
Index: 10 | SetSamplers
Index: 11 | SetShader
Index: 12 | DrawIndexed
Index: 13 | Draw
Index: 14 | Map
Index: 15 | Unmap
Index: 16 | PSSetConstantBuffer
Index: 17 | IASetInputLayout
Index: 18 | IASetVertexBuffers
Index: 19 | IASetIndexBuffer
Index: 20 | DrawIndexedInstanced
Index: 21 | DrawInstanced
Index: 22 | GSSetConstantBuffers
Index: 23 | GSSetShader
Index: 24 | IASetPrimitiveTopology
Index: 25 | VSSetShaderResources
Index: 26 | VSSetSamplers
Index: 27 | Begin
Index: 28 | End
Index: 29 | GetData
Index: 30 | GSSetPredication
Index: 31 | GSSetShaderResources
Index: 32 | GSSetSamplers
Index: 33 | OMSetRenderTargets
Index: 34 | OMSetRenderTargetsAndUnorderedAccessViews
Index: 35 | OMSetBlendState
Index: 36 | OMSetDepthStencilState
Index: 37 | SOSetTargets
Index: 38 | DrawAuto
Index: 39 | DrawIndexedInstancedIndirect
Index: 40 | DrawInstancedIndirect
Index: 41 | Dispatch
Index: 42 | DispatchIndirect
Index: 43 | RSSetState
Index: 44 | RSSetViewports
Index: 45 | RSSetScissorRects
Index: 46 | CopySubresourceRegion
Index: 47 | CopyResource
Index: 48 | UpdateSubresource
Index: 49 | CopyStructureCount
Index: 50 | ClearRenderTargetView
Index: 51 | ClearUnorderedAccessViewUint
Index: 52 | ClearUnorderedAccessViewFloat
Index: 53 | ClearDepthStencilView
Index: 54 | GenerateMips
Index: 55 | SetResourceMinLOD
Index: 56 | GetResourceMinLOD
Index: 57 | ResolveSubresource
Index: 58 | ExecuteCommandList
Index: 59 | HSSetShaderResources
Index: 60 | HSSetShader
Index: 61 | HSSetSamplers
Index: 62 | HSSetConstantBuffers
Index: 63 | DSSetShaderResources
Index: 64 | DSSetShader
Index: 65 | DSSetSamplers
Index: 66 | DSSetConstantBuffers
Index: 67 | DSSetShaderResources
Index: 68 | CSSetUnorderedAccessViews
Index: 69 | CSSetShader
Index: 70 | CSSetSamplers
Index: 71 | CSSetConstantBuffers
Index: 72 | VSGetConstantBuffers
Index: 73 | PSGetShaderResources
Index: 74 | PSGetShader
Index: 75 | PSGetSamplers
Index: 76 | VSGetShader
Index: 77 | PSGetConstantBuffers
Index: 78 | IAGetInputLayout
Index: 79 | IAGetVertexBuffers
Index: 80 | IAGetIndexBuffer
Index: 81 | GSGetConstantBuffers
Index: 82 | GSGetShader
Index: 83 | IAGetPrimitiveTopology
Index: 84 | VSGetShaderResources
Index: 85 | VSGetSamplers
Index: 86 | GetPredication
Index: 87 | GSGetShaderResources
Index: 88 | GSGetSamplers
Index: 89 | OMGetRenderTargets
Index: 90 | OMGetRenderTargetsAndUnorderedAccessViews
Index: 91 | OMGetBlendState
Index: 92 | OMGetDepthStencilState
Index: 93 | SOGetTargets
Index: 94 | RSGetState
Index: 95 | RSGetViewports
Index: 96 | RSGetScissorRects
Index: 97 | HSGetShaderResources
Index: 98 | HSGetShader
Index: 99 | HSGetSamplers
Index: 100 | HSGetConstantBuffers
Index: 101 | DSGetShaderResources
Index: 102 | DSGetShader
Index: 103 | DSGetSamplers
Index: 104 | DSGetConstantBuffers
Index: 105 | CSGetShaderResources
Index: 106 | CSGetUnorderedAccessViews
Index: 107 | CSGetShader
Index: 108 | CSGetSamplers
Index: 109 | CSGetConstantBuffers
Index: 110 | ClearState
Index: 111 | Flush
Index: 112 | GetType
Index: 113 | GetContextFlags
Index: 114 | FinishCommandList


// ID3D11 DEVICE virtuals
#define CREATEBUFFER						0
#define CREATETEXTURE1D						1
#define CREATETEXTURE2D						2
#define CREATETEXTURE3D						3
#define CREATESHADERRESOURCEVIEW			4
#define CREATEUNORDEREDACCESSVIEW			5
#define CREATERENDERTARGETVIEW				6
#define CREATEDEPTHSTENCILVIEW				7
#define CREATEINPUTLAYOUT					8
#define CREATEVERTEXSHADER					9
#define CREATEGEOMETRYSHADER				10
#define CREATEGEOMETRYSHADERWITHSREAMOUTPUT				11
#define CREATEPIXELSHADER					12
#define CREATEHULLSHADER					13
#define CREATEDOMAINSHADER					14
#define CREATECOMPUTESHADER					15
#define CREATECLASSLINKAGE					16
#define CREATEBLENDSTATE					17
#define CREATEDEPTHSTENCILSTATE				18
#define CREATERASTERIZERSTATE				19
#define CREATESAMPLERSTATE					20
#define CREATEQUERY							21
#define CREATEPREDICATE						22
#define CREATECOUNTER						23
#define CREATEDERERREDCONTEXT				24
#define OPENSHADERRESOURCE					25
#define CHECKFORMATSUPPORT					26
#define CHECKMULTISAMPLEQUALITYLEVELS		27
#define CHECKCOUNTERINFO					28
#define CHECKCOUNTER						29
#define CHECKFEATURESUPPORT					30
#define GETPRIVATEDATA						31
#define SETPRIVATEDATA						32
#define SETPRIVATEDATAINTERFACE				33
#define GETFEATURELEVEL						34
#define GETCREATIONFLAGS					35
#define GETDEVICEREMOVEDREASON				36
#define GETIMMEDIATECONTEXT					37
#define SETEXCEPTIONMODE					38
#define GETEXCEPTIONMODE					39


// IDXGI SWAPCHAIN virtuals
[0]    7405CADA    (CMTUseCountedObject<CDXGISwapChain>::QueryInterface)
[1]    7405C9A7    (CMTUseCountedObject<CDXGISwapChain>::AddRef)
[2]    7405C9D8    (CMTUseCountedObject<CDXGISwapChain>::Release)
[3]    7405D6BF    (CDXGISwapChain::SetPrivateData)
[4]    7405F6FC    (CDXGISwapChain::SetPrivateDataInterface)
[5]    7405D6AF    (CDXGISwapChain::GetPrivateData)
[6]    7406106A    (CDXGISwapChain::GetParent)
[7]    7405EFDE    (CDXGISwapChain::GetDevice)
[8]    74061BD1    (CDXGISwapChain::Present)
[9]    740617A7    (CDXGISwapChain::GetBuffer)
[10]    74065CD6    (CDXGISwapChain::SetFullscreenState)
[11]    740662DC    (CDXGISwapChain::GetFullscreenState)
[12]    74061146    (CDXGISwapChain::GetDesc)
[13]    740655ED    (CDXGISwapChain::ResizeBuffers)
[14]    74065B8D    (CDXGISwapChain::ResizeTarget)
[15]    7406197B    (CDXGISwapChain::GetContainingOutput)
[16]    74066524    (CDXGISwapChain::GetFrameStatistics)
[17]    74066A58    (CDXGISwapChain::GetLastPresentCount)
[18]    740612C6    (CDXGISwapChain::GetDesc1)
[19]    740613E0    (CDXGISwapChain::GetFullscreenDesc)
[20]    740614F9    (CDXGISwapChain::GetHwnd)
[21]    7406156D    (CDXGISwapChain::GetCoreWindow)
[22]    74061D0D    (CDXGISwapChain[::IDXGISwapChain1]::Present1)
[23]    74062069    (CDXGISwapChain::IsTemporaryMonoSupported)
[24]    740615BB    (CDXGISwapChain::GetRestrictToOutput)
[25]    740615FB    (CDXGISwapChain::SetBackgroundColor)
[26]    740616F1    (CDXGISwapChain::GetBackgroundColor)
[27]    7406173F    (CDXGISwapChain::SetRotation)
[28]    74061770    (CDXGISwapChain::GetRotation)
[29]    7405CC1A    (CMTUseCountedObject<CDXGISwapChain>::`vector deleting destructor')
[30]    7405181E    (CMTUseCountedObject<CDXGISwapChain>::LUCCompleteLayerConstruction)
[31]    7405CBA5    (DXGID3D10ETWRundown)


// DXGI VTable:
[0]	6ED3F979	(CMTUseCountedObject<CDXGISwapChain>::QueryInterface)
[1]	6ED3F84D	(CMTUseCountedObject<CDXGISwapChain>::AddRef)
[2]	6ED3F77D	(CMTUseCountedObject<CDXGISwapChain>::Release)
[3]	6ED6A6D7	(CDXGISwapChain::SetPrivateData)
[4]	6ED6A904	(CDXGISwapChain::SetPrivateDataInterface)
[5]	6ED72BC9	(CDXGISwapChain::GetPrivateData)
[6]	6ED6DCDD	(CDXGISwapChain::GetParent)
[7]	6ED69BF4	(CDXGISwapChain::GetDevice)
[8]	6ED3FAAD	(CDXGISwapChain::Present)
[9]	6ED40209	(CDXGISwapChain::GetBuffer)
[10]	6ED47C1C	(CDXGISwapChain::SetFullscreenState)
[11]	6ED48CD9	(CDXGISwapChain::GetFullscreenState)
[12]	6ED40CB1	(CDXGISwapChain::GetDesc)
[13]	6ED48A3B	(CDXGISwapChain::ResizeBuffers)
[14]	6ED6F153	(CDXGISwapChain::ResizeTarget)
[15]	6ED47BA5	(CDXGISwapChain::GetContainingOutput)
[16]	6ED6D9B5	(CDXGISwapChain::GetFrameStatistics)
[17]	6ED327B5	(CDXGISwapChain::GetLastPresentCount)
[18]	6ED43400	(CDXGISwapChain::GetDesc1)
[19]	6ED6D9D0	(CDXGISwapChain::GetFullscreenDesc)
[20]	6ED6DA90	(CDXGISwapChain::GetHwnd)
[21]	6ED6D79F	(CDXGISwapChain::GetCoreWindow)
[22]	6ED6E352	(?Present1@?QIDXGISwapChain2@@CDXGISwapChain@@UAGJIIPBUDXGI_PRESENT_PARAMETERS@@@Z)
[23]	6ED6E240	(CDXGISwapChain::IsTemporaryMonoSupported)
[24]	6ED44146	(CDXGISwapChain::GetRestrictToOutput)
[25]	6ED6F766	(CDXGISwapChain::SetBackgroundColor)
[26]	6ED6D6B9	(CDXGISwapChain::GetBackgroundColor)
[27]	6ED4417B	(CDXGISwapChain::SetRotation)
[28]	6ED6DDE3	(CDXGISwapChain::GetRotation)
[29]	6ED6FF85	(CDXGISwapChain::SetSourceSize)
[30]	6ED6DF4F	(CDXGISwapChain::GetSourceSize)
[31]	6ED6FCBD	(CDXGISwapChain::SetMaximumFrameLatency)
[32]	6ED6DBE5	(CDXGISwapChain::GetMaximumFrameLatency)
[33]	6ED6D8CD	(CDXGISwapChain::GetFrameLatencyWaitableObject)
[34]	6ED6FB45	(CDXGISwapChain::SetMatrixTransform)
[35]	6ED6DAD0	(CDXGISwapChain::GetMatrixTransform)
[36]	6ED6C155	(CDXGISwapChain::CheckMultiplaneOverlaySupportInternal)
[37]	6ED6E82D	(CDXGISwapChain::PresentMultiplaneOverlayInternal)
[38]	6ED4397A	(CMTUseCountedObject<CDXGISwapChain>::`vector deleting destructor')
[39]	6ED4EAE0	(CSwapBuffer::AddRef)
[40]	6ED46C81	(CMTUseCountedObject<CDXGISwapChain>::LUCBeginLayerDestruction)
*/
