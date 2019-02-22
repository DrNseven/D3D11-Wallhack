//D3D11 wallhack by n7
//compile in release mode, not debug

#include <Windows.h>
#include <vector>
#include <d3d11.h>
#include <D3Dcompiler.h> //generateshader
#pragma comment(lib, "D3dcompiler.lib")
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "winmm.lib") //timeGetTime
#include "MinHook/include/MinHook.h" //detour x86&x64
#include "FW1FontWrapper/FW1FontWrapper.h" //font
#pragma warning( disable : 4244 )


typedef HRESULT(__stdcall *D3D11PresentHook) (IDXGISwapChain* pSwapChain, UINT SyncInterval, UINT Flags);
typedef HRESULT(__stdcall *D3D11ResizeBuffersHook) (IDXGISwapChain *pSwapChain, UINT BufferCount, UINT Width, UINT Height, DXGI_FORMAT NewFormat, UINT SwapChainFlags);
typedef void(__stdcall *D3D11PSSetShaderResourcesHook) (ID3D11DeviceContext* pContext, UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView *const *ppShaderResourceViews);
typedef void(__stdcall *D3D11DrawHook) (ID3D11DeviceContext* pContext, UINT VertexCount, UINT StartVertexLocation);
typedef void(__stdcall *D3D11DrawIndexedHook) (ID3D11DeviceContext* pContext, UINT IndexCount, UINT StartIndexLocation, INT BaseVertexLocation);
typedef void(__stdcall *D3D11DrawIndexedInstancedHook) (ID3D11DeviceContext* pContext, UINT IndexCountPerInstance, UINT InstanceCount, UINT StartIndexLocation, INT BaseVertexLocation, UINT StartInstanceLocation);
typedef void(__stdcall *D3D11CreateQueryHook) (ID3D11Device* pDevice, const D3D11_QUERY_DESC *pQueryDesc, ID3D11Query **ppQuery);


D3D11PresentHook phookD3D11Present = NULL;
D3D11ResizeBuffersHook phookD3D11ResizeBuffers = NULL;
D3D11PSSetShaderResourcesHook phookD3D11PSSetShaderResources = NULL;
D3D11DrawHook phookD3D11Draw = NULL;
D3D11DrawIndexedHook phookD3D11DrawIndexed = NULL;
D3D11DrawIndexedInstancedHook phookD3D11DrawIndexedInstanced = NULL;
D3D11CreateQueryHook phookD3D11CreateQuery = NULL;


ID3D11Device *pDevice = NULL;
ID3D11DeviceContext *pContext = NULL;

DWORD_PTR* pSwapChainVtable = NULL;
DWORD_PTR* pContextVTable = NULL;
DWORD_PTR* pDeviceVTable = NULL;

IFW1Factory *pFW1Factory = NULL;
IFW1FontWrapper *pFontWrapper = NULL;

#include "main.h" //helper funcs


//==========================================================================================================================

HRESULT __stdcall hookD3D11ResizeBuffers(IDXGISwapChain *pSwapChain, UINT BufferCount, UINT Width, UINT Height, DXGI_FORMAT NewFormat, UINT SwapChainFlags)
{
	if (RenderTargetView != NULL) { RenderTargetView->Release(); RenderTargetView = NULL; }

	return phookD3D11ResizeBuffers(pSwapChain, BufferCount, Width, Height, NewFormat, SwapChainFlags);
}

//==========================================================================================================================

