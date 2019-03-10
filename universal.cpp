//D3D11 wallhack by n7
//compile in release mode, not debug

#include <Windows.h>
#include <intrin.h>
#include <vector>
#include <d3d11.h>
#include <D3Dcompiler.h>
#pragma comment(lib, "D3dcompiler.lib")
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "winmm.lib")

//imgui
#include "imgui\imgui.h"
#include "imgui\imgui_impl_win32.h"
#include "imgui\imgui_impl_dx11.h"

//detours
#include "detours.h"
#if defined _M_X64
#pragma comment(lib, "detours.X64/detours.lib")
#elif defined _M_IX86
#pragma comment(lib, "detours.X86/detours.lib")
#endif

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

IDXGISwapChain* SwapChain;
ID3D11Device *pDevice = NULL;
ID3D11DeviceContext *pContext = NULL;

DWORD_PTR* pSwapChainVtable = NULL;
DWORD_PTR* pContextVTable = NULL;
DWORD_PTR* pDeviceVTable = NULL;

#include "main.h" //helper funcs


//==========================================================================================================================

extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK hWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	ImGuiIO& io = ImGui::GetIO();
	POINT mPos;
	GetCursorPos(&mPos);
	ScreenToClient(window, &mPos);
	ImGui::GetIO().MousePos.x = mPos.x;
	ImGui::GetIO().MousePos.y = mPos.y;
	
	/*
	if (uMsg == WM_SIZE)
	{
		if (wParam == SIZE_MINIMIZED)
		ShowMenu = false;

		if (wParam != SIZE_MINIMIZED)
		{
			ImGui_ImplDX11_InvalidateDeviceObjects();
			CleanupRenderTarget();
			SwapChain->ResizeBuffers(0, (UINT)LOWORD(lParam), (UINT)HIWORD(lParam), DXGI_FORMAT_UNKNOWN, 0);
			CreateRenderTarget();
			ImGui_ImplDX11_CreateDeviceObjects();
		}
	}
	*/

	if (uMsg == WM_KEYUP)
	{
		if (wParam == VK_INSERT)
		{
			if(ShowMenu)
				io.MouseDrawCursor = true;
			else
				io.MouseDrawCursor = false;
		}

	}

	if (ShowMenu)
	{
		ImGui_ImplWin32_WndProcHandler(hWnd, uMsg, wParam, lParam);
		return true;
	}

	return CallWindowProc(OriginalWndProcHandler, hWnd, uMsg, wParam, lParam);
}

//==========================================================================================================================
static bool imGuiInitializing = false;
HRESULT __stdcall hookD3D11ResizeBuffers(IDXGISwapChain *pSwapChain, UINT BufferCount, UINT Width, UINT Height, DXGI_FORMAT NewFormat, UINT SwapChainFlags)
{
	imGuiInitializing = true;
	ImGui_ImplDX11_InvalidateDeviceObjects();
	CleanupRenderTarget();
	HRESULT toReturn = phookD3D11ResizeBuffers(pSwapChain, BufferCount, Width, Height, NewFormat, SwapChainFlags);
	//CreateRenderTarget();
	ImGui_ImplDX11_CreateDeviceObjects();
	imGuiInitializing = false;
	
	return toReturn;
}

//==========================================================================================================================

HRESULT __stdcall hookD3D11Present(IDXGISwapChain* pSwapChain, UINT SyncInterval, UINT Flags)
{
	if (firstTime)
	{
		firstTime = false; //only once

		//get device
		if (SUCCEEDED(pSwapChain->GetDevice(__uuidof(ID3D11Device), (void **)&pDevice)))
		{
			SwapChain = pSwapChain;
			pSwapChain->GetDevice(__uuidof(pDevice), (void**)&pDevice);
			pDevice->GetImmediateContext(&pContext);
		}
		
		//imgui
		DXGI_SWAP_CHAIN_DESC sd;
		pSwapChain->GetDesc(&sd);
		ImGui::CreateContext();
		ImGuiIO& io = ImGui::GetIO(); (void)io;
		io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
		window = sd.OutputWindow;

		//wndprochandler
		OriginalWndProcHandler = (WNDPROC)SetWindowLongPtr(window, GWLP_WNDPROC, (LONG_PTR)hWndProc);

		ImGui_ImplWin32_Init(window);
		ImGui_ImplDX11_Init(pDevice, pContext);
		ImGui::GetIO().ImeWindowHandle = window;
		
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
			return hr;
		}

		//create rendertargetview
		hr = pDevice->CreateRenderTargetView(backbuffer, NULL, &RenderTargetView);
		backbuffer->Release();
		if (FAILED(hr)) {
			Log("Failed to get RenderTarget");
			return hr;
		}
	}
	else //call before you draw
		pContext->OMSetRenderTargets(1, &RenderTargetView, NULL);
		
	//create rendertarget & init imgui
	//if (!(Flags & DXGI_PRESENT_TEST) && !imGuiInitializing)
	//{
	//}

	//create depthstencil states
	if (createdepthstencil)
	{
		createdepthstencil = false; //once
		CreateDepthStencilStates();
	}

	//create shaders
	if (!psRed)
		GenerateShader(pDevice, &psRed, 1.0f, 0.0f, 0.0f);

	if (!psGreen)
		GenerateShader(pDevice, &psGreen, 0.0f, 1.0f, 0.0f);

	//imgui
	ImGui_ImplWin32_NewFrame();
	ImGui_ImplDX11_NewFrame();
	ImGui::NewFrame();

	//info
	if (greetings)
	{
		ImVec4 Bgcol = ImColor(0.0f, 0.4f, 0.28f, 0.8f);
		ImGui::PushStyleColor(ImGuiCol_WindowBg, Bgcol);
		ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.2f, 0.2f, 0.2f, 0.8f));

		ImGui::Begin("");
		ImGui::Text("Wallhack loaded, press INSERT for menu");
		ImGui::End();

		static DWORD lastTime = timeGetTime();
		DWORD timePassed = timeGetTime() - lastTime;
		if (timePassed > 6000)
		{
			greetings = false;
			lastTime = timeGetTime();
		}
	}

	//menu
	if (ShowMenu)
	{
		//ImGui::SetNextWindowPos(ImVec2(50.0f, 400.0f)); //pos
		ImGui::SetNextWindowSize(ImVec2(410.0f, 450.0f)); //size
		ImVec4 Bgcol = ImColor(0.0f, 0.4f, 0.28f, 0.8f); //bg color
		ImGui::PushStyleColor(ImGuiCol_WindowBg, Bgcol);
		ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.2f, 0.2f, 0.2f, 0.8f)); //frame color

		ImGui::Begin("Hack Menu");
		ImGui::Checkbox("Wallhack", &Wallhack);
		ImGui::Checkbox("Shader Chams", &ShaderChams);
		ImGui::Checkbox("Texture Chams", &TextureChams);
		ImGui::SliderInt("EDepth", &countEdepth, 0, 20);
		ImGui::SliderInt("RDepth", &countRdepth, 0, 20);
		ImGui::Checkbox("Modelrec Finder", &ModelrecFinder);

		if (ModelrecFinder)
		{
			if (g_Index != g_Vector.size() - 1)
				g_SelectedAddress = g_Vector[g_Index];

			if (!IsAddressPresent(ReturnAddress))
				g_Vector.push_back(ReturnAddress);

			ImGui::SliderInt("find Stride", &countStride, -1, 100);
			ImGui::SliderInt("find IndexCount", &countIndexCount, -1, 100);
			ImGui::SliderInt("find RetAddr", &g_Index, -1, 10);
			ImGui::SliderInt("countnum", &countnum, -1, 100);

			ImGui::Text("Use Keys: (ALT + F1) to toggle Wallhack");
			ImGui::Text("Use Keys: (ALT + F2) to toggle Shader Chams");
			ImGui::Text("Use Keys: (ALT + F3) to toggle Texture Chams");
			ImGui::Text("Use Keys (5/6) to find eDepthState");
			ImGui::Text("Use Keys (7/8) to find rDepthState");
			ImGui::Text("Use Keys (0/0) to find countnum");
			ImGui::Text("Press Home to toggle ModelRec finder");

			ImGui::Text("Use Keys (Page Up/Down) to find Stride");
			ImGui::Text("Press END to log highlated textures");
			ImGui::Text("Press F9 to log draw functions");
			ImGui::Text("Press F10 to reset settings");

			//need to recreate depthstencil if bruteforce Depth
			static int old_Evalue = countEdepth;
			if (countEdepth != old_Evalue)
			{
				//Log("countEdepth is different than previous call");
				createdepthstencil = true; //reload
			}
			old_Evalue = countEdepth;

			static int old_Rvalue = countRdepth;
			if (countRdepth != old_Rvalue)
			{
				//Log("countRdepth is different than previous call");
				createdepthstencil = true; //reload
			}
			old_Rvalue = countRdepth;
		}
		ImGui::End();
	}

	ImGui::EndFrame();
	ImGui::Render();
	ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

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
	

	//get ret addr
	ReturnAddress = _ReturnAddress(); //usually not needed

	//get rendertarget
	pContext->OMGetRenderTargets(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT, &pRTV[0], &pDSV);

	//get orig depthstencil
	pContext->OMGetDepthStencilState(&origDepthStencilState, &stencilRef); //get original
	if(origDepthStencilState && logger) origDepthStencilState->GetDesc(&origdsd);

	//wallhack/chams
	if(Wallhack||ShaderChams||TextureChams) //if wallhack/chams option is enabled in menu
	//
	//___________________________________________________________________________________
	//Model recognition goes here, see your log.txt for the right Stride etc. You may have to do trial and error to see which values work best
	if ((countnum == pssrStartSlot) || //optional: set countnum to something you want to test or log (pssrStartSlot for example) and use keys 0 and 9 to increase/decrease values
	(countStride == Stride || countIndexCount == IndexCount / 400) || (ReturnAddress != NULL && g_SelectedAddress != NULL && ReturnAddress == g_SelectedAddress))
	//___________________________________________________________________________________
	//
	//ut4 models
	//if ((Stride == 32 && IndexCount == 10155)||(Stride == 44 && IndexCount == 11097)||(Stride == 40 && IndexCount == 11412)||(Stride == 40 && IndexCount == 11487)||(Stride == 44 && IndexCount == 83262)||(Stride == 40 && IndexCount == 23283))
	//qc models
	//if (Stride >= 16 && vedesc.ByteWidth >= 14000000 && vedesc.ByteWidth <= 45354840)
	//al models
	//if (Stride == 28 && ((DWORD)ReturnAddress & 0x0000FFFF) == 0x00002d02) //0x78022d02
	{	
		//depth OFF for wallhack and chams
		if (Wallhack || ShaderChams || TextureChams)
		{
			if (rDEPTHBIASState) pContext->RSSetState(rDEPTHBIASState); // set depth bias to do the stencil check in this draw against previously written biased values with same bias
			if ((eDepthState)countRdepth >= 0 && (eDepthState)countRdepth <= 20) SetDepthStencilState((eDepthState)countRdepth); // read depth buffers with correct comparison func (LESS) but not write them (WRITE_MASK_ZERO)
		}
		if((eDepthState)countRdepth >= 5 && (eDepthState)countRdepth <= 20)
		SAFE_RELEASE(myDepthStencilStates[(eDepthState)countRdepth]);

		//shader chams
		if (ShaderChams)
		pContext->PSSetShader(psRed, NULL, NULL);

		//texture chams
		if (TextureChams)
		{
			pContext->PSSetShaderResources(0, 1, &texSRVred);
			pContext->PSSetSamplers(0, 1, &pSamplerState);
		}

	phookD3D11DrawIndexed(pContext, IndexCount, StartIndexLocation, BaseVertexLocation);

		//depth ON for wallhack and chams
		if (Wallhack || ShaderChams || TextureChams)
		{
			//orig depth on for 0
			if ((eDepthState)countEdepth == 0)
				pContext->OMSetDepthStencilState(origDepthStencilState, stencilRef); //orig

			if ((eDepthState)countEdepth >= 1 && (eDepthState)countEdepth <= 20) SetDepthStencilState((eDepthState)countEdepth); //custom
			if (rNORMALState) pContext->RSSetState(rNORMALState);
		}

		//shader chams
		if (ShaderChams)
		pContext->PSSetShader(psGreen, NULL, NULL);

		//texture chams
		if (TextureChams)
		{
			pContext->PSSetShaderResources(0, 1, &texSRVgreen);
			pContext->PSSetSamplers(0, 1, &pSamplerState);
		}
	
	phookD3D11DrawIndexed(pContext, IndexCount, StartIndexLocation, BaseVertexLocation); // render state is normal (not biased) here, so it should only draw Green when actually visible

		//depthbias OFF for wallhack and chams
		if (Wallhack || ShaderChams || TextureChams)
		if (rDEPTHBIASState) pContext->RSSetState(rDEPTHBIASState);

		pContext->OMSetRenderTargets(0, NULL, pDSV);

	phookD3D11DrawIndexed(pContext, IndexCount, StartIndexLocation, BaseVertexLocation); // finally, make sure the Red (anything) written before gets drawn no matter what by updating the current depth buffer with bias but not the rendertargets to not overdraw again
		
		pContext->OMSetRenderTargets(8, pRTV, pDSV);

		//depthbias ON for wallhack and chams
		if (Wallhack || ShaderChams || TextureChams)
		if (rNORMALState) pContext->RSSetState(rNORMALState);
	
	}

	//in qc stride 16 needs to be erased to make wallhack work
	//if (sOptions[0].Function == 1)
	//if (Stride == 16 && IndexCount > 120)
	//{
		//return;
	//}

	//small bruteforce logger
	if (ShowMenu)
	{
		if((countnum == pssrStartSlot)|| //optional: set countnum to something you want to test or log (pssrStartSlot for example) and use keys 0 and 9 to increase/decrease values
		(countStride == Stride || countIndexCount == IndexCount / 400) || (ReturnAddress != NULL && g_SelectedAddress != NULL && ReturnAddress == g_SelectedAddress))
			if (GetAsyncKeyState(VK_END) & 1)
				Log("Stride == %d && IndexCount == %d && indesc.ByteWidth == %d && vedesc.ByteWidth == %d && pscdesc.ByteWidth == %d && vscdesc.ByteWidth == %d && pssrStartSlot == %d && vscStartSlot == %d && countEdepth == %d && countRdepth == %d && ReturnAddress == 0x%X",
					Stride, IndexCount, indesc.ByteWidth, vedesc.ByteWidth, pscdesc.ByteWidth, vscdesc.ByteWidth, pssrStartSlot, vscStartSlot, countEdepth, countRdepth, ReturnAddress); //Descr.Format, Descr.Buffer.NumElements, texdesc.Format, texdesc.Height, texdesc.Width 
	}

	return phookD3D11DrawIndexed(pContext, IndexCount, StartIndexLocation, BaseVertexLocation);
}

//==========================================================================================================================

void __stdcall hookD3D11PSSetShaderResources(ID3D11DeviceContext* pContext, UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView *const *ppShaderResourceViews)
{
	pssrStartSlot = StartSlot;

	//reset settings
	if (ShowMenu)
	{
		//alt + f1 to toggle wallhack
		if (GetAsyncKeyState(VK_MENU) && GetAsyncKeyState(VK_F1) & 1)
			Wallhack = !Wallhack;

		if (GetAsyncKeyState(VK_MENU) && GetAsyncKeyState(VK_F2) & 1)
			ShaderChams = !ShaderChams;

		if (GetAsyncKeyState(VK_MENU) && GetAsyncKeyState(VK_F3) & 1)
			TextureChams = !TextureChams;

		if (GetAsyncKeyState(VK_HOME) & 1) //home/pos1
			ModelrecFinder = !ModelrecFinder;

		//hold down 6 key until a texture is wallhacked
		if (GetAsyncKeyState(VK_NEXT) & 1) //page down
			countStride--;
		if (GetAsyncKeyState(VK_PRIOR) & 1) //page up
			countStride++;

		//find SetDepthStencilState value (in front of walls)
		if (GetAsyncKeyState(0x35) & 1) //5-
		{
			countEdepth--;
			createdepthstencil = true;
		}
		if (GetAsyncKeyState(0x36) & 1) //6+
		{
			countEdepth++;
			createdepthstencil = true;
		}

		//find SetDepthStencilState value (behind walls)
		if (GetAsyncKeyState(0x37) & 1) //7-
		{
			countRdepth--;
			createdepthstencil = true;
		}

		if (GetAsyncKeyState(0x38) & 1) //8+
		{
			countRdepth++;
			createdepthstencil = true;
		}

		//increase/decrease countnum
		if (GetAsyncKeyState(0x39) & 1) //9-
			countnum--;
		
		if (GetAsyncKeyState(0x30) & 1) //0+
			countnum++;

		if (GetAsyncKeyState(VK_F10) & 1)
		{
			Wallhack = 0;
			ShaderChams = 0;
			TextureChams = 0;
			ModelrecFinder = 0;
			countStride = -1;
			countIndexCount = -1;
			countnum = -1;
			g_Index = -1;
		}
	}

	//make menu still usable if WndProc is slow or non-functional
	if (GetAsyncKeyState(VK_INSERT) & 1)
	{
		SaveCfg();
		ShowMenu = !ShowMenu;
	}

	//on alt tab
	if (GetAsyncKeyState(VK_MENU) && GetAsyncKeyState(VK_TAB) & 1)
		ShowMenu = false;
	if (GetAsyncKeyState(VK_TAB) && GetAsyncKeyState(VK_MENU) & 1)
		ShowMenu = false;


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

	scd.BufferDesc.Width = 1;
	scd.BufferDesc.Height = 1;
	scd.BufferDesc.RefreshRate.Numerator = 0;
	scd.BufferDesc.RefreshRate.Denominator = 1;

	UINT createFlags = 0;
#ifdef _DEBUG
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

	phookD3D11Present = (D3D11PresentHook)(DWORD_PTR*)pSwapChainVtable[8];
	phookD3D11ResizeBuffers = (D3D11ResizeBuffersHook)(DWORD_PTR*)pSwapChainVtable[13];
	phookD3D11PSSetShaderResources = (D3D11PSSetShaderResourcesHook)(DWORD_PTR*)pContextVTable[8];
	phookD3D11Draw = (D3D11DrawHook)(DWORD_PTR*)pContextVTable[13];
	phookD3D11DrawIndexed = (D3D11DrawIndexedHook)(DWORD_PTR*)pContextVTable[12];
	phookD3D11DrawIndexedInstanced = (D3D11DrawIndexedInstancedHook)(DWORD_PTR*)pContextVTable[20];
	//phookD3D11CreateQuery = (D3D11CreateQueryHook)(DWORD_PTR*)pDeviceVTable[24];

	DetourTransactionBegin();
	DetourUpdateThread(GetCurrentThread());
	DetourAttach(&(LPVOID&)phookD3D11Present, (PBYTE)hookD3D11Present);
	DetourAttach(&(LPVOID&)phookD3D11ResizeBuffers, (PBYTE)hookD3D11ResizeBuffers);
	DetourAttach(&(LPVOID&)phookD3D11PSSetShaderResources, (PBYTE)hookD3D11PSSetShaderResources);
	DetourAttach(&(LPVOID&)phookD3D11Draw, (PBYTE)hookD3D11Draw);
	DetourAttach(&(LPVOID&)phookD3D11DrawIndexed, (PBYTE)hookD3D11DrawIndexed);
	DetourAttach(&(LPVOID&)phookD3D11DrawIndexedInstanced, (PBYTE)hookD3D11DrawIndexedInstanced);
	//DetourAttach(&(LPVOID&)phookD3D11CreateQuery, (PBYTE)hookD3D11CreateQuery);
	DetourTransactionCommit();

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
		DetourTransactionBegin();
		DetourUpdateThread(GetCurrentThread());
		DetourDetach(&(LPVOID&)phookD3D11Present, (PBYTE)hookD3D11Present);
		DetourDetach(&(LPVOID&)phookD3D11ResizeBuffers, (PBYTE)hookD3D11ResizeBuffers);
		DetourDetach(&(LPVOID&)phookD3D11PSSetShaderResources, (PBYTE)hookD3D11PSSetShaderResources);
		DetourDetach(&(LPVOID&)phookD3D11Draw, (PBYTE)hookD3D11Draw);
		DetourDetach(&(LPVOID&)phookD3D11DrawIndexed, (PBYTE)hookD3D11DrawIndexed);
		DetourDetach(&(LPVOID&)phookD3D11DrawIndexedInstanced, (PBYTE)hookD3D11DrawIndexedInstanced);
		//DetourDetach(&(LPVOID&)phookD3D11CreateQuery, (PBYTE)hookD3D11CreateQuery);
		DetourTransactionCommit();
		break;
	}
	return TRUE;
}