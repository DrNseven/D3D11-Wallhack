//d3d11 wallhack (main.h)

//==========================================================================================================================

//features deafult settings
bool Wallhack=true;
bool ShaderChams = false;
bool TextureChams = false;
bool ModelrecFinder = true;

//init only once
bool firstTime = true; 
bool createdepthstencil = true;

//viewport
UINT vps = 1;
D3D11_VIEWPORT viewport;
float ScreenCenterX;
float ScreenCenterY;

//create texture
ID3D11Texture2D* texGreen = nullptr;
ID3D11Texture2D* texRed = nullptr;

//create shaderresourceview
ID3D11ShaderResourceView* texSRVgreen;
ID3D11ShaderResourceView* texSRVred;

//create samplerstate
ID3D11SamplerState *pSamplerState;
ID3D11SamplerState *pSamplerState2;

//create rendertarget
ID3D11RenderTargetView* RenderTargetView = NULL;

//get rendertarget
ID3D11RenderTargetView *pRTV[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT];
ID3D11DepthStencilView *pDSV;

//shader
ID3D11PixelShader* psRed = NULL;
ID3D11PixelShader* psGreen = NULL;

//vertex
ID3D11Buffer *veBuffer;
UINT Stride;
UINT veBufferOffset;
D3D11_BUFFER_DESC vedesc;

//index
ID3D11Buffer *inBuffer;
DXGI_FORMAT inFormat;
UINT        inOffset;
D3D11_BUFFER_DESC indesc;

//psgetConstantbuffers
UINT pscStartSlot;
ID3D11Buffer *pscBuffer;
D3D11_BUFFER_DESC pscdesc;

//vsgetconstantbuffers
UINT vscStartSlot;
ID3D11Buffer *vscBuffer;
D3D11_BUFFER_DESC vscdesc;

//pssetshaderresources
UINT pssrStartSlot;
ID3D11Resource *Resource;
D3D11_SHADER_RESOURCE_VIEW_DESC Descr;
D3D11_TEXTURE2D_DESC texdesc;

//return address
void* ReturnAddress;

//wndproc
HWND window = nullptr;
bool ShowMenu = false;
static WNDPROC OriginalWndProcHandler = nullptr;

//logger, misc
bool logger = false;
int countnum = -1;
int countStride = -1;
int countIndexCount = -1;
int countEdepth = 0; //1al, 11ut4, 11ssf, 0qc
int countRdepth = 2; //3al, 12ut4, 4ssf, 2qc
wchar_t reportValue[256];
#define SAFE_RELEASE(x) if (x) { x->Release(); x = NULL; }
HRESULT hr;
bool greetings = true;

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

int									g_Index = -1;
std::vector<void*>					g_Vector;
void*								g_SelectedAddress = NULL;

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

//generate shader func
HRESULT GenerateShader(ID3D11Device* pD3DDevice, ID3D11PixelShader** pShader, float r, float g, float b)
{
	char szCast[] = "struct VS_OUT"
		"{"
		" float4 Position : SV_Position;"
		" float4 Color : COLOR0;"
		"};"

		"float4 main( VS_OUT input ) : SV_Target"
		"{"
		" float4 fake;"
		" fake.a = 1.0f;"
		" fake.r = %f;"
		" fake.g = %f;"
		" fake.b = %f;"
		" return fake;"
		"}";

	ID3D10Blob* pBlob;
	char szPixelShader[1000];

	sprintf_s(szPixelShader, szCast, r, g, b);

	ID3DBlob* d3dErrorMsgBlob;

	HRESULT hr = D3DCompile(szPixelShader, sizeof(szPixelShader), "shader", NULL, NULL, "main", "ps_4_0", NULL, NULL, &pBlob, &d3dErrorMsgBlob);

	if (FAILED(hr))
		return hr;

	hr = pD3DDevice->CreatePixelShader((DWORD*)pBlob->GetBufferPointer(), pBlob->GetBufferSize(), NULL, pShader);

	if (FAILED(hr))
		return hr;

	return S_OK;
}

//==========================================================================================================================

//orig
UINT stencilRef = 0;
D3D11_DEPTH_STENCIL_DESC origdsd;
ID3D11DepthStencilState* origDepthStencilState = NULL;

//wh
enum eDepthState
{
	ORIGINAL,
	ENABLED,
	DISABLED,
	READ_NO_WRITE,
	NO_READ_NO_WRITE,

	ENABLED1,
	ENABLED2,
	ENABLED3,
	ENABLED4,
	ENABLED5,
	ENABLED6,
	ENABLED7,
	ENABLED8,

	READ_NO_WRITE1,
	READ_NO_WRITE2,
	READ_NO_WRITE3,
	READ_NO_WRITE4,
	READ_NO_WRITE5,
	READ_NO_WRITE6,
	READ_NO_WRITE7,
	READ_NO_WRITE8,

	_DEPTH_COUNT
};

int EDepth = (eDepthState)countEdepth;
int RDepth = (eDepthState)countRdepth;

ID3D11DepthStencilState* myDepthStencilStates[static_cast<int>(eDepthState::_DEPTH_COUNT)];

void SetDepthStencilState(eDepthState aState)
{
	pContext->OMSetDepthStencilState(myDepthStencilStates[aState], 1);
}

//wire
char *state;
ID3D11RasterizerState* rDEPTHBIASState;
ID3D11RasterizerState* rNORMALState;
ID3D11RasterizerState* rWIREFRAMEState;
ID3D11RasterizerState* rSOLIDState;
#define DEPTH_BIAS_D32_FLOAT(d) (d/(1/pow(2,23)))

//==========================================================================================================================

#include <string>
#include <fstream>
//save cfg
void SaveCfg()
{
	ofstream fout;
	fout.open(GetDirectoryFile("d3dwh.ini"), ios::trunc);
	fout << "Wallhack " << Wallhack << endl;
	fout << "ShaderChams " << ShaderChams << endl;
	fout << "TextureChams " << TextureChams << endl;
	fout << "EDepth " << countEdepth << endl;
	fout << "RDepth " << countRdepth << endl;
	fout << "ModelrecFinder " << ModelrecFinder << endl;
	fout.close();
}

//load cfg
void LoadCfg()
{
	ifstream fin;
	string Word = "";
	fin.open(GetDirectoryFile("d3dwh.ini"), ifstream::in);
	fin >> Word >> Wallhack;
	fin >> Word >> ShaderChams;
	fin >> Word >> TextureChams;
	fin >> Word >> EDepth;
	fin >> Word >> RDepth;
	fin >> Word >> ModelrecFinder;
	fin.close();
}

//create rendertarget
void CreateRenderTarget()
{
	DXGI_SWAP_CHAIN_DESC sd;
	SwapChain->GetDesc(&sd);
	ID3D11Texture2D* pBackBuffer;
	D3D11_RENDER_TARGET_VIEW_DESC render_target_view_desc;
	ZeroMemory(&render_target_view_desc, sizeof(render_target_view_desc));
	render_target_view_desc.Format = sd.BufferDesc.Format;
	render_target_view_desc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
	hr = SwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&pBackBuffer);
	if (FAILED(hr)) { Log("Failed to get BackBuffer"); }
	hr = pDevice->CreateRenderTargetView(pBackBuffer, &render_target_view_desc, &RenderTargetView);
	pBackBuffer->Release();
	if (FAILED(hr)) { Log("Failed to get RenderTarget"); }
}

//cleanup rendertarget
void CleanupRenderTarget()
{
	if (nullptr != RenderTargetView)
	{
		RenderTargetView->Release();
		RenderTargetView = nullptr;
	}
}

//create depthstencil states
void CreateDepthStencilStates()
{
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
	ZeroMemory(&stencilDesc7, sizeof(D3D11_DEPTH_STENCIL_DESC));
	//stencilDesc7.DepthEnable = true;
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
	ZeroMemory(&stencilDesc8, sizeof(D3D11_DEPTH_STENCIL_DESC));
	//stencilDesc8.DepthEnable = false;
	stencilDesc8.DepthFunc = (D3D11_COMPARISON_FUNC)8;//5
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
}