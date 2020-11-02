#pragma comment(linker,"\"/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

#include <windows.h>
#include <atlbase.h>
#include <atlhost.h>
#include <string>
#include <winternl.h>
#include "resource.h"
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

LPWSTR RunCommand(LPCWSTR lpszCommand)
{
	return CmdProcess::Get()->RunCommand(lpszCommand);
}

LPWSTR HtmlEncode(LPCWSTR lpText)
{
	int i, m;
	LPWSTR ptr;
	const int n = lstrlenW(lpText);
	for (i = 0, m = 0; i < n; i++)
	{
		switch (lpText[i])
		{
		case L'>':
		case L'<':
			m += 4;
			break;
		case L'&':
			m += 5;
			break;
		default:
			m++;
		}
	}
	if (n == m)return 0;
	LPWSTR lpOutText = (LPWSTR)GlobalAlloc(0, sizeof(WCHAR) * (m + 1));
	for (i = 0, ptr = lpOutText; i <= n; i++)
	{
		switch (lpText[i])
		{
		case L'>':
			lstrcpyW(ptr, L"&gt;");
			ptr += lstrlenW(ptr);
			break;
		case L'<':
			lstrcpyW(ptr, L"&lt;");
			ptr += lstrlenW(ptr);
			break;
		case L'&':
			lstrcpyW(ptr, L"&amp;");
			ptr += lstrlenW(ptr);
			break;
		default:
			*ptr++ = lpText[i];
		}
	}
	return lpOutText;
}

DWORD WINAPI ThreadFunc(LPVOID p)
{
	HWND hWnd = (HWND)p;
	LPCWSTR lpszCommand = (LPCWSTR)GetWindowLongPtr(hWnd, DWLP_USER);
	PostMessageW(hWnd, WM_APP, 0, (LPARAM)RunCommand(lpszCommand));
	ExitThread(0);
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
	static HANDLE hThread;
	static LPWSTR lpszCommand;
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
		WaitForSingleObject(hThread, INFINITE);
		CloseHandle(hThread);
		hThread = 0;
		{
			LPCWSTR lpszReturnString = (LPWSTR)lParam;
			if (lpszReturnString) {
				LPWSTR lpszReturnString2 = HtmlEncode(lpszReturnString);
				if (lpszReturnString2) {
					GlobalFree((HGLOBAL)lpszReturnString);
					lpszReturnString = lpszReturnString2;
				}
				const int nHtmlLength = lstrlenW(lpszReturnString) + 256;
				LPWSTR lpszHtml = (LPWSTR)GlobalAlloc(0, nHtmlLength * sizeof(WCHAR));
				lstrcpyW(lpszHtml, L"<div class=\"result\"><div class=\"icon\"><img></div><div class=\"chatting\"><div class=\"output\"><pre>");
				lstrcatW(lpszHtml, lpszReturnString);
				GlobalFree((HGLOBAL)lpszReturnString);
				lstrcatW(lpszHtml, L"</pre></div></div></div>");
				CComQIPtr<IHTMLElement>pElementBody;
				pDocument->get_body((IHTMLElement**)&pElementBody);
				if (pElementBody) {
					BSTR where = SysAllocString(L"beforeEnd");
					BSTR html = SysAllocString(lpszHtml);
					pElementBody->insertAdjacentHTML(where, html);
					SysFreeString(where);
					SysFreeString(html);
					pElementBody.Release();
					ScrollBottom(pDocument);
				}
				GlobalFree(lpszHtml);
			}
		}
		EnableWindow(hBrowser, TRUE);
		EnableWindow(hEdit, TRUE);
		SetFocus(hEdit);
		break;
	case WM_COMMAND:
		if (LOWORD(wParam) == ID_RUN)
		{
			if (GetFocus() != hEdit)
				SetFocus(hEdit);
			const int nTextLength = GetWindowTextLength(hEdit);
			if (nTextLength > 0) {
				GlobalFree(lpszCommand);
				lpszCommand = (LPWSTR)GlobalAlloc(0, (nTextLength + 1) * sizeof(WCHAR));
				if (lpszCommand) {
					GetWindowTextW(hEdit, lpszCommand, nTextLength + 1);
					SetWindowLongPtrW(hWnd, DWLP_USER, (LONG_PTR)lpszCommand);
					LPWSTR lpszCommand2 = HtmlEncode(lpszCommand);
					UINT nHtmlLength = (lpszCommand2 ? lstrlenW(lpszCommand2) : nTextLength) + 256;
					LPWSTR lpszHtml = (LPWSTR)GlobalAlloc(0, nHtmlLength * sizeof(WCHAR));
					if (lpszHtml) {
						lstrcpyW(lpszHtml, L"<div class=\"input\"><pre>");
						lstrcatW(lpszHtml, lpszCommand2 ? lpszCommand2 : lpszCommand);
						lstrcatW(lpszHtml, L"</pre></div>");
						CComQIPtr<IHTMLElement>pElementBody;
						pDocument->get_body((IHTMLElement**)&pElementBody);
						if (pElementBody) {
							BSTR where = SysAllocString(L"beforeEnd");
							BSTR html = SysAllocString(lpszHtml);
							pElementBody->insertAdjacentHTML(where, html);
							SysFreeString(where);
							SysFreeString(html);
							pElementBody.Release();
							ScrollBottom(pDocument);
						}
						GlobalFree(lpszHtml);
						SetWindowText(hEdit, 0);
						DWORD dwParam;
						EnableWindow(hBrowser, FALSE);
						EnableWindow(hEdit, FALSE);
						hThread = CreateThread(0, 0, ThreadFunc, (LPVOID)hWnd, 0, &dwParam);
					}
					GlobalFree((HGLOBAL)lpszCommand2);
				}
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
		GlobalFree(lpszCommand);
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

int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow)
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