HRESULT __stdcall hookD3D11Present(IDXGISwapChain* pSwapChain, UINT SyncInterval, UINT Flags)
{
	if (firstTime)
	{
		firstTime = false;

		//get device
		if (SUCCEEDED(pSwapChain->GetDevice(__uuidof(ID3D11Device), (void **)&pDevice)))
		{
			pSwapChain->GetDevice(__uuidof(pDevice), (void**)&pDevice);
			pDevice->GetImmediateContext(&pContext);
		}

		//create depthstencilstate
		D3D11_DEPTH_STENCIL_DESC  stencilDesc;
		stencilDesc.DepthFunc = D3D11_COMPARISON_LESS;
		stencilDesc.StencilEnable = true;
		stencilDesc.StencilReadMask = 0xFF;
		stencilDesc.StencilWriteMask = 0xFF;
		stencilDesc.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
		stencilDesc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_INCR;
		stencilDesc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
		stencilDesc.FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
		stencilDesc.BackFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
		stencilDesc.BackFace.StencilDepthFailOp = D3D11_STENCIL_OP_DECR;
		stencilDesc.BackFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
		stencilDesc.BackFace.StencilFunc = D3D11_COMPARISON_ALWAYS;

		stencilDesc.DepthEnable = true;
		stencilDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
		pDevice->CreateDepthStencilState(&stencilDesc, &myDepthStencilStates[static_cast<int>(eDepthState::ENABLED)]);

		stencilDesc.DepthEnable = false;
		stencilDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
		pDevice->CreateDepthStencilState(&stencilDesc, &myDepthStencilStates[static_cast<int>(eDepthState::DISABLED)]);

		stencilDesc.DepthEnable = false;
		stencilDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
		stencilDesc.StencilEnable = false;
		stencilDesc.StencilReadMask = UINT8(0xFF);
		stencilDesc.StencilWriteMask = 0x0;
		pDevice->CreateDepthStencilState(&stencilDesc, &myDepthStencilStates[static_cast<int>(eDepthState::NO_READ_NO_WRITE)]);

		stencilDesc.DepthEnable = true;
		//stencilDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
		stencilDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
		stencilDesc.DepthFunc = D3D11_COMPARISON_LESS;
		stencilDesc.StencilEnable = false;
		stencilDesc.StencilReadMask = UINT8(0xFF);
		stencilDesc.StencilWriteMask = 0x0;

		stencilDesc.FrontFace.StencilFailOp = D3D11_STENCIL_OP_ZERO;
		stencilDesc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_ZERO;
		stencilDesc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
		stencilDesc.FrontFace.StencilFunc = D3D11_COMPARISON_EQUAL;

		stencilDesc.BackFace.StencilFailOp = D3D11_STENCIL_OP_ZERO;
		stencilDesc.BackFace.StencilDepthFailOp = D3D11_STENCIL_OP_ZERO;
		stencilDesc.BackFace.StencilPassOp = D3D11_STENCIL_OP_ZERO;
		stencilDesc.BackFace.StencilFunc = D3D11_COMPARISON_NEVER;
		pDevice->CreateDepthStencilState(&stencilDesc, &myDepthStencilStates[static_cast<int>(eDepthState::READ_NO_WRITE)]);


		//create depthstencilstate2
		D3D11_DEPTH_STENCIL_DESC stencilDesc1;
		stencilDesc1.DepthFunc = D3D11_COMPARISON_NEVER;
		stencilDesc1.StencilEnable = true;
		stencilDesc1.StencilReadMask = 0xFF;
		stencilDesc1.StencilWriteMask = 0xFF;
		stencilDesc1.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
		stencilDesc1.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_INCR;
		stencilDesc1.FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
		stencilDesc1.FrontFace.StencilFunc = D3D11_COMPARISON_NEVER;
		stencilDesc1.BackFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
		stencilDesc1.BackFace.StencilDepthFailOp = D3D11_STENCIL_OP_DECR;
		stencilDesc1.BackFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
		stencilDesc1.BackFace.StencilFunc = D3D11_COMPARISON_NEVER;
		stencilDesc1.DepthEnable = true;
		stencilDesc1.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
		pDevice->CreateDepthStencilState(&stencilDesc1, &myDepthStencilStates[static_cast<int>(eDepthState::ENABLED1)]);

		D3D11_DEPTH_STENCIL_DESC stencilDesc2;
		stencilDesc2.DepthFunc = (D3D11_COMPARISON_FUNC)2;
		stencilDesc2.StencilEnable = true;
		stencilDesc2.StencilReadMask = 0xFF;
		stencilDesc2.StencilWriteMask = 0xFF;
		stencilDesc2.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
		stencilDesc2.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_INCR;
		stencilDesc2.FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
		//stencilDesc2.FrontFace.StencilFunc = (D3D11_COMPARISON_FUNC)2;
		stencilDesc2.FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
		stencilDesc2.BackFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
		stencilDesc2.BackFace.StencilDepthFailOp = D3D11_STENCIL_OP_DECR;
		stencilDesc2.BackFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
		//stencilDesc2.BackFace.StencilFunc = (D3D11_COMPARISON_FUNC)2;
		stencilDesc2.BackFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
		stencilDesc2.DepthEnable = true;
		stencilDesc2.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
		pDevice->CreateDepthStencilState(&stencilDesc2, &myDepthStencilStates[static_cast<int>(eDepthState::ENABLED2)]);

		D3D11_DEPTH_STENCIL_DESC stencilDesc3;
		stencilDesc3.DepthFunc = (D3D11_COMPARISON_FUNC)3;
		stencilDesc3.StencilEnable = true;
		stencilDesc3.StencilReadMask = 0xFF;
		stencilDesc3.StencilWriteMask = 0xFF;
		stencilDesc3.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
		stencilDesc3.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_INCR;
		stencilDesc3.FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
		stencilDesc3.FrontFace.StencilFunc = (D3D11_COMPARISON_FUNC)3;
		stencilDesc3.BackFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
		stencilDesc3.BackFace.StencilDepthFailOp = D3D11_STENCIL_OP_DECR;
		stencilDesc3.BackFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
		stencilDesc3.BackFace.StencilFunc = (D3D11_COMPARISON_FUNC)3;
		stencilDesc3.DepthEnable = true;
		stencilDesc3.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
		pDevice->CreateDepthStencilState(&stencilDesc3, &myDepthStencilStates[static_cast<int>(eDepthState::ENABLED3)]);

		D3D11_DEPTH_STENCIL_DESC stencilDesc4;
		stencilDesc4.DepthFunc = (D3D11_COMPARISON_FUNC)4;
		stencilDesc4.StencilEnable = true;
		stencilDesc4.StencilReadMask = 0xFF;
		stencilDesc4.StencilWriteMask = 0xFF;
		stencilDesc4.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
		stencilDesc4.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_INCR;
		stencilDesc4.FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
		stencilDesc4.FrontFace.StencilFunc = (D3D11_COMPARISON_FUNC)4;
		stencilDesc4.BackFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
		stencilDesc4.BackFace.StencilDepthFailOp = D3D11_STENCIL_OP_DECR;
		stencilDesc4.BackFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
		stencilDesc4.BackFace.StencilFunc = (D3D11_COMPARISON_FUNC)4;
		stencilDesc4.DepthEnable = true;
		stencilDesc4.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
		pDevice->CreateDepthStencilState(&stencilDesc4, &myDepthStencilStates[static_cast<int>(eDepthState::ENABLED4)]);

		D3D11_DEPTH_STENCIL_DESC stencilDesc5;
		stencilDesc5.DepthFunc = (D3D11_COMPARISON_FUNC)5;
		stencilDesc5.StencilEnable = true;
		stencilDesc5.StencilReadMask = 0xFF;
		stencilDesc5.StencilWriteMask = 0xFF;
		stencilDesc5.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
		stencilDesc5.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_INCR;
		stencilDesc5.FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
		stencilDesc5.FrontFace.StencilFunc = (D3D11_COMPARISON_FUNC)5;
		stencilDesc5.BackFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
		stencilDesc5.BackFace.StencilDepthFailOp = D3D11_STENCIL_OP_DECR;
		stencilDesc5.BackFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
		stencilDesc5.BackFace.StencilFunc = (D3D11_COMPARISON_FUNC)5;
		stencilDesc5.DepthEnable = true;
		stencilDesc5.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
		pDevice->CreateDepthStencilState(&stencilDesc5, &myDepthStencilStates[static_cast<int>(eDepthState::ENABLED5)]);

		D3D11_DEPTH_STENCIL_DESC stencilDesc6;
		stencilDesc6.DepthFunc = (D3D11_COMPARISON_FUNC)6;
		stencilDesc6.StencilEnable = true;
		stencilDesc6.StencilReadMask = 0xFF;
		stencilDesc6.StencilWriteMask = 0xFF;
		stencilDesc6.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
		stencilDesc6.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_INCR;
		stencilDesc6.FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
		stencilDesc6.FrontFace.StencilFunc = (D3D11_COMPARISON_FUNC)6;
		stencilDesc6.BackFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
		stencilDesc6.BackFace.StencilDepthFailOp = D3D11_STENCIL_OP_DECR;
		stencilDesc6.BackFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
		stencilDesc6.BackFace.StencilFunc = (D3D11_COMPARISON_FUNC)6;
		stencilDesc6.DepthEnable = true;
		stencilDesc6.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
		pDevice->CreateDepthStencilState(&stencilDesc6, &myDepthStencilStates[static_cast<int>(eDepthState::ENABLED6)]);

		D3D11_DEPTH_STENCIL_DESC stencilDesc7;
		stencilDesc7.DepthFunc = (D3D11_COMPARISON_FUNC)7;
		stencilDesc7.StencilEnable = true;
		stencilDesc7.StencilReadMask = 0xFF;
		stencilDesc7.StencilWriteMask = 0xFF;
		stencilDesc7.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
		stencilDesc7.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_INCR;
		stencilDesc7.FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
		stencilDesc7.FrontFace.StencilFunc = (D3D11_COMPARISON_FUNC)7;
		stencilDesc7.BackFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
		stencilDesc7.BackFace.StencilDepthFailOp = D3D11_STENCIL_OP_DECR;
		stencilDesc7.BackFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
		stencilDesc7.BackFace.StencilFunc = (D3D11_COMPARISON_FUNC)7;
		stencilDesc7.DepthEnable = true;
		stencilDesc7.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
		pDevice->CreateDepthStencilState(&stencilDesc7, &myDepthStencilStates[static_cast<int>(eDepthState::ENABLED7)]);

		D3D11_DEPTH_STENCIL_DESC stencilDesc8;
		stencilDesc8.DepthFunc = (D3D11_COMPARISON_FUNC)8;
		stencilDesc8.StencilEnable = true;
		stencilDesc8.StencilReadMask = 0xFF;
		stencilDesc8.StencilWriteMask = 0xFF;
		stencilDesc8.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
		stencilDesc8.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_INCR;
		stencilDesc8.FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
		stencilDesc8.FrontFace.StencilFunc = (D3D11_COMPARISON_FUNC)8;
		stencilDesc8.BackFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
		stencilDesc8.BackFace.StencilDepthFailOp = D3D11_STENCIL_OP_DECR;
		stencilDesc8.BackFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
		stencilDesc8.BackFace.StencilFunc = (D3D11_COMPARISON_FUNC)8;
		stencilDesc8.DepthEnable = true;
		stencilDesc8.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
		pDevice->CreateDepthStencilState(&stencilDesc8, &myDepthStencilStates[static_cast<int>(eDepthState::ENABLED8)]);

		//stencilDesc.DepthEnable = false;
		//stencilDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
		//pDevice->CreateDepthStencilState(&stencilDesc, &myDepthStencilStates[static_cast<int>(eDepthState::DISABLED)]);

		//stencilDesc.DepthEnable = false;
		//stencilDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
		//stencilDesc.StencilEnable = false;
		//stencilDesc.StencilReadMask = UINT8(0xFF);
		//stencilDesc.StencilWriteMask = 0x0;
		//pDevice->CreateDepthStencilState(&stencilDesc, &myDepthStencilStates[static_cast<int>(eDepthState::NO_READ_NO_WRITE)]);

		D3D11_DEPTH_STENCIL_DESC stencilDescRNW1;
		stencilDescRNW1.DepthEnable = true;
		//stencilDescRNW1.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
		stencilDescRNW1.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
		stencilDescRNW1.DepthFunc = D3D11_COMPARISON_NEVER;
		stencilDescRNW1.StencilEnable = false;
		stencilDescRNW1.StencilReadMask = UINT8(0xFF);
		stencilDescRNW1.StencilWriteMask = 0x0;
		stencilDescRNW1.FrontFace.StencilFailOp = D3D11_STENCIL_OP_ZERO;
		stencilDescRNW1.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_ZERO;
		stencilDescRNW1.FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
		stencilDescRNW1.FrontFace.StencilFunc = D3D11_COMPARISON_NEVER;
		stencilDescRNW1.BackFace.StencilFailOp = D3D11_STENCIL_OP_ZERO;
		stencilDescRNW1.BackFace.StencilDepthFailOp = D3D11_STENCIL_OP_ZERO;
		stencilDescRNW1.BackFace.StencilPassOp = D3D11_STENCIL_OP_ZERO;
		stencilDescRNW1.BackFace.StencilFunc = D3D11_COMPARISON_NEVER;
		pDevice->CreateDepthStencilState(&stencilDescRNW1, &myDepthStencilStates[static_cast<int>(eDepthState::READ_NO_WRITE1)]);

		D3D11_DEPTH_STENCIL_DESC stencilDescRNW2;
		stencilDescRNW2.DepthEnable = true;
		stencilDescRNW2.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
		stencilDescRNW2.DepthFunc = (D3D11_COMPARISON_FUNC)2;
		stencilDescRNW2.StencilEnable = false;
		stencilDescRNW2.StencilReadMask = UINT8(0xFF);
		stencilDescRNW2.StencilWriteMask = 0x0;
		stencilDescRNW2.FrontFace.StencilFailOp = D3D11_STENCIL_OP_ZERO;
		stencilDescRNW2.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_ZERO;
		stencilDescRNW2.FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
		//stencilDescRNW2.FrontFace.StencilFunc = (D3D11_COMPARISON_FUNC)2;
		stencilDescRNW2.FrontFace.StencilFunc = D3D11_COMPARISON_EQUAL;
		stencilDescRNW2.BackFace.StencilFailOp = D3D11_STENCIL_OP_ZERO;
		stencilDescRNW2.BackFace.StencilDepthFailOp = D3D11_STENCIL_OP_ZERO;
		stencilDescRNW2.BackFace.StencilPassOp = D3D11_STENCIL_OP_ZERO;
		//stencilDescRNW2.BackFace.StencilFunc = (D3D11_COMPARISON_FUNC)2;
		stencilDescRNW2.BackFace.StencilFunc = D3D11_COMPARISON_NEVER;
		pDevice->CreateDepthStencilState(&stencilDescRNW2, &myDepthStencilStates[static_cast<int>(eDepthState::READ_NO_WRITE2)]);

		D3D11_DEPTH_STENCIL_DESC stencilDescRNW3;
		stencilDescRNW3.DepthEnable = true;
		stencilDescRNW3.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
		stencilDescRNW3.DepthFunc = (D3D11_COMPARISON_FUNC)3;
		stencilDescRNW3.StencilEnable = false;
		stencilDescRNW3.StencilReadMask = UINT8(0xFF);
		stencilDescRNW3.StencilWriteMask = 0x0;
		stencilDescRNW3.FrontFace.StencilFailOp = D3D11_STENCIL_OP_ZERO;
		stencilDescRNW3.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_ZERO;
		stencilDescRNW3.FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
		stencilDescRNW3.FrontFace.StencilFunc = (D3D11_COMPARISON_FUNC)3;
		stencilDescRNW3.BackFace.StencilFailOp = D3D11_STENCIL_OP_ZERO;
		stencilDescRNW3.BackFace.StencilDepthFailOp = D3D11_STENCIL_OP_ZERO;
		stencilDescRNW3.BackFace.StencilPassOp = D3D11_STENCIL_OP_ZERO;
		stencilDescRNW3.BackFace.StencilFunc = (D3D11_COMPARISON_FUNC)3;
		pDevice->CreateDepthStencilState(&stencilDescRNW3, &myDepthStencilStates[static_cast<int>(eDepthState::READ_NO_WRITE3)]);

		D3D11_DEPTH_STENCIL_DESC stencilDescRNW4;
		stencilDescRNW4.DepthEnable = true;
		stencilDescRNW4.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
		stencilDescRNW4.DepthFunc = (D3D11_COMPARISON_FUNC)4;
		stencilDescRNW4.StencilEnable = false;
		stencilDescRNW4.StencilReadMask = UINT8(0xFF);
		stencilDescRNW4.StencilWriteMask = 0x0;
		stencilDescRNW4.FrontFace.StencilFailOp = D3D11_STENCIL_OP_ZERO;
		stencilDescRNW4.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_ZERO;
		stencilDescRNW4.FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
		stencilDescRNW4.FrontFace.StencilFunc = (D3D11_COMPARISON_FUNC)4;
		stencilDescRNW4.BackFace.StencilFailOp = D3D11_STENCIL_OP_ZERO;
		stencilDescRNW4.BackFace.StencilDepthFailOp = D3D11_STENCIL_OP_ZERO;
		stencilDescRNW4.BackFace.StencilPassOp = D3D11_STENCIL_OP_ZERO;
		stencilDescRNW4.BackFace.StencilFunc = (D3D11_COMPARISON_FUNC)4;
		pDevice->CreateDepthStencilState(&stencilDescRNW4, &myDepthStencilStates[static_cast<int>(eDepthState::READ_NO_WRITE4)]);

		D3D11_DEPTH_STENCIL_DESC stencilDescRNW5;
		stencilDescRNW5.DepthEnable = true;
		stencilDescRNW5.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
		stencilDescRNW5.DepthFunc = (D3D11_COMPARISON_FUNC)5;
		stencilDescRNW5.StencilEnable = false;
		stencilDescRNW5.StencilReadMask = UINT8(0xFF);
		stencilDescRNW5.StencilWriteMask = 0x0;
		stencilDescRNW5.FrontFace.StencilFailOp = D3D11_STENCIL_OP_ZERO;
		stencilDescRNW5.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_ZERO;
		stencilDescRNW5.FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
		stencilDescRNW5.FrontFace.StencilFunc = (D3D11_COMPARISON_FUNC)5;
		stencilDescRNW5.BackFace.StencilFailOp = D3D11_STENCIL_OP_ZERO;
		stencilDescRNW5.BackFace.StencilDepthFailOp = D3D11_STENCIL_OP_ZERO;
		stencilDescRNW5.BackFace.StencilPassOp = D3D11_STENCIL_OP_ZERO;
		stencilDescRNW5.BackFace.StencilFunc = (D3D11_COMPARISON_FUNC)5;
		pDevice->CreateDepthStencilState(&stencilDescRNW5, &myDepthStencilStates[static_cast<int>(eDepthState::READ_NO_WRITE5)]);

		D3D11_DEPTH_STENCIL_DESC stencilDescRNW6;
		stencilDescRNW6.DepthEnable = true;
		stencilDescRNW6.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
		stencilDescRNW6.DepthFunc = (D3D11_COMPARISON_FUNC)6;
		stencilDescRNW6.StencilEnable = false;
		stencilDescRNW6.StencilReadMask = UINT8(0xFF);
		stencilDescRNW6.StencilWriteMask = 0x0;
		stencilDescRNW6.FrontFace.StencilFailOp = D3D11_STENCIL_OP_ZERO;
		stencilDescRNW6.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_ZERO;
		stencilDescRNW6.FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
		stencilDescRNW6.FrontFace.StencilFunc = (D3D11_COMPARISON_FUNC)6;
		stencilDescRNW6.BackFace.StencilFailOp = D3D11_STENCIL_OP_ZERO;
		stencilDescRNW6.BackFace.StencilDepthFailOp = D3D11_STENCIL_OP_ZERO;
		stencilDescRNW6.BackFace.StencilPassOp = D3D11_STENCIL_OP_ZERO;
		stencilDescRNW6.BackFace.StencilFunc = (D3D11_COMPARISON_FUNC)6;
		pDevice->CreateDepthStencilState(&stencilDescRNW6, &myDepthStencilStates[static_cast<int>(eDepthState::READ_NO_WRITE6)]);

		D3D11_DEPTH_STENCIL_DESC stencilDescRNW7;
		stencilDescRNW7.DepthEnable = true;
		stencilDescRNW7.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
		stencilDescRNW7.DepthFunc = (D3D11_COMPARISON_FUNC)7;
		stencilDescRNW7.StencilEnable = false;
		stencilDescRNW7.StencilReadMask = UINT8(0xFF);
		stencilDescRNW7.StencilWriteMask = 0x0;
		stencilDescRNW7.FrontFace.StencilFailOp = D3D11_STENCIL_OP_ZERO;
		stencilDescRNW7.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_ZERO;
		stencilDescRNW7.FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
		stencilDescRNW7.FrontFace.StencilFunc = (D3D11_COMPARISON_FUNC)7;
		stencilDescRNW7.BackFace.StencilFailOp = D3D11_STENCIL_OP_ZERO;
		stencilDescRNW7.BackFace.StencilDepthFailOp = D3D11_STENCIL_OP_ZERO;
		stencilDescRNW7.BackFace.StencilPassOp = D3D11_STENCIL_OP_ZERO;
		stencilDescRNW7.BackFace.StencilFunc = (D3D11_COMPARISON_FUNC)7;
		pDevice->CreateDepthStencilState(&stencilDescRNW7, &myDepthStencilStates[static_cast<int>(eDepthState::READ_NO_WRITE7)]);

		D3D11_DEPTH_STENCIL_DESC stencilDescRNW8;
		stencilDescRNW8.DepthEnable = true;
		stencilDescRNW8.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
		stencilDescRNW8.DepthFunc = (D3D11_COMPARISON_FUNC)8;
		stencilDescRNW8.StencilEnable = false;
		stencilDescRNW8.StencilReadMask = UINT8(0xFF);
		stencilDescRNW8.StencilWriteMask = 0x0;
		stencilDescRNW8.FrontFace.StencilFailOp = D3D11_STENCIL_OP_ZERO;
		stencilDescRNW8.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_ZERO;
		stencilDescRNW8.FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
		stencilDescRNW8.FrontFace.StencilFunc = (D3D11_COMPARISON_FUNC)8;
		stencilDescRNW8.BackFace.StencilFailOp = D3D11_STENCIL_OP_ZERO;
		stencilDescRNW8.BackFace.StencilDepthFailOp = D3D11_STENCIL_OP_ZERO;
		stencilDescRNW8.BackFace.StencilPassOp = D3D11_STENCIL_OP_ZERO;
		stencilDescRNW8.BackFace.StencilFunc = (D3D11_COMPARISON_FUNC)8;
		pDevice->CreateDepthStencilState(&stencilDescRNW8, &myDepthStencilStates[static_cast<int>(eDepthState::READ_NO_WRITE8)]);


		//create wallhacked rasterizer
		D3D11_RASTERIZER_DESC rasterizer_desc;
		ZeroMemory(&rasterizer_desc, sizeof(rasterizer_desc));
		rasterizer_desc.FillMode = D3D11_FILL_SOLID;
		rasterizer_desc.CullMode = D3D11_CULL_NONE; //D3D11_CULL_FRONT;
		rasterizer_desc.FrontCounterClockwise = false;
		float bias = 1000.0f;
		float bias_float = static_cast<float>(-bias);
		bias_float /= 10000.0f;
		rasterizer_desc.DepthBias = DEPTH_BIAS_D32_FLOAT(*(DWORD*)&bias_float); //c
		rasterizer_desc.SlopeScaledDepthBias = 0.0f;
		rasterizer_desc.DepthBiasClamp = 0.0f;
		rasterizer_desc.DepthClipEnable = true;
		rasterizer_desc.ScissorEnable = false;
		rasterizer_desc.MultisampleEnable = false;
		rasterizer_desc.AntialiasedLineEnable = false;
		hr = pDevice->CreateRasterizerState(&rasterizer_desc, &rDEPTHBIASState);
		if (FAILED(hr)) { Log("Failed to CreateRasterizerState rasterizer_desc"); }

		//create normal rasterizer
		D3D11_RASTERIZER_DESC nrasterizer_desc;
		ZeroMemory(&nrasterizer_desc, sizeof(nrasterizer_desc));
		nrasterizer_desc.FillMode = D3D11_FILL_SOLID;
		//nrasterizer_desc.CullMode = D3D11_CULL_BACK; //flickering
		nrasterizer_desc.CullMode = D3D11_CULL_NONE; 
		nrasterizer_desc.FrontCounterClockwise = false;
		nrasterizer_desc.DepthBias = 0.0f;
		nrasterizer_desc.SlopeScaledDepthBias = 0.0f;
		nrasterizer_desc.DepthBiasClamp = 0.0f;
		nrasterizer_desc.DepthClipEnable = true;
		nrasterizer_desc.ScissorEnable = false;
		nrasterizer_desc.MultisampleEnable = false;
		nrasterizer_desc.AntialiasedLineEnable = false;
		hr = pDevice->CreateRasterizerState(&nrasterizer_desc, &rNORMALState);
		if (FAILED(hr)) { Log("Failed to CreateRasterizerState nrasterizer_desc"); }

		//create wireframe
		D3D11_RASTERIZER_DESC rwDesc;
		pContext->RSGetState(&rWIREFRAMEState);
		if (rWIREFRAMEState != NULL) {
			rWIREFRAMEState->GetDesc(&rwDesc);
			rwDesc.FillMode = D3D11_FILL_WIREFRAME;
			rwDesc.CullMode = D3D11_CULL_NONE;
		}
		else { Log("No RS state set, defaults are in use"); }
		hr = pDevice->CreateRasterizerState(&rwDesc, &rWIREFRAMEState);
		if (FAILED(hr)) { Log("Failed to CreateRasterizerState rwDesc"); }

		//solid
		D3D11_RASTERIZER_DESC rsDesc;
		pContext->RSGetState(&rSOLIDState);
		if (rSOLIDState != NULL) {
			rSOLIDState->GetDesc(&rsDesc);
			rsDesc.FillMode = D3D11_FILL_SOLID;
			rsDesc.CullMode = D3D11_CULL_BACK;
		}
		else { Log("No RS state set, defaults are in use"); }
		pDevice->CreateRasterizerState(&rsDesc, &rSOLIDState);
		if (FAILED(hr)) { Log("Failed to CreateRasterizerState rsDesc"); }

		//create sample state
		D3D11_SAMPLER_DESC sampDesc;
		ZeroMemory(&sampDesc, sizeof(sampDesc));
		sampDesc.Filter = D3D11_FILTER::D3D11_FILTER_MIN_MAG_MIP_LINEAR;
		sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
		sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
		sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
		sampDesc.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
		sampDesc.MaxAnisotropy = 1;
		sampDesc.MinLOD = 0;
		sampDesc.MaxLOD = D3D11_FLOAT32_MAX;
		hr = pDevice->CreateSamplerState(&sampDesc, &pSamplerState);
		if (FAILED(hr)) { Log("Failed to CreateSamplerState"); }

		//create green texture
		DXGI_FORMAT format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB; //format
		static const uint32_t s_pixel = 0xff00ff00; //0xffffffff white, 0xff00ff00 green, 0xffff0000 blue, 0xff0000ff red
		D3D11_SUBRESOURCE_DATA initData = { &s_pixel, sizeof(uint32_t), 0 };
		D3D11_TEXTURE2D_DESC desc;
		memset(&desc, 0, sizeof(desc));
		desc.Width = desc.Height = desc.MipLevels = desc.ArraySize = 1;
		desc.Format = format;
		desc.SampleDesc.Count = 1;
		desc.Usage = D3D11_USAGE_DEFAULT;// D3D11_USAGE_IMMUTABLE;
		desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
		hr = pDevice->CreateTexture2D(&desc, &initData, &texGreen);
		if (FAILED(hr)) { Log("Failed to CreateTexture2D"); }

		//create red texture
		static const uint32_t s_pixelr = 0xff0000ff; //0xffffffff white, 0xff00ff00 green, 0xffff0000 blue, 0xff0000ff red
		D3D11_SUBRESOURCE_DATA initDatar = { &s_pixelr, sizeof(uint32_t), 0 };
		D3D11_TEXTURE2D_DESC descr;
		memset(&descr, 0, sizeof(descr));
		descr.Width = descr.Height = descr.MipLevels = descr.ArraySize = 1;
		descr.Format = format;
		descr.SampleDesc.Count = 1;
		descr.Usage = D3D11_USAGE_DEFAULT;// D3D11_USAGE_IMMUTABLE;
		descr.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
		hr = pDevice->CreateTexture2D(&descr, &initDatar, &texRed);
		if (FAILED(hr)) { Log("Failed to CreateTexture2D"); }

		//create green shaderresourceview
		D3D11_SHADER_RESOURCE_VIEW_DESC SRVDesc;
		memset(&SRVDesc, 0, sizeof(SRVDesc));
		SRVDesc.Format = format;
		SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		SRVDesc.Texture2D.MipLevels = 1;
		hr = pDevice->CreateShaderResourceView(texGreen, &SRVDesc, &texSRVgreen);
		if (FAILED(hr)) { Log("Failed to CreateShaderResourceView"); }
		texGreen->Release();

		//create red shaderresourceview
		D3D11_SHADER_RESOURCE_VIEW_DESC SRVDescr;
		memset(&SRVDescr, 0, sizeof(SRVDescr));
		SRVDescr.Format = format;
		SRVDescr.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		SRVDescr.Texture2D.MipLevels = 1;
		hr = pDevice->CreateShaderResourceView(texRed, &SRVDescr, &texSRVred);
		if (FAILED(hr)) { Log("Failed to CreateShaderResourceView"); }
		texRed->Release();

		//create font
		hr = FW1CreateFactory(FW1_VERSION, &pFW1Factory);
		if (FAILED(hr)) { Log("Failed to FW1CreateFactory"); return hr; }
		hr = pFW1Factory->CreateFontWrapper(pDevice, L"Tahoma", &pFontWrapper);
		if (FAILED(hr)) { Log("Failed to CreateFontWrapper"); return hr; }
		pFW1Factory->Release();

		//load cfg settings
		LoadCfg();
	}

	
	//create rendertarget
	if (RenderTargetView == NULL)
	{
		//viewport
		pContext->RSGetViewports(&vps, &viewport);
		ScreenCenterX = viewport.Width / 2.0f;
		ScreenCenterY = viewport.Height / 2.0f;

		//get backbuffer
		ID3D11Texture2D* backbuffer = NULL;
		hr = pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&backbuffer);
		if (FAILED(hr)) {
			Log("Failed to get BackBuffer");
			pContext->OMGetRenderTargets(1, &RenderTargetView, NULL);
			pContext->OMSetRenderTargets(1, &RenderTargetView, NULL);
			return hr;
		}

		//create rendertarget
		hr = pDevice->CreateRenderTargetView(backbuffer, NULL, &RenderTargetView);
		backbuffer->Release();
		if (FAILED(hr)) {
			Log("Failed to get RenderTarget");
			return hr;
		}
	}
	else //call before you draw
		pContext->OMSetRenderTargets(1, &RenderTargetView, NULL);

	//create shaders
	if (!psRed)
		GenerateShader(pDevice, &psRed, 1.0f, 0.0f, 0.0f);

	if (!psGreen)
		GenerateShader(pDevice, &psGreen, 0.0f, 1.0f, 0.0f);

	//info
	if (greetings && pFontWrapper)
		pFontWrapper->DrawString(pContext, L"D3D11 Wallhack loaded, press INSERT for menu | ALT + L for options", 14, 16.0f, 16.0f, 0xffff1612, FW1_RESTORESTATE | FW1_ALIASED);
	if (greetings)
	{
		static DWORD lastTime = timeGetTime();
		DWORD timePassed = timeGetTime() - lastTime;
		if (timePassed > 9000)
		{
			greetings = false;
			lastTime = timeGetTime();
		}
	}

	//menu
	if (IsReady() == false)
	{
		Init_Menu(pContext, L"D3D11 Menu", 140, 100);
		Do_Menu();
		Color_Font = 0xFFFFFFFF;//white
		Color_Off = 0xFFFF0000;//red
		Color_On = 0xFF00FF00;//green
		Color_Folder = 0xFF2F4F4F;//grey
		Color_Current = 0xFF00BFFF;//orange
	}
	Draw_Menu();
	Navigation();

	
	//logger
	if (logger && pFontWrapper)
	{
		swprintf_s(reportValue, L"Keys: 5/6: find Models, Del = Log [countnum = %d]", countnum);
		pFontWrapper->DrawString(pContext, reportValue, 16.0f, 400.0f, 100.0f, 0xff00ff00, FW1_RESTORESTATE);

		swprintf_s(reportValue, L"Keys: 7/8: [countEdepthstate = %d]", (eDepthState)countEdepth);
		pFontWrapper->DrawString(pContext, reportValue, 16.0f, 400.0f, 120.0f, 0xff00ff00, FW1_RESTORESTATE);

		swprintf_s(reportValue, L"Keys: 9/0: [countRdepthstate = %d]", (eDepthState)countRdepth);
		pFontWrapper->DrawString(pContext, reportValue, 16.0f, 400.0f, 140.0f, 0xff00ff00, FW1_RESTORESTATE);

		//swprintf_s(reportValue, L"Selected Address = [0x%X]", g_SelectedAddress);
		//pFontWrapper->DrawString(pContext, reportValue, 16.0f, 400.0f, 160.0f, 0xff00ff00, FW1_RESTORESTATE);


		pFontWrapper->DrawString(pContext, L"F9 = log drawfunc & depthstates", 16.0f, 400.0f, 180.0f, 0xffffffff, FW1_RESTORESTATE);
	}

	if (GetAsyncKeyState(VK_F10) & 1)
	firstTime = true;


	return phookD3D11Present(pSwapChain, SyncInterval, Flags);
}

//==========================================================================================================================

void __stdcall hookD3D11DrawIndexed(ID3D11DeviceContext* pContext, UINT IndexCount, UINT StartIndexLocation, INT BaseVertexLocation)
{
	if (GetAsyncKeyState(VK_F9) & 1)
		Log("DrawIndexed called");

	//get stride & vedesc.ByteWidth
	pContext->IAGetVertexBuffers(0, 1, &veBuffer, &Stride, &veBufferOffset);
	if (veBuffer != NULL)
		veBuffer->GetDesc(&vedesc);
	if (veBuffer != NULL) { veBuffer->Release(); veBuffer = NULL; }
	
	//get indesc.ByteWidth (comment out if not used)
	pContext->IAGetIndexBuffer(&inBuffer, &inFormat, &inOffset);
	if (inBuffer != NULL)
		inBuffer->GetDesc(&indesc);
	if (inBuffer != NULL) { inBuffer->Release(); inBuffer = NULL; }
	
	//get pscdesc.ByteWidth (comment out if not used)
	pContext->PSGetConstantBuffers(pscStartSlot, 1, &pscBuffer);
	if (pscBuffer != NULL)
		pscBuffer->GetDesc(&pscdesc);
	if (pscBuffer != NULL) { pscBuffer->Release(); pscBuffer = NULL; }

	//get vscdesc.ByteWidth (comment out if not used)
	pContext->VSGetConstantBuffers(vscStartSlot, 1, &vscBuffer);
	if (vscBuffer != NULL)
		vscBuffer->GetDesc(&vscdesc);
	if (vscBuffer != NULL) { vscBuffer->Release(); vscBuffer = NULL; }
	

	ReturnAddress = _ReturnAddress();
	//ReturnAddress = _AddressOfReturnAddress();

	//get rendertarget
	pContext->OMGetRenderTargets(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT, &pRTV[0], &pDSV);

	//wallhack/chams
	if (sOptions[0].Function == 1 || sOptions[1].Function == 1 || sOptions[1].Function == 2) //if wallhack/chams option is enabled in menu
	if (countnum == Stride) //<----------------- try finding models stride first
	//if (countnum == IndexCount / 100)
	//if (ReturnAddress != NULL && g_SelectedAddress != NULL && ReturnAddress == g_SelectedAddress)
	//ut4 models
	//if ((Stride == 32 && IndexCount == 10155)||(Stride == 44 && IndexCount == 11097)||(Stride == 40 && IndexCount == 11412)||(Stride == 40 && IndexCount == 11487)||(Stride == 44 && IndexCount == 83262)||(Stride == 40 && IndexCount == 23283))
	//qc models
	//if (Stride >= 16 && vedesc.ByteWidth >= 14000000 && vedesc.ByteWidth <= 45354840)
	//al models
	//if (Stride == 28 && ((DWORD)ReturnAddress & 0x0000FFFF) == 0x00002d02)
	{	
		//depth OFF for wallhack and chams
		if (sOptions[0].Function == 1 || sOptions[1].Function == 1 || sOptions[1].Function == 2)
		{
			if (rDEPTHBIASState) pContext->RSSetState(rDEPTHBIASState); // set depth bias to do the stencil check in this draw against previously written biased values with same bias
			if ((eDepthState)countRdepth >= 0 && (eDepthState)countRdepth <= 19) SetDepthStencilState((eDepthState)countRdepth); //3=ut4 // read depth buffers with correct comparison func (LESS) but not write them (WRITE_MASK_ZERO)
		}
		if((eDepthState)countRdepth >= 4 && (eDepthState)countRdepth <= 19)
		SAFE_RELEASE(myDepthStencilStates[(eDepthState)countRdepth]);

		//shader chams
		if (sOptions[1].Function == 1)
		pContext->PSSetShader(psRed, NULL, NULL);

		//texture chams
		if (sOptions[1].Function == 2)
		{
			pContext->PSSetShaderResources(0, 1, &texSRVred);
			pContext->PSSetSamplers(0, 1, &pSamplerState);
		}

	phookD3D11DrawIndexed(pContext, IndexCount, StartIndexLocation, BaseVertexLocation);

		//depth ON for wallhack and chams
		if (sOptions[0].Function == 1 || sOptions[1].Function == 1 || sOptions[1].Function == 2)
		{
			if ((eDepthState)countEdepth >= 0 && (eDepthState)countEdepth <= 19) SetDepthStencilState((eDepthState)countEdepth); //8=ut4
			if (rNORMALState) pContext->RSSetState(rNORMALState);
		}

		//shader chams
		if (sOptions[1].Function == 1)
		pContext->PSSetShader(psGreen, NULL, NULL);

		//texture chams
		if (sOptions[1].Function == 2)
		{
			pContext->PSSetShaderResources(0, 1, &texSRVgreen);
			pContext->PSSetSamplers(0, 1, &pSamplerState);
		}
	
	phookD3D11DrawIndexed(pContext, IndexCount, StartIndexLocation, BaseVertexLocation); // render state is normal (not biased) here, so it should only draw Green when actually visible

		//depthbias OFF for wallhack and chams
		if (sOptions[0].Function == 1 || sOptions[1].Function == 1 || sOptions[1].Function == 2)
		if (rDEPTHBIASState) pContext->RSSetState(rDEPTHBIASState);

		pContext->OMSetRenderTargets(0, NULL, pDSV);

	phookD3D11DrawIndexed(pContext, IndexCount, StartIndexLocation, BaseVertexLocation); // finally, make sure the Red (anything) written before gets drawn no matter what by updating the current depth buffer with bias but not the rendertargets to not overdraw again
		
		pContext->OMSetRenderTargets(8, pRTV, pDSV);

		//depthbias ON for wallhack and chams
		if (sOptions[0].Function == 1 || sOptions[1].Function == 1 || sOptions[1].Function == 2)
		if (rNORMALState) pContext->RSSetState(rNORMALState);
	
	}


	//small bruteforce logger
	//press ALT + L in game to enable logger
	//hold 6 till models turn invisible, press DELETE to log values of those models to log.txt
	if (logger)
	{
		if (countnum == Stride || countnum == IndexCount / 100)
			if (GetAsyncKeyState(VK_DELETE) & 1)
				Log("Stride == %d && IndexCount == %d && indesc.ByteWidth == %d && vedesc.ByteWidth == %d && pscdesc.ByteWidth == %d && vscdesc.ByteWidth == %d && pssrStartSlot == %d && vscStartSlot == %d && pssStartSlot == %d && vedesc.Usage == %d && ReturnAddress == 0x%X",
					Stride, IndexCount, indesc.ByteWidth, vedesc.ByteWidth, pscdesc.ByteWidth, vscdesc.ByteWidth, pssrStartSlot, vscStartSlot, pssStartSlot, vedesc.Usage, ReturnAddress); //Descr.Format, Descr.Buffer.NumElements, texdesc.Format, texdesc.Height, texdesc.Width 

		if (GetAsyncKeyState(VK_F9) & 1)
			Log("Stride == %d && countEdepth == %d && countRdepth == %d && ReturnAddress == 0x%X", Stride, countEdepth, countRdepth, ReturnAddress);

		if (!IsAddressPresent(ReturnAddress))
			g_Vector.push_back(ReturnAddress);
	}

	return phookD3D11DrawIndexed(pContext, IndexCount, StartIndexLocation, BaseVertexLocation);
}

//==========================================================================================================================

void __stdcall hookD3D11PSSetShaderResources(ID3D11DeviceContext* pContext, UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView *const *ppShaderResourceViews)
{
	pssrStartSlot = StartSlot;

	//logger keys (here for compatibility reasons)
	if (GetAsyncKeyState(VK_MENU) && GetAsyncKeyState(0x4C) & 1) //ALT + L toggles logger
		logger = !logger;
	if (logger)
	{
		//SaveCfg(); //save settings

		//hold down 6 key until a texture is wallhacked, press I to log values of those textures
		if (GetAsyncKeyState(0x35) & 1) //5-
			countnum--;
		if (GetAsyncKeyState(0x36) & 1) //6+
			countnum++;
		if (GetAsyncKeyState(VK_MENU) && GetAsyncKeyState('9') & 1) //reset, set to -1
			countnum = -1;

		//find SetDepthStencilState value (in front of walls)
		if (GetAsyncKeyState(0x37) & 1) //7-
		{
			countEdepth--;
			firstTime = true; //need to reload or it will bug out
		}
		if (GetAsyncKeyState(0x38) & 1) //8+
		{
			countEdepth++;
			firstTime = true;
		}

		//find SetDepthStencilState value (behind walls)
		if (GetAsyncKeyState(0x39) & 1) //9-
		{
			countRdepth--;
			firstTime = true;
		}

		if (GetAsyncKeyState(0x30) & 1) //0+
		{
			countRdepth++;
			firstTime = true;
		}

		/*
		//retaddr keys
		if (GetAsyncKeyState(VK_PRIOR) & 1) //page up
		{
			if (g_Index != g_Vector.size() - 1)
			{
				g_Index++;
				g_SelectedAddress = g_Vector[g_Index];
			}
		}
		else if (GetAsyncKeyState(VK_NEXT) & 1) //page down
		{
			if (g_Index >= 0)
			{
				g_Index--;
				g_SelectedAddress = g_Vector[g_Index];
				if (g_Index == -1)
					g_SelectedAddress = NULL;
			}
		}
		*/
	}

	/*
	//texture stuff (usually not needed)
	for (UINT j = 0; j < NumViews; j++)
	{
		ID3D11ShaderResourceView* pShaderResView = ppShaderResourceViews[j];
		if (pShaderResView)
		{
			pShaderResView->GetDesc(&Descr);

			ID3D11Resource *Resource;
			pShaderResView->GetResource(&Resource);
			ID3D11Texture2D *Texture = (ID3D11Texture2D *)Resource;
			Texture->GetDesc(&texdesc);

			SAFE_RELEASE(Resource);
			SAFE_RELEASE(Texture);
		}
	}
	*/

	/*
	//ALTERNATIVE wallhack example for f'up games, use this if no draw function works for wallhack
	//if (rDEPTHBIASState) pContext->RSSetState(rDEPTHBIASState); 
	SetDepthStencilState((eDepthState)countRdepth); //depth on
	if (Descr.Format == xx)
	{
		SetDepthStencilState((eDepthState)countEdepth); //depth off
		//if (rNORMALState) pContext->RSSetState(rNORMALState);
		//pContext->PSSetShader(psRed, NULL, NULL);
	}
	*/

	return phookD3D11PSSetShaderResources(pContext, StartSlot, NumViews, ppShaderResourceViews);
}

//==========================================================================================================================

void __stdcall hookD3D11DrawIndexedInstanced(ID3D11DeviceContext* pContext, UINT IndexCountPerInstance, UINT InstanceCount, UINT StartIndexLocation, INT BaseVertexLocation, UINT StartInstanceLocation)
{
	if (GetAsyncKeyState(VK_F9) & 1)
		Log("DrawIndexedInstanced called");

	//if game is drawing player models in DrawIndexedInstanced, do everything here instead (rare)

	return phookD3D11DrawIndexedInstanced(pContext, IndexCountPerInstance, InstanceCount, StartIndexLocation, BaseVertexLocation, StartInstanceLocation);
}

//==========================================================================================================================

void __stdcall hookD3D11Draw(ID3D11DeviceContext* pContext, UINT VertexCount, UINT StartVertexLocation)
{
	if (GetAsyncKeyState(VK_F9) & 1)
		Log("Draw called");

	return phookD3D11Draw(pContext, VertexCount, StartVertexLocation);
}

//==========================================================================================================================

void __stdcall hookD3D11CreateQuery(ID3D11Device* pDevice, const D3D11_QUERY_DESC *pQueryDesc, ID3D11Query **ppQuery)
{
	/*
	//disables Occlusion which prevents rendering player models through certain objects 
	//REDUCES FPS, not recommended, only works if occlusion is client side etc.
	if (pQueryDesc->Query == D3D11_QUERY_OCCLUSION)
	{
		D3D11_QUERY_DESC oqueryDesc = CD3D11_QUERY_DESC();
		(&oqueryDesc)->MiscFlags = pQueryDesc->MiscFlags;
		(&oqueryDesc)->Query = D3D11_QUERY_TIMESTAMP;

		return phookD3D11CreateQuery(pDevice, &oqueryDesc, ppQuery);
	}
	*/
	return phookD3D11CreateQuery(pDevice, pQueryDesc, ppQuery);
}

//==========================================================================================================================

const int MultisampleCount = 1; // Set to 1 to disable multisampling
LRESULT CALLBACK DXGIMsgProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) { return DefWindowProc(hwnd, uMsg, wParam, lParam); }
DWORD __stdcall InitHooks(LPVOID)
{
	HMODULE hDXGIDLL = 0;
	do
	{
		hDXGIDLL = GetModuleHandle("dxgi.dll");
		Sleep(4000);
	} while (!hDXGIDLL);
	Sleep(100);

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
	if (MH_CreateHook((DWORD_PTR*)pSwapChainVtable[13], hookD3D11ResizeBuffers, reinterpret_cast<void**>(&phookD3D11ResizeBuffers)) != MH_OK) { return 1; }
	if (MH_EnableHook((DWORD_PTR*)pSwapChainVtable[13]) != MH_OK) { return 1; }
	if (MH_CreateHook((DWORD_PTR*)pContextVTable[8], hookD3D11PSSetShaderResources, reinterpret_cast<void**>(&phookD3D11PSSetShaderResources)) != MH_OK) { return 1; }
	if (MH_EnableHook((DWORD_PTR*)pContextVTable[8]) != MH_OK) { return 1; }
	if (MH_CreateHook((DWORD_PTR*)pContextVTable[13], hookD3D11Draw, reinterpret_cast<void**>(&phookD3D11Draw)) != MH_OK) { return 1; }
	if (MH_EnableHook((DWORD_PTR*)pContextVTable[13]) != MH_OK) { return 1; }
	if (MH_CreateHook((DWORD_PTR*)pContextVTable[12], hookD3D11DrawIndexed, reinterpret_cast<void**>(&phookD3D11DrawIndexed)) != MH_OK) { return 1; }
	if (MH_EnableHook((DWORD_PTR*)pContextVTable[12]) != MH_OK) { return 1; }
	if (MH_CreateHook((DWORD_PTR*)pContextVTable[20], hookD3D11DrawIndexedInstanced, reinterpret_cast<void**>(&phookD3D11DrawIndexedInstanced)) != MH_OK) { return 1; }
	if (MH_EnableHook((DWORD_PTR*)pContextVTable[20]) != MH_OK) { return 1; }
	//if (MH_CreateHook((DWORD_PTR*)pDeviceVTable[24], hookD3D11CreateQuery, reinterpret_cast<void**>(&phookD3D11CreateQuery)) != MH_OK) { return 1; }
	//if (MH_EnableHook((DWORD_PTR*)pDeviceVTable[24]) != MH_OK) { return 1; }

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
		for (size_t i = strlen(dlldir); i > 0; i--) { if (dlldir[i] == '\\') { dlldir[i + 1] = 0; break; } }
		CreateThread(NULL, 0, InitHooks, NULL, 0, NULL);
		break;

	case DLL_PROCESS_DETACH: // A process unloads the DLL.

		if (pFontWrapper)
		{
			pFontWrapper->Release();
		}

		if (MH_Uninitialize() != MH_OK) { return 1; }
		if (MH_DisableHook((DWORD_PTR*)pSwapChainVtable[8]) != MH_OK) { return 1; }
		if (MH_DisableHook((DWORD_PTR*)pSwapChainVtable[13]) != MH_OK) { return 1; }
		if (MH_DisableHook((DWORD_PTR*)pContextVTable[8]) != MH_OK) { return 1; }
		if (MH_DisableHook((DWORD_PTR*)pSwapChainVtable[13]) != MH_OK) { return 1; }
		if (MH_DisableHook((DWORD_PTR*)pContextVTable[12]) != MH_OK) { return 1; }
		if (MH_DisableHook((DWORD_PTR*)pContextVTable[20]) != MH_OK) { return 1; }
		//if (MH_DisableHook((DWORD_PTR*)pDeviceVTable[24]) != MH_OK) { return 1; }
		break;
	}
	return TRUE;
}