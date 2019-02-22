//d3d11 wallhack

//DX Includes
#include <DirectXMath.h>
using namespace DirectX;

//==========================================================================================================================

//features deafult settings
int Folder1 = 1;
int Item1 = 1; //sOptions[0].Function //wallhack
int Item2 = 1; //sOptions[1].Function //chams
int Item3 = 0; //countEdepth
int Item4 = 2; //countRdepth

//init only once
bool firstTime = true; 

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

//pssetsamplers
UINT pssStartSlot;

//return address
void* ReturnAddress;

//logger, misc
bool logger = false;
UINT countnum = -1;
UINT countEdepth = 0; //1al, 11ut4, 11ssf, 0qc
UINT countRdepth = 2; //3al, 12ut4, 4ssf, 2qc
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
	/*
	//texture sample chams bright
	const char szCast[] =
		"struct PS_INPUT\
			{\
			float4 pos : SV_POSITION;\
			float4 col : COLOR0;\
			float2 uv  : TEXCOORD0;\
			};\
			sampler sampler0;\
			Texture2D texture0;\
			\
			float4 main(PS_INPUT input) : SV_Target\
			{\
			float4 out_col = input.col.bgra + texture0.Sample(sampler0, input.uv); \
			out_col.g = %f; \
			out_col.a = 1.0f; \
			return out_col; \
			}";
	*/

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

//menu
#define MAX_ITEMS 25

#define T_FOLDER 1
#define T_OPTION 2
#define T_ONETWOOPTION 3

#define LineH 15

struct Options {
	LPCWSTR Name;
	int	Function;
	BYTE Type;
};

struct Menu {
	LPCWSTR Title;
	int x;
	int y;
	int w;
};

DWORD Color_Font;
DWORD Color_On;
DWORD Color_Off;
DWORD Color_Folder;
DWORD Color_Current;

bool Is_Ready, Visible;
int Items, Cur_Pos;

Options sOptions[MAX_ITEMS];
Menu sMenu;

#include <string>
#include <fstream>
void SaveCfg()
{
	ofstream fout;
	fout.open(GetDirectoryFile("d3dwh.ini"), ios::trunc);
	fout << "Item1 " << sOptions[0].Function << endl;
	fout << "Item2 " << sOptions[1].Function << endl;
	fout << "Item3 " << countEdepth << endl;
	fout << "Item4 " << countRdepth << endl;
	fout.close();
}

void LoadCfg()
{
	ifstream fin;
	string Word = "";
	fin.open(GetDirectoryFile("d3dwh.ini"), ifstream::in);
	fin >> Word >> Item1;
	fin >> Word >> Item2;
	fin >> Word >> Item3;
	fin >> Word >> Item4;
	fin.close();
}

void JBMenu(void)
{
	Visible = true;
}

void Init_Menu(ID3D11DeviceContext *pContext, LPCWSTR Title, int x, int y)
{
	Is_Ready = true;
	sMenu.Title = Title;
	sMenu.x = x;
	sMenu.y = y;
}

void AddFolder(LPCWSTR Name, int Pointer)
{
	sOptions[Items].Name = (LPCWSTR)Name;
	sOptions[Items].Function = Pointer;
	sOptions[Items].Type = T_FOLDER;
	Items++;
}

void AddOption(LPCWSTR Name, int Pointer, int *Folder)
{
	if (*Folder == 0)
		return;
	sOptions[Items].Name = Name;
	sOptions[Items].Function = Pointer;
	sOptions[Items].Type = T_OPTION;
	Items++;
}

void AddOneTwoOption(LPCWSTR Name, int Pointer, int *Folder)
{
	if (*Folder == 0)
		return;
	sOptions[Items].Name = Name;
	sOptions[Items].Function = Pointer;
	sOptions[Items].Type = T_ONETWOOPTION;
	Items++;
}

void Navigation()
{
	if (GetAsyncKeyState(VK_INSERT) & 1)
	{
		SaveCfg(); //save settings
		Visible = !Visible;
	}

	if (!Visible)
		return;
	
	int value = 0;

	if (GetAsyncKeyState(VK_DOWN) & 1)
	{
		Cur_Pos++;
		if (sOptions[Cur_Pos].Name == 0)
			Cur_Pos--;
	}

	if (GetAsyncKeyState(VK_UP) & 1)
	{
		Cur_Pos--;
		if (Cur_Pos == -1)
			Cur_Pos++;
	}

	else if (sOptions[Cur_Pos].Type == T_OPTION && GetAsyncKeyState(VK_RIGHT) & 1)
	{
		if (sOptions[Cur_Pos].Function == 0)
			value++;
	}

	else if (sOptions[Cur_Pos].Type == T_OPTION && (GetAsyncKeyState(VK_LEFT) & 1) && sOptions[Cur_Pos].Function == 1)
	{
		value--;
	}

	else if (sOptions[Cur_Pos].Type == T_ONETWOOPTION && GetAsyncKeyState(VK_RIGHT) & 1 && sOptions[Cur_Pos].Function <= 1)//max
	{
		value++;
	}

	else if (sOptions[Cur_Pos].Type == T_ONETWOOPTION && (GetAsyncKeyState(VK_LEFT) & 1) && sOptions[Cur_Pos].Function != 0)
	{
		value--;
	}

	if (value) {
		sOptions[Cur_Pos].Function += value;
		if (sOptions[Cur_Pos].Type == T_FOLDER)
		{
			memset(&sOptions, 0, sizeof(sOptions));
			Items = 0;
		}
	}
	
}

bool IsReady()
{
	if (Items)
		return true;
	return false;
}

void DrawTextF(ID3D11DeviceContext* pContext, LPCWSTR text, int FontSize, int x, int y, DWORD Col)
{
	if (Is_Ready == false)
		MessageBoxA(0, "Error, you dont initialize the menu!", "Error", MB_OK);

	if (pFontWrapper)
		pFontWrapper->DrawString(pContext, text, (float)FontSize, (float)x, (float)y, Col, FW1_RESTORESTATE);
}

void Draw_Menu()
{
	if (!Visible)
		return;

	DrawTextF(pContext, sMenu.Title, 14, sMenu.x + 10, sMenu.y, Color_Font);
	for (int i = 0; i < Items; i++)
	{
		if (sOptions[i].Type == T_OPTION)
		{
			if (sOptions[i].Function)
			{
				DrawTextF(pContext, L"On", 14, sMenu.x + 150, sMenu.y + LineH * (i + 2), Color_On);
			}
			else {
				DrawTextF(pContext, L"Off", 14, sMenu.x + 150, sMenu.y + LineH * (i + 2), Color_Off);
			}
		}

		if (sOptions[i].Type == T_ONETWOOPTION)
		{
			if (sOptions[i].Function == 0)
				DrawTextF(pContext, L"Off", 14, sMenu.x + 150, sMenu.y + LineH * (i + 2), Color_Off);

			if (sOptions[i].Function == 1)
				DrawTextF(pContext, L"Shader Chams", 14, sMenu.x + 150, sMenu.y + LineH * (i + 2), Color_On);

			if (sOptions[i].Function == 2)
				DrawTextF(pContext, L"Texture Chams", 14, sMenu.x + 150, sMenu.y + LineH * (i + 2), Color_On);
		}

		if (sOptions[i].Type == T_FOLDER)
		{
			if (sOptions[i].Function)
			{
				DrawTextF(pContext, L"Open", 14, sMenu.x + 150, sMenu.y + LineH * (i + 2), Color_Folder);
			}
			else {
				DrawTextF(pContext, L"Closed", 14, sMenu.x + 150, sMenu.y + LineH * (i + 2), Color_Folder);
			}
		}
		DWORD Color = Color_Font;
		if (Cur_Pos == i)
			Color = Color_Current;
		DrawTextF(pContext, sOptions[i].Name, 14, sMenu.x + 6, sMenu.y + 1 + LineH * (i + 2), 0xFF2F4F4F);
		DrawTextF(pContext, sOptions[i].Name, 14, sMenu.x + 5, sMenu.y + LineH * (i + 2), Color);

	}
}

void Do_Menu()
{
	AddOption(L"Wallhack", Item1, &Folder1);
	AddOneTwoOption(L"Chams", Item2, &Folder1);
}