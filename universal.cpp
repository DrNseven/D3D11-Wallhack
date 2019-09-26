//D3D11 wallhack by n7

#include <Windows.h>
#include <intrin.h>
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

HRESULT __stdcall hookD3D11ResizeBuffers(IDXGISwapChain *pSwapChain, UINT BufferCount, UINT Width, UINT Height, DXGI_FORMAT NewFormat, UINT SwapChainFlags)
{
	ImGui_ImplDX11_InvalidateDeviceObjects();
	if (nullptr != RenderTargetView) { RenderTargetView->Release(); RenderTargetView = nullptr; }

	HRESULT toReturn = phookD3D11ResizeBuffers(pSwapChain, BufferCount, Width, Height, NewFormat, SwapChainFlags);

	ImGui_ImplDX11_CreateDeviceObjects();
	
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
			//SwapChain = pSwapChain;
			pSwapChain->GetDevice(__uuidof(pDevice), (void**)&pDevice);
			pDevice->GetImmediateContext(&pContext);
		}
		
		//imgui
		DXGI_SWAP_CHAIN_DESC sd;
		pSwapChain->GetDesc(&sd);
		ImGui::CreateContext();
		ImGuiIO& io = ImGui::GetIO(); (void)io;
		ImGui::GetIO().WantCaptureMouse || ImGui::GetIO().WantTextInput || ImGui::GetIO().WantCaptureKeyboard; //control menu with mouse
		io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;  // Enable Keyboard Controls
		window = sd.OutputWindow;

		//wndprochandler
		OriginalWndProcHandler = (WNDPROC)SetWindowLongPtr(window, GWLP_WNDPROC, (LONG_PTR)hWndProc);

		ImGui_ImplWin32_Init(window);
		ImGui_ImplDX11_Init(pDevice, pContext);
		ImGui::GetIO().ImeWindowHandle = window;

		// Create depthstencil state
		D3D11_DEPTH_STENCIL_DESC depthStencilDesc;
		depthStencilDesc.DepthEnable = TRUE;
		depthStencilDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
		depthStencilDesc.DepthFunc = D3D11_COMPARISON_ALWAYS;
		depthStencilDesc.StencilEnable = FALSE;
		depthStencilDesc.StencilReadMask = 0xFF;
		depthStencilDesc.StencilWriteMask = 0xFF;
		// Stencil operations if pixel is front-facing
		depthStencilDesc.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
		depthStencilDesc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_INCR;
		depthStencilDesc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
		depthStencilDesc.FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
		// Stencil operations if pixel is back-facing
		depthStencilDesc.BackFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
		depthStencilDesc.BackFace.StencilDepthFailOp = D3D11_STENCIL_OP_DECR;
		depthStencilDesc.BackFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
		depthStencilDesc.BackFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
		pDevice->CreateDepthStencilState(&depthStencilDesc, &DepthStencilState_FALSE);

		//create depthbias rasterizer state
		D3D11_RASTERIZER_DESC rasterizer_desc;
		ZeroMemory(&rasterizer_desc, sizeof(rasterizer_desc));
		rasterizer_desc.FillMode = D3D11_FILL_SOLID;
		rasterizer_desc.CullMode = D3D11_CULL_NONE; //D3D11_CULL_FRONT;
		rasterizer_desc.FrontCounterClockwise = false;
		float bias = 1000.0f;
		float bias_float = static_cast<float>(-bias);
		bias_float /= 10000.0f;
		rasterizer_desc.DepthBias = DEPTH_BIAS_D32_FLOAT(*(DWORD*)&bias_float);
		rasterizer_desc.SlopeScaledDepthBias = 0.0f;
		rasterizer_desc.DepthBiasClamp = 0.0f;
		rasterizer_desc.DepthClipEnable = true;
		rasterizer_desc.ScissorEnable = false;
		rasterizer_desc.MultisampleEnable = false;
		rasterizer_desc.AntialiasedLineEnable = false;
		pDevice->CreateRasterizerState(&rasterizer_desc, &DEPTHBIASState_FALSE);
		
		//create normal rasterizer state
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
		pDevice->CreateRasterizerState(&nrasterizer_desc, &DEPTHBIASState_TRUE);
		
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

		ImGui::Begin("title", &greetings, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs);
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

	//mouse cursor on in menu
	if (ShowMenu)
		ImGui::GetIO().MouseDrawCursor = 1;
	else
		ImGui::GetIO().MouseDrawCursor = 0;

	//menu
	if (ShowMenu)
	{
		//ImGui::SetNextWindowPos(ImVec2(50.0f, 400.0f)); //pos
		ImGui::SetNextWindowSize(ImVec2(510.0f, 400.0f)); //size
		ImVec4 Bgcol = ImColor(0.0f, 0.4f, 0.28f, 0.8f); //bg color
		ImGui::PushStyleColor(ImGuiCol_WindowBg, Bgcol);
		ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.2f, 0.2f, 0.2f, 0.8f)); //frame color

		ImGui::Begin("Hack Menu");
		//ImGui::Checkbox("Wallhack Texture", &Wallhack);

		const char* Wallhack_Options[] = { "Off", "DepthStencil", "DepthBias" };
		ImGui::Text("Wallhack Texture");
		ImGui::SameLine();
		ImGui::Combo("##Wallhack", (int*)&Wallhack, Wallhack_Options, IM_ARRAYSIZE(Wallhack_Options));

		ImGui::Checkbox("Delete Texture ", &DeleteTexture); //the point is to highlight textures to see which we are logging
		ImGui::Checkbox("Modelrec Finder", &ModelrecFinder);

		if (ModelrecFinder)
		{
			ImGui::SliderInt("find Stride", &countStride, -1, 148);

			if (countIndexCount >= -1 && countIndexCount <= 147)
			{
				ImGui::SliderInt("find IndexCount", &countIndexCount, -1, 148);
			}
			else if(countIndexCount >= 148 && countIndexCount <= 295)
			{
				ImGui::SliderInt("find IndexCount", &countIndexCount, 149, 296);
			}
			else if (countIndexCount >= 296 && countIndexCount <= 443)
			{
				ImGui::SliderInt("find IndexCount", &countIndexCount, 297, 444);
			}
			else if (countIndexCount >= 444 && countIndexCount <= 591)
			{
				ImGui::SliderInt("find IndexCount", &countIndexCount, 445, 592);
			}
			else if (countIndexCount >= 592 && countIndexCount <= 739)
			{
				ImGui::SliderInt("find IndexCount", &countIndexCount, 593, 740);
			}
			else if (countIndexCount >= 740 && countIndexCount <= 887)
			{
				ImGui::SliderInt("find IndexCount", &countIndexCount, 741, 888);
			}
			else if (countIndexCount >= 888 && countIndexCount <= 1035)
			{
				ImGui::SliderInt("find IndexCount", &countIndexCount, 889, 1036);
				if (countIndexCount == 1036)
					countIndexCount = -1;
			}

			ImGui::SliderInt("find pscdesc.ByteWidth", &countpscdescByteWidth, -1, 148);
			ImGui::SliderInt("find indesc.ByteWidth", &countindescByteWidth, -1, 148);
			ImGui::SliderInt("find vedesc.ByteWidth", &countvedescByteWidth, -1, 148);

			ImGui::Text("ImGui Menu Navigation: ");
			ImGui::Text("Use TAB & Arrows or Mouse to navigate, Space to select options");
			ImGui::Text("Press F9 to log draw functions");
			ImGui::Text("Press END to log deleted textures");
			ImGui::Spacing();
			ImGui::Text("Hotkeys:");
			ImGui::Text("ALT + F1 toggles Wallhack");
			ImGui::Text("ALT + F2 toggles DeleteTexture");
			ImGui::Text("ALT + F3 toggles ModelrecFinder");
			ImGui::Text("Use Page Up/Down to find Stride");
			ImGui::Text("Use 7/8 to find IndexCount");
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


	//wallhack/chams
	if (Wallhack==1||Wallhack==2) //if wallhack option is enabled in menu
	//
	//ut4 model recognition example
	//if ((Stride == 32 && IndexCount == 10155)||(Stride == 44 && IndexCount == 11097)||(Stride == 40 && IndexCount == 11412)||(Stride == 40 && IndexCount == 11487)||(Stride == 44 && IndexCount == 83262)||(Stride == 40 && IndexCount == 23283))
	//if (Stride == 40 && pscdesc.ByteWidth == 256 && vscdesc.ByteWidth == 4096 && pssrStartSlot == 0) //swbf2 incomplete
	//_____________________________________________________________________________________________________________________________________________________________
	//Model recognition goes here, see your log.txt for the right Stride etc. You may have to do trial and error to see which values work best
		if ((countnum == pssrStartSlot || countStride == Stride || countIndexCount == IndexCount / 100 || countpscdescByteWidth == pscdesc.ByteWidth / 10 ||
			countindescByteWidth == indesc.ByteWidth / 1000 || countvedescByteWidth == vedesc.ByteWidth / 10000))
			//_____________________________________________________________________________________________________________________________________________________________
			//			
		{
			//get orig
			if (Wallhack == 1)
				pContext->OMGetDepthStencilState(&DepthStencilState_ORIG, 0); //get original

			//set off
			if(Wallhack==1)
				pContext->OMSetDepthStencilState(DepthStencilState_FALSE, 0); //depthstencil off

			//set off
			if(Wallhack==2)
				pContext->RSSetState(DEPTHBIASState_FALSE); //depthbias off

			phookD3D11DrawIndexed(pContext, IndexCount, StartIndexLocation, BaseVertexLocation); //redraw

			//restore orig
			if (Wallhack == 1)
				pContext->OMSetDepthStencilState(DepthStencilState_ORIG, 0); //depthstencil on

			//set on (we set true instead of restoring original to get alternative wallhack effect)
			if (Wallhack == 2)
				pContext->RSSetState(DEPTHBIASState_TRUE); //depthbias true

			//release
			if (Wallhack == 1)
				SAFE_RELEASE(DepthStencilState_ORIG); //release
		}

	//small bruteforce logger
	if (ShowMenu)
	{
		//how to log models:
		//run the game, inject dll, press insert for menu
		//0. press F9 to see which drawing function is called by the game
		//1. select DeleteTexture
		//2. select Stride and use the slider till an enemy model/texture disappears
		//3. press END to log the values of that model/texture to log.txt
		//4. add that Stride number to your model recognition, example if(Stride == 32)
		//5. next log IndexCount of that model Stride
		//6. add IndexCount to your model rec, example if(Stride == 32 && IndexCount == 10155)
		//7. and so on

		if ((countnum == pssrStartSlot || countStride == Stride || countIndexCount == IndexCount / 100 || countpscdescByteWidth == pscdesc.ByteWidth / 10 ||
			countindescByteWidth == indesc.ByteWidth / 1000 || countvedescByteWidth == vedesc.ByteWidth / 10000))
			if (GetAsyncKeyState(VK_END) & 1)
				Log("Stride == %d && IndexCount == %d && indesc.ByteWidth == %d && vedesc.ByteWidth == %d && pscdesc.ByteWidth == %d && vscdesc.ByteWidth == %d && pssrStartSlot == %d && vscStartSlot == %d",
					Stride, IndexCount, indesc.ByteWidth, vedesc.ByteWidth, pscdesc.ByteWidth, vscdesc.ByteWidth, pssrStartSlot, vscStartSlot);

		//log specific model
		//if (Stride == 40 && pscdesc.ByteWidth == 256 && vscdesc.ByteWidth == 4096 && pssrStartSlot == 0)
		//if (GetAsyncKeyState(VK_F10) & 1)
		//Log("Stride == %d && IndexCount == %d && indesc.ByteWidth == %d && vedesc.ByteWidth == %d && pscdesc.ByteWidth == %d && vscdesc.ByteWidth == %d && pssrStartSlot == %d && vscStartSlot == %d && Descr.Format == %d && Descr.Buffer.NumElements == %d && texdesc.Format == %d && texdesc.Height == %d && texdesc.Width == %d",
			//Stride, IndexCount, indesc.ByteWidth, vedesc.ByteWidth, pscdesc.ByteWidth, vscdesc.ByteWidth, pssrStartSlot, vscStartSlot, Descr.Format, Descr.Buffer.NumElements, texdesc.Format, texdesc.Height, texdesc.Width);

		//if delete texture is enabled in menu
		if (DeleteTexture)
			if ((countnum == pssrStartSlot || countStride == Stride || countIndexCount == IndexCount / 100 || countpscdescByteWidth == pscdesc.ByteWidth / 10 ||
				countindescByteWidth == indesc.ByteWidth / 1000 || countvedescByteWidth == vedesc.ByteWidth / 100000))
				return; //delete texture
	}
	return phookD3D11DrawIndexed(pContext, IndexCount, StartIndexLocation, BaseVertexLocation);
}

//==========================================================================================================================

void __stdcall hookD3D11DrawIndexedInstanced(ID3D11DeviceContext* pContext, UINT IndexCountPerInstance, UINT InstanceCount, UINT StartIndexLocation, INT BaseVertexLocation, UINT StartInstanceLocation)
{
	if (GetAsyncKeyState(VK_F9) & 1)
		Log("DrawIndexedInstanced called");

	//if game is drawing player models in DrawIndexedInstanced, do everything here instead (see code below)

	
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


	//wallhack/chams
	if (Wallhack==1||Wallhack==2) //if wallhack option is enabled in menu
	//
	//ut4 model recognition example
	//if ((Stride == 32 && IndexCount == 10155)||(Stride == 44 && IndexCount == 11097)||(Stride == 40 && IndexCount == 11412)||(Stride == 40 && IndexCount == 11487)||(Stride == 44 && IndexCount == 83262)||(Stride == 40 && IndexCount == 23283))
	//if (Stride == 40 && pscdesc.ByteWidth == 256 && vscdesc.ByteWidth == 4096 && pssrStartSlot == 0) //swbf2 incomplete
	//_____________________________________________________________________________________________________________________________________________________________
	//Model recognition goes here, see your log.txt for the right Stride etc. You may have to do trial and error to see which values work best
	if ( (countnum == pssrStartSlot || countStride == Stride || countIndexCount == IndexCountPerInstance / 100 || countpscdescByteWidth == pscdesc.ByteWidth / 10 || 
	countindescByteWidth == indesc.ByteWidth / 1000|| countvedescByteWidth == vedesc.ByteWidth / 10000) )
	//_____________________________________________________________________________________________________________________________________________________________
	//			
	{
		//get orig
		if (Wallhack == 1)
			pContext->OMGetDepthStencilState(&DepthStencilState_ORIG, 0); //get original

		//set off
		if (Wallhack == 1)
			pContext->OMSetDepthStencilState(DepthStencilState_FALSE, 0); //depthstencil off

		//set off
		if (Wallhack == 2)
			pContext->RSSetState(DEPTHBIASState_FALSE); //depthbias off

		phookD3D11DrawIndexedInstanced(pContext, IndexCountPerInstance, InstanceCount, StartIndexLocation, BaseVertexLocation, StartInstanceLocation); //redraw

		//restore orig
		if (Wallhack == 1)
			pContext->OMSetDepthStencilState(DepthStencilState_ORIG, 0); //depthstencil on

		//set on (we set true instead of restoring original to get alternative wallhack effect)
		if (Wallhack == 2)
			pContext->RSSetState(DEPTHBIASState_TRUE); //depthbias true

		//release
		if (Wallhack == 1)
			SAFE_RELEASE(DepthStencilState_ORIG); //release
	}

	//small bruteforce logger
	if (ShowMenu)
	{
		//how to log models:
		//run the game, inject dll, press insert for menu
		//0. press F9 to see which drawing function is called by the game
		//1. select DeleteTexture
		//2. select Stride and use the slider till an enemy model/texture disappears
		//3. press END to log the values of that model/texture to log.txt
		//4. add that Stride number to your model recognition, example if(Stride == 32)
		//5. next log IndexCount of that model Stride
		//6. add IndexCount to your model rec, example if(Stride == 32 && IndexCount == 10155)
		//7. and so on

		if ((countnum == pssrStartSlot || countStride == Stride || countIndexCount == IndexCountPerInstance / 100 || countpscdescByteWidth == pscdesc.ByteWidth / 10 ||
			countindescByteWidth == indesc.ByteWidth / 1000 || countvedescByteWidth == vedesc.ByteWidth / 10000))
			if (GetAsyncKeyState(VK_END) & 1)
				Log("Stride == %d && IndexCountPerInstance == %d && indesc.ByteWidth == %d && vedesc.ByteWidth == %d && pscdesc.ByteWidth == %d && vscdesc.ByteWidth == %d && pssrStartSlot == %d && vscStartSlot == %d",
					Stride, IndexCountPerInstance, indesc.ByteWidth, vedesc.ByteWidth, pscdesc.ByteWidth, vscdesc.ByteWidth, pssrStartSlot, vscStartSlot);

		//log specific model
		//if (Stride == 40 && pscdesc.ByteWidth == 256 && vscdesc.ByteWidth == 4096 && pssrStartSlot == 0)
		//if (GetAsyncKeyState(VK_F10) & 1)
		//Log("Stride == %d && IndexCountPerInstance == %d && indesc.ByteWidth == %d && vedesc.ByteWidth == %d && pscdesc.ByteWidth == %d && vscdesc.ByteWidth == %d && pssrStartSlot == %d && vscStartSlot == %d && Descr.Format == %d && Descr.Buffer.NumElements == %d && texdesc.Format == %d && texdesc.Height == %d && texdesc.Width == %d",
			//Stride, IndexCountPerInstance, indesc.ByteWidth, vedesc.ByteWidth, pscdesc.ByteWidth, vscdesc.ByteWidth, pssrStartSlot, vscStartSlot, Descr.Format, Descr.Buffer.NumElements, texdesc.Format, texdesc.Height, texdesc.Width);

		//if delete texture is enabled in menu
		if(DeleteTexture)
		if ((countnum == pssrStartSlot || countStride == Stride || countIndexCount == IndexCountPerInstance / 100 || countpscdescByteWidth == pscdesc.ByteWidth / 10 ||
			countindescByteWidth == indesc.ByteWidth / 1000 || countvedescByteWidth == vedesc.ByteWidth / 100000))
			return; //delete texture
	}
	
	return phookD3D11DrawIndexedInstanced(pContext, IndexCountPerInstance, InstanceCount, StartIndexLocation, BaseVertexLocation, StartInstanceLocation);
}

//==========================================================================================================================

void __stdcall hookD3D11PSSetShaderResources(ID3D11DeviceContext* pContext, UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView *const *ppShaderResourceViews)
{
	pssrStartSlot = StartSlot;

	//make menu still usable if WndProc is slow or non-functional
	if (GetAsyncKeyState(VK_INSERT) & 1)
	{
		SaveCfg();
		ShowMenu = !ShowMenu;
	}

	//hotkeys
	if (ShowMenu)
	{
		//alt + f1 to toggle wallhack
		//if (GetAsyncKeyState(VK_MENU) && GetAsyncKeyState(VK_F1) & 1)
			//Wallhack = !Wallhack;

		//alt + f1 to toggle three options off, wallhack1, wallhack2
		if (GetAsyncKeyState(VK_MENU) && GetAsyncKeyState(VK_F1) & 1)
		{
			Wallhack++;
			if (Wallhack > 2) Wallhack = 0;
		}

		if (GetAsyncKeyState(VK_MENU) && GetAsyncKeyState(VK_F2) & 1)
			DeleteTexture = !DeleteTexture;

		if (GetAsyncKeyState(VK_MENU) && GetAsyncKeyState(VK_F3) & 1)
			ModelrecFinder = !ModelrecFinder;

		//hold down pageup key until a texture changes
		if (GetAsyncKeyState(VK_NEXT) & 1) //page down
			countStride--;
		if (GetAsyncKeyState(VK_PRIOR) & 1) //page up
			countStride++;

		if (GetAsyncKeyState(0x37) & 1) //7-
			countIndexCount--;
		if (GetAsyncKeyState(0x38) & 1) //8+
			countIndexCount++;
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
	return phookD3D11PSSetShaderResources(pContext, StartSlot, NumViews, ppShaderResourceViews);
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