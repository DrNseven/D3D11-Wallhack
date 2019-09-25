//d3d11 wallhack (main.h)

//==========================================================================================================================

//features deafult settings
bool ModelrecFinder = true;
bool Wallhack=true;
bool DeleteTexture = true;

//init only once
bool firstTime = true; 

//wh
ID3D11DepthStencilState* DepthStencilState_ORIG = NULL; //depth on
ID3D11DepthStencilState* DepthStencilState_FALSE = NULL; //depth off

//viewport
UINT vps = 1;
D3D11_VIEWPORT viewport;
float ScreenCenterX;
float ScreenCenterY;

//create rendertarget
ID3D11RenderTargetView* RenderTargetView = NULL;

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

//wndproc
HWND window = nullptr;
bool ShowMenu = false;
static WNDPROC OriginalWndProcHandler = nullptr;

//logger, misc
bool logger = false;
int countnum = -1;
int countStride = -1;
int countIndexCount = -1;
int countpscdescByteWidth = -1;
int countindescByteWidth = -1;
int countvedescByteWidth = -1;

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

//==========================================================================================================================

#include <string>
#include <fstream>
//save cfg
void SaveCfg()
{
	ofstream fout;
	fout.open(GetDirectoryFile("d3dwh.ini"), ios::trunc);
	fout << "Wallhack " << Wallhack << endl;
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
	fin >> Word >> ModelrecFinder;
	fin.close();
}