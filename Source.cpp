#pragma comment(linker,"\"/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

#include <windows.h>
#include <atlbase.h>
#include <atlhost.h>
#include <string>
#include <vector>
#include <winternl.h>
#include "resource.h"
#include "ChatData.h"
#include "CmdProcess.h"

#define DEFAULT_DPI 96
#define SCALEX(X) MulDiv(X, uDpiX, DEFAULT_DPI)
#define SCALEY(Y) MulDiv(Y, uDpiY, DEFAULT_DPI)
#define POINT2PIXEL(PT) MulDiv(PT, uDpiY, 72)

TCHAR szClassName[] = TEXT("Window");

BOOL GetScaling(HWND hWnd, UINT* pnX, UINT* pnY)
{
	BOOL bSetScaling = FALSE;
	const HMONITOR hMonitor = MonitorFromWindow(hWnd, MONITOR_DEFAULTTONEAREST);
	if (hMonitor)
	{
		HMODULE hShcore = LoadLibraryW(L"SHCORE");
		if (hShcore)
		{
			typedef HRESULT __stdcall GetDpiForMonitor(HMONITOR, int, UINT*, UINT*);
			GetDpiForMonitor* fnGetDpiForMonitor = reinterpret_cast<GetDpiForMonitor*>(GetProcAddress(hShcore, "GetDpiForMonitor"));
			if (fnGetDpiForMonitor)
			{
				UINT uDpiX, uDpiY;
				if (SUCCEEDED(fnGetDpiForMonitor(hMonitor, 0, &uDpiX, &uDpiY)) && uDpiX > 0 && uDpiY > 0)
				{
					*pnX = uDpiX;
					*pnY = uDpiY;
					bSetScaling = TRUE;
				}
			}
			FreeLibrary(hShcore);
		}
	}
	if (!bSetScaling)
	{
		HDC hdc = GetDC(NULL);
		if (hdc)
		{
			*pnX = GetDeviceCaps(hdc, LOGPIXELSX);
			*pnY = GetDeviceCaps(hdc, LOGPIXELSY);
			ReleaseDC(NULL, hdc);
			bSetScaling = TRUE;
		}
	}
	if (!bSetScaling)
	{
		*pnX = DEFAULT_DPI;
		*pnY = DEFAULT_DPI;
		bSetScaling = TRUE;
	}
	return bSetScaling;
}

std::wstring HtmlEncode(const std::wstring& text)
{
	std::wstring html;

	for (auto it = text.cbegin(); it != text.cend(); ++it)
	{
		switch (*it)
		{
		case L'>':
			html += L"&gt;";
			break;

		case L'<':
			html += L"&lt;";
			break;

		case L'&':
			html += L"&amp;";
			break;

		default:
			html += *it;
		}
	}

	return html;
}

VOID execBrowserCommand(IHTMLDocument2* pDocument, LPCWSTR lpszCommand)
{
	VARIANT var = { 0 };
	VARIANT_BOOL varBool = { 0 };
	var.vt = VT_EMPTY;
	BSTR command = SysAllocString(lpszCommand);
	pDocument->execCommand(command, VARIANT_FALSE, var, &varBool);
	SysFreeString(command);
}

VOID ScrollBottom(IHTMLDocument2* pDocument)
{
	IHTMLWindow2* pHtmlWindow2;
	pDocument->get_parentWindow(&pHtmlWindow2);
	if (pHtmlWindow2) {
		pHtmlWindow2->scrollTo(0, SHRT_MAX);
		pHtmlWindow2->Release();
	}
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	static CComQIPtr<IWebBrowser2>pWB2;
	static CComQIPtr<IHTMLDocument2>pDocument;
	static HWND hEdit, hBrowser;
	static HBRUSH hBrush;
	static HFONT hFont;
	static UINT uDpiX = DEFAULT_DPI, uDpiY = DEFAULT_DPI;
	switch (msg)
	{
	case WM_CREATE:
		CmdProcess::Get()->Create(hWnd);
		hBrush = CreateSolidBrush(RGB(200, 219, 249));
		hEdit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", 0, WS_VISIBLE | WS_CHILD | ES_AUTOHSCROLL, 0, 0, 0, 0, hWnd, 0, ((LPCREATESTRUCT)lParam)->hInstance, 0);
		SendMessageW(hEdit, EM_SETCUEBANNER, TRUE, (LPARAM)L"コマンドを入力");
		hBrowser = CreateWindowExW(0, L"AtlAxWin140", L"about:blank", WS_CHILD | WS_VISIBLE | WS_VSCROLL, 0, 0, 0, 0, hWnd, 0, ((LPCREATESTRUCT)lParam)->hInstance, 0);
		{
			CComPtr<IUnknown> punkIE;
			if (AtlAxGetControl(hBrowser, &punkIE) == S_OK) {
				pWB2 = punkIE;
				punkIE.Release();
				if (pWB2) {
					pWB2->put_Silent(VARIANT_TRUE);
					pWB2->put_RegisterAsDropTarget(VARIANT_FALSE);
					pWB2->get_Document((IDispatch**)&pDocument);
					{
						CComPtr<IUnknown> punkIE;
						ATL::CComPtr<IAxWinAmbientDispatch> ambient;
						AtlAxGetHost(hBrowser, &punkIE);
						ambient = punkIE;
						if (ambient) {
							ambient->put_AllowContextMenu(VARIANT_FALSE);
						}
					}
				}
			}
		}
		if (!pDocument) {
			return -1;
		}
		{
			WCHAR szModuleFilePath[MAX_PATH] = { 0 };
			GetModuleFileNameW(NULL, szModuleFilePath, MAX_PATH);
			WCHAR szURL[MAX_PATH] = { 0 };
			wsprintfW(szURL, L"res://%s/%d", szModuleFilePath, IDR_HTML1);
			BSTR url = SysAllocString(szURL);
			pWB2->Navigate(url, NULL, NULL, NULL, NULL);
			SysFreeString(url);
		}
		SendMessage(hWnd, WM_APP + 1, 0, 0);
		RECT rect;
		GetWindowRect(hWnd, &rect);
		SetWindowPos(hWnd, NULL, 0, 0, (rect.right - rect.left) / 3, (rect.bottom - rect.top) / 2, SWP_NOMOVE | SWP_NOZORDER | SWP_NOSENDCHANGING | SWP_NOREDRAW);
		break;
	case WM_SIZE:
		MoveWindow(hBrowser, 0, 0, LOWORD(lParam), HIWORD(lParam) - POINT2PIXEL(32), TRUE);
		MoveWindow(hEdit, 0, HIWORD(lParam) - POINT2PIXEL(32), LOWORD(lParam), POINT2PIXEL(32), TRUE);
		break;
	case WM_CTLCOLOREDIT:
	case WM_CTLCOLORSTATIC:
		SetBkMode((HDC)wParam, TRANSPARENT);
		return(INT_PTR)hBrush;
	case WM_SETFOCUS:
		SetFocus(hEdit);
		break;
	case WM_APP:
		{
			std::wstring output;
			while (ChatData::Get()->PopFrontOutput(&output)) {
				const std::wstring outputHtml = HtmlEncode(output);
				std::wstring html;
				html += L"<div class=\"result\"><div class=\"icon\"><img></div><div class=\"chatting\"><div class=\"output\"><pre>";
				html += outputHtml;
				html += L"</pre></div></div></div>";

				CComQIPtr<IHTMLElement>pElementBody;
				pDocument->get_body(&pElementBody);
				if (pElementBody) {
					BSTR whereBStr = SysAllocString(L"beforeEnd");
					BSTR htmlBStr = SysAllocString(html.c_str());
					pElementBody->insertAdjacentHTML(whereBStr, htmlBStr);
					SysFreeString(whereBStr);
					SysFreeString(htmlBStr);
					pElementBody.Release();
					ScrollBottom(pDocument);
				}
			}
		}
		EnableWindow(hBrowser, TRUE);
		EnableWindow(hEdit, TRUE);
		SetFocus(hEdit);
		break;
	case WM_COMMAND:
		if (LOWORD(wParam) == ID_RUN)
		{
			if (GetFocus() != hEdit) {
				SetFocus(hEdit);
			}
			const int nTextLength = GetWindowTextLength(hEdit);
			if (nTextLength > 0) {
				std::vector<wchar_t> vecCommand(nTextLength + 1, L'\0');
				GetWindowTextW(hEdit, &vecCommand.front(), nTextLength + 1);
				const std::wstring command = &vecCommand.front();
				const std::wstring commandHtml = HtmlEncode(command);
				std::wstring html;
				html += L"<div class=\"input\"><pre>";
				html += commandHtml;
				html += L"</pre></div>";

				CComQIPtr<IHTMLElement>pElementBody;
				pDocument->get_body(&pElementBody);
				if (pElementBody) {
					BSTR whereBStr = SysAllocString(L"beforeEnd");
					BSTR htmlBStr = SysAllocString(html.c_str());
					pElementBody->insertAdjacentHTML(whereBStr, htmlBStr);
					SysFreeString(whereBStr);
					SysFreeString(htmlBStr);
					pElementBody.Release();
					ScrollBottom(pDocument);
				}
				SetWindowTextW(hEdit, 0);
				EnableWindow(hBrowser, FALSE);
				EnableWindow(hEdit, FALSE);

				CmdProcess::Get()->RunCommand(command.c_str());
			}
		}
		else if (LOWORD(wParam) == ID_COPY) {
			if (GetFocus() == hEdit) {
				SendMessage(hEdit, WM_COPY, 0, 0);
			} else {
				execBrowserCommand(pDocument, L"Copy");
				execBrowserCommand(pDocument, L"Unselect");
				SetFocus(hEdit);
			}
		}
		else if (LOWORD(wParam) == ID_TAB) {
			execBrowserCommand(pDocument, L"Unselect");
			SendMessage(hEdit, EM_SETSEL, 0, -1);
			SetFocus(hEdit);
		}
		break;
	case WM_NCCREATE:
		{
			const HMODULE hModUser32 = GetModuleHandleW(L"user32.dll");
			if (hModUser32)
			{
				typedef BOOL(WINAPI* fnTypeEnableNCScaling)(HWND);
				const fnTypeEnableNCScaling fnEnableNCScaling = (fnTypeEnableNCScaling)GetProcAddress(hModUser32, "EnableNonClientDpiScaling");
				if (fnEnableNCScaling)
				{
					fnEnableNCScaling(hWnd);
				}
			}
		}
		return DefWindowProc(hWnd, msg, wParam, lParam);
	case WM_DPICHANGED:
	case WM_APP+1:
		GetScaling(hWnd, &uDpiX, &uDpiY);
		DeleteObject(hFont);
		hFont = CreateFontW(-POINT2PIXEL(12), 0, 0, 0, FW_NORMAL, 0, 0, 0, ANSI_CHARSET, 0, 0, 0, 0, L"Consolas");
		SendMessage(hEdit, WM_SETFONT, (WPARAM)hFont, 0);
		break;
	case WM_CLOSE:
		DestroyWindow(hWnd);
		break;
	case WM_DESTROY:
		pDocument.Release();
		pWB2.Release();
		DeleteObject(hBrush);
		DeleteObject(hFont);
		CmdProcess::Get()->Exit();
		PostQuitMessage(0);
		break;
	default:
		return DefDlgProc(hWnd, msg, wParam, lParam);
	}
	return 0;
}

int WINAPI wWinMain(
	_In_ HINSTANCE hInstance,
	_In_opt_ HINSTANCE hPrevInstance,
	_In_ LPWSTR lpCmdLine,
	_In_ int nShowCmd
)
{
	if (FAILED(CoInitialize(NULL)))
	{
		return 0;
	}
	::AtlAxWinInit();
	MSG msg = { 0 };
	{
		CComModule _Module;
		WNDCLASS wndclass = {
			0,
			WndProc,
			0,
			DLGWINDOWEXTRA,
			hInstance,
			LoadIcon(hInstance,(LPCTSTR)IDI_ICON1),
			0,
			0,
			0,
			szClassName
		};
		RegisterClass(&wndclass);
		HWND hWnd = CreateWindow(
			szClassName,
			TEXT("コマンド プロンプト (チャット風)"),
			WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
			CW_USEDEFAULT,
			0,
			CW_USEDEFAULT,
			0,
			0,
			0,
			hInstance,
			0
		);
		ShowWindow(hWnd, SW_SHOWDEFAULT);
		UpdateWindow(hWnd);
		HACCEL hAccel = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDR_ACCELERATOR1));
		while (GetMessage(&msg, 0, 0, 0))
		{
			if (!TranslateAccelerator(hWnd, hAccel, &msg) && !IsDialogMessage(hWnd, &msg))
			{
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}
		}
	}
	::AtlAxWinTerm();
	CoUninitialize();
	return (int)msg.wParam;
}
