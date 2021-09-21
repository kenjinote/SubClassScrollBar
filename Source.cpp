#pragma comment(linker,"\"/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

#include <windows.h>
#include <commctrl.h>
#include "resource.h"

#define DEFAULT_DPI 96
#define SCALEX(X) MulDiv(X, uDpiX, DEFAULT_DPI)
#define SCALEY(Y) MulDiv(Y, uDpiY, DEFAULT_DPI)
#define POINT2PIXEL(PT) MulDiv(PT, uDpiY, 72)
#define TIMERID_NOTIFY	100
#define TIMERID_DELAY	200
#define TIME_DELAY		500
#define TIME_INTER		100
#define THUMB_BORDER 3
#define THUMB_MINSIZE (THUMB_BORDER*2)
#define SZCECOWIZSCROLLBARPROC TEXT("CBitmapScrollBarProc")

TCHAR szClassName[] = TEXT("Window");
WNDPROC defScrollBarWndProc;
WNDPROC defStaticWndProc;

BOOL GetScaling(HWND hWnd, UINT* pnX, UINT* pnY)
{
	BOOL bSetScaling = FALSE;
	const HMONITOR hMonitor = MonitorFromWindow(hWnd, MONITOR_DEFAULTTONEAREST);
	if (hMonitor)
	{
		HMODULE hShcore = LoadLibrary(TEXT("SHCORE"));
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

class CSkinScrollBar
{
public:
	CSkinScrollBar() {
		m_hBmp = NULL;
		m_bDrag = FALSE;
		memset(&m_si, 0, sizeof(SCROLLINFO));
		m_si.nTrackPos = -1;
		m_uClicked = -1;
		m_bNotify = FALSE;
		m_uHtPrev = -1;
		m_bPause = FALSE;
		m_bTrace = FALSE;
	};

	HWND	m_hWnd;
	HBITMAP	m_hBmp;
	int		m_nWid;
	int		m_nFrmHei;
	int		m_nHei;

	SCROLLINFO	m_si;
	BOOL		m_bDrag;
	POINT		m_ptDrag;
	int			m_nDragPos;

	UINT		m_uClicked;
	BOOL		m_bNotify;
	UINT		m_uHtPrev;
	BOOL		m_bPause;
	BOOL		m_bTrace;
public:
	virtual LRESULT LocalScrollBarProc(
		HWND hWnd,
		UINT msg,
		WPARAM wParam,
		LPARAM lParam
	);
	static LRESULT CALLBACK GlobalScrollBarProc(
		HWND hWnd,
		UINT msg,
		WPARAM wParam,
		LPARAM lParam
	);
public:
	void DrawArrow(UINT uArrow, int nState);
	void SetBitmap(HBITMAP hBmp);
	BOOL IsVertical();
	virtual ~CSkinScrollBar() {};

	// Generated message map functions
protected:
	void DrawThumb(HDC pDestDC, RECT* pDestRect, HDC pSourDC, RECT* pSourRect);
	void TileBlt(HDC pDestDC, RECT* pDestRect, HDC pSourDC, RECT* pSourRect);
	RECT GetRect(UINT uSBCode);
	RECT GetImageRect(UINT uSBCode, int nState = 0);
	UINT HitTest(POINT pt);
	//{{AFX_MSG(CSkinScrollBar)
	//afx_msg void OnSize(UINT nType, int cx, int cy);
	//afx_msg void OnPaint();
	//afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	void OnMouseMove(UINT nFlags, POINT point);
	//afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
	//afx_msg void OnTimer(UINT nIDEvent);
	//}}AFX_MSG
	//afx_msg LRESULT OnMouseLeave(WPARAM wparam, LPARAM lparam);

};

/* グローバル（静的）のウィンドウプロシージャ ※ローカル変数へのアクセス不可 */
LRESULT CALLBACK CSkinScrollBar::GlobalScrollBarProc(
	HWND hWnd,
	UINT msg,
	WPARAM wParam,
	LPARAM lParam
)
{
	CSkinScrollBar* p = (CSkinScrollBar*)GetProp(hWnd, SZCECOWIZSCROLLBARPROC);
	if (p)
	{
		return p->LocalScrollBarProc(hWnd, msg, wParam, lParam);
	}
	return 0;
}

BOOL CSkinScrollBar::IsVertical()
{
	DWORD dwStyle = (DWORD)GetWindowLongPtr(m_hWnd, GWL_STYLE);
	return dwStyle & SBS_VERT;
}

UINT CSkinScrollBar::HitTest(POINT pt)
{
	int nTestPos = pt.y;
	if (!IsVertical()) nTestPos = pt.x;
	if (nTestPos < 0) return -1;
	SCROLLINFO si = m_si;
	int nInterHei = m_nHei - 2 * m_nWid;
	if (nInterHei < 0) nInterHei = 0;
	int	nSlideHei = si.nPage * nInterHei / (si.nMax - si.nMin + 1);
	if (nSlideHei < THUMB_MINSIZE) nSlideHei = THUMB_MINSIZE;
	if (nInterHei < THUMB_MINSIZE) nSlideHei = 0;
	int nEmptyHei = nInterHei - nSlideHei;

	int nArrowHei = (nInterHei == 0) ? (m_nHei / 2) : m_nWid;
	int nBottom = 0;
	int nSegLen = nArrowHei;
	nBottom += nSegLen;
	UINT uHit = SB_LINEUP;
	if (nTestPos < nBottom) goto end;
	if (si.nTrackPos == -1) si.nTrackPos = si.nPos;
	uHit = SB_PAGEUP;
	if ((si.nMax - si.nMin - si.nPage + 1) == 0)
		nSegLen = nEmptyHei / 2;
	else
		nSegLen = nEmptyHei * si.nTrackPos / (si.nMax - si.nMin - si.nPage + 1);
	nBottom += nSegLen;
	if (nTestPos < nBottom) goto end;
	nSegLen = nSlideHei;
	nBottom += nSegLen;
	uHit = SB_THUMBTRACK;
	if (nTestPos < nBottom) goto end;
	nBottom = m_nHei - nArrowHei;
	uHit = SB_PAGEDOWN;
	if (nTestPos < nBottom) goto end;
	uHit = SB_LINEDOWN;
end:
	return uHit;
}

void CSkinScrollBar::SetBitmap(HBITMAP hBmp)
{
	m_hBmp = hBmp;
	BITMAP bm;
	GetObject(hBmp, sizeof(bm), &bm);
	m_nWid = bm.bmWidth / 9;
	m_nFrmHei = bm.bmHeight / 3;
	RECT rc;
	GetWindowRect(m_hWnd, &rc);
	ScreenToClient(GetParent(m_hWnd), (LPPOINT)&rc.left);
	ScreenToClient(GetParent(m_hWnd), (LPPOINT)&rc.right);
	if (IsVertical())
	{
		rc.right = rc.left + m_nWid;
	}
	else
	{
		rc.bottom = rc.top + m_nWid;
	}
	MoveWindow(m_hWnd, rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top, FALSE);
}

RECT CSkinScrollBar::GetImageRect(UINT uSBCode, int nState)
{
	int nIndex = 0;
	switch (uSBCode)
	{
	case SB_LINEUP:nIndex = 0; break;
	case SB_PAGEUP:nIndex = 3; break;
	case SB_THUMBTRACK:nIndex = 2; break;
	case SB_PAGEDOWN:nIndex = 3; break;
	case SB_LINEDOWN:nIndex = 1; break;
	}
	if (!IsVertical())nIndex += 4;
	RECT rcRet = { m_nWid * nIndex,m_nWid * nState,m_nWid * (nIndex + 1),m_nWid * (nState + 1) };
	return rcRet;
}

RECT CSkinScrollBar::GetRect(UINT uSBCode)
{
	SCROLLINFO si = m_si;
	if (si.nTrackPos == -1) si.nTrackPos = si.nPos;
	int nInterHei = m_nHei - 2 * m_nWid;
	if (nInterHei < 0) nInterHei = 0;
	int	nSlideHei = si.nPage * nInterHei / (si.nMax - si.nMin + 1);
	if (nSlideHei < THUMB_MINSIZE) nSlideHei = THUMB_MINSIZE;
	if (nInterHei < THUMB_MINSIZE) nSlideHei = 0;
	if ((si.nMax - si.nMin - si.nPage + 1) == 0) nSlideHei = 0;
	int nEmptyHei = nInterHei - nSlideHei;
	int nArrowHei = m_nWid;
	if (nInterHei == 0) nArrowHei = m_nHei / 2;
	RECT rcRet = { 0,0,m_nWid,nArrowHei };
	if (uSBCode == SB_LINEUP) goto end;
	rcRet.top = rcRet.bottom;
	if ((si.nMax - si.nMin - si.nPage + 1) == 0)
		rcRet.bottom += nEmptyHei / 2;
	else
		rcRet.bottom += nEmptyHei * si.nTrackPos / (si.nMax - si.nMin - si.nPage + 1);
	if (uSBCode == SB_PAGEUP) goto end;
	rcRet.top = rcRet.bottom;
	rcRet.bottom += nSlideHei;
	if (uSBCode == SB_THUMBTRACK) goto end;
	rcRet.top = rcRet.bottom;
	rcRet.bottom = m_nHei - nArrowHei;
	if (uSBCode == SB_PAGEDOWN) goto end;
	rcRet.top = rcRet.bottom;
	rcRet.bottom = m_nHei;
	if (uSBCode == SB_LINEDOWN) goto end;
end:
	if (!IsVertical())
	{
		int t = rcRet.left;
		rcRet.left = rcRet.top;
		rcRet.top = t;
		t = rcRet.right;
		rcRet.right = rcRet.bottom;
		rcRet.bottom = t;
	}
	return rcRet;
}

void CSkinScrollBar::TileBlt(HDC pDestDC, RECT* pDestRect, HDC pSourDC, RECT* pSourRect)
{
	int nSourHei = pSourRect->bottom - pSourRect->top;
	int nSourWid = pSourRect->right - pSourRect->left;

	int y = pDestRect->top;
	while (y < pDestRect->bottom)
	{
		int nHei = nSourHei;
		if (y + nHei > pDestRect->bottom) nHei = pDestRect->bottom - y;

		int x = pDestRect->left;
		while (x < pDestRect->right)
		{
			int nWid = nSourWid;
			if (x + nWid > pDestRect->right) nWid = pDestRect->right - x;
			BitBlt(pDestDC, x, y, nWid, nHei, pSourDC, pSourRect->left, pSourRect->top, SRCCOPY);
			x += nWid;
		}
		y += nHei;
	}
}

void CSkinScrollBar::DrawThumb(HDC pDestDC, RECT* pDestRect, HDC pSourDC, RECT* pSourRect)
{
	if (IsRectEmpty(pDestRect)) return;
	RECT rcDest = *pDestRect, rcSour = *pSourRect;
	if (IsVertical())
	{
		//ASSERT(pDestRect->bottom - pDestRect->top >= THUMB_MINSIZE);
		BitBlt(pDestDC, pDestRect->left, pDestRect->top, m_nWid, THUMB_BORDER, pSourDC, pSourRect->left, pSourRect->top, SRCCOPY);
		BitBlt(pDestDC, pDestRect->left, pDestRect->bottom - THUMB_BORDER, m_nWid, THUMB_BORDER, pSourDC, pSourRect->left, pSourRect->bottom - THUMB_BORDER, SRCCOPY);
		rcDest.top += THUMB_BORDER, rcDest.bottom -= THUMB_BORDER;
		rcSour.top += THUMB_BORDER, rcSour.bottom -= THUMB_BORDER;
		TileBlt(pDestDC, &rcDest, pSourDC, &rcSour);
	}
	else
	{
		//ASSERT(pDestRect->right - pDestRect->left >= THUMB_MINSIZE);
		BitBlt(pDestDC, pDestRect->left, pDestRect->top, THUMB_BORDER, m_nWid, pSourDC, pSourRect->left, pSourRect->top, SRCCOPY);
		BitBlt(pDestDC, pDestRect->right - THUMB_BORDER, pDestRect->top, THUMB_BORDER, m_nWid, pSourDC, pSourRect->right - THUMB_BORDER, pSourRect->top, SRCCOPY);
		rcDest.left += THUMB_BORDER, rcDest.right -= THUMB_BORDER;
		rcSour.left += THUMB_BORDER, rcSour.right -= THUMB_BORDER;
		TileBlt(pDestDC, &rcDest, pSourDC, &rcSour);
	}
}

void CSkinScrollBar::DrawArrow(UINT uArrow, int nState)
{
	//ASSERT(uArrow == SB_LINEUP || uArrow == SB_LINEDOWN);
	HDC pDC = GetDC(m_hWnd);
	HDC memdc = CreateCompatibleDC(pDC);
	HGDIOBJ hOldBmp = ::SelectObject(memdc, m_hBmp);

	RECT rcDest = GetRect(uArrow);
	RECT rcSour = GetImageRect(uArrow, nState);
	if ((rcDest.right - rcDest.left != rcSour.right - rcSour.left)
		|| (rcDest.bottom - rcDest.top != rcSour.bottom - rcSour.top))
		StretchBlt(pDC, rcDest.left, rcDest.top, rcDest.right - rcDest.left, rcDest.bottom - rcDest.top,
			memdc,
			rcSour.left, rcSour.top, rcSour.right - rcSour.left, rcSour.bottom - rcSour.top,
			SRCCOPY);
	else
		BitBlt(pDC, rcDest.left, rcDest.top, m_nWid, m_nWid, memdc, rcSour.left, rcSour.top, SRCCOPY);

	ReleaseDC(m_hWnd, pDC);
	::SelectObject(memdc, hOldBmp);
}

void CSkinScrollBar::OnMouseMove(UINT nFlags, POINT point)
{
	if (!m_bTrace && nFlags != -1)
	{
		m_bTrace = TRUE;
		TRACKMOUSEEVENT tme;
		tme.cbSize = sizeof(tme);
		tme.hwndTrack = m_hWnd;
		tme.dwFlags = TME_LEAVE;
		tme.dwHoverTime = 1;
		m_bTrace = TrackMouseEvent(&tme);
	}

	if (m_bDrag)
	{
		int nInterHei = m_nHei - 2 * m_nWid;
		int	nSlideHei = m_si.nPage * nInterHei / (m_si.nMax - m_si.nMin + 1);
		if (nSlideHei < THUMB_MINSIZE) nSlideHei = THUMB_MINSIZE;
		if (nInterHei < THUMB_MINSIZE) nSlideHei = 0;
		int nEmptyHei = nInterHei - nSlideHei;
		int nDragLen = IsVertical() ? point.y - m_ptDrag.y : point.x - m_ptDrag.x;
		int nSlide = (nEmptyHei == 0) ? 0 : (nDragLen * (int)(m_si.nMax - m_si.nMin - m_si.nPage + 1) / nEmptyHei);
		int nNewTrackPos = m_nDragPos + nSlide;
		if (nNewTrackPos < m_si.nMin)
		{
			nNewTrackPos = m_si.nMin;
		}
		else if (nNewTrackPos > (int)(m_si.nMax - m_si.nMin - m_si.nPage + 1))
		{
			nNewTrackPos = m_si.nMax - m_si.nMin - m_si.nPage + 1;
		}
		if (nNewTrackPos != m_si.nTrackPos)
		{
			HDC pDC = GetDC(m_hWnd);
			HDC memdc;
			memdc = CreateCompatibleDC(pDC);
			HGDIOBJ hOldBmp = SelectObject(memdc, m_hBmp);
			RECT rcThumb1 = GetRect(SB_THUMBTRACK);
			m_si.nTrackPos = nNewTrackPos;
			RECT rcThumb2 = GetRect(SB_THUMBTRACK);

			RECT rcSourSlide = GetImageRect(SB_PAGEUP, 0);
			RECT rcSourThumb = GetImageRect(SB_THUMBTRACK, 2);
			RECT rcOld;
			if (IsVertical())
			{
				rcOld.left = 0, rcOld.right = m_nWid;
				if (rcThumb2.bottom > rcThumb1.bottom)
				{
					rcOld.top = rcThumb1.top;
					rcOld.bottom = rcThumb2.top;
				}
				else
				{
					rcOld.top = rcThumb2.bottom;
					rcOld.bottom = rcThumb1.bottom;
				}
			}
			else
			{
				rcOld.top = 0, rcOld.bottom = m_nWid;
				if (rcThumb2.right > rcThumb1.right)
				{
					rcOld.left = rcThumb1.left;
					rcOld.right = rcThumb2.left;
				}
				else
				{
					rcOld.left = rcThumb2.right;
					rcOld.right = rcThumb1.right;
				}
			}
			TileBlt(pDC, &rcOld, memdc, &rcSourSlide);
			DrawThumb(pDC, &rcThumb2, memdc, &rcSourThumb);
			SelectObject(memdc, hOldBmp);
			ReleaseDC(m_hWnd, pDC);

			SendMessage(GetParent(m_hWnd), IsVertical() ? WM_VSCROLL : WM_HSCROLL, MAKELONG(SB_THUMBTRACK, m_si.nTrackPos), (LPARAM)m_hWnd);
		}
	}
	else if (m_uClicked != -1)
	{
		RECT rc = GetRect(m_uClicked);
		m_bPause = !PtInRect(&rc, point);
		if (m_uClicked == SB_LINEUP || m_uClicked == SB_LINEDOWN)
		{
			DrawArrow(m_uClicked, m_bPause ? 0 : 2);
		}
	}
	else
	{
		UINT uHit = HitTest(point);
		if (uHit != m_uHtPrev)
		{
			if (m_uHtPrev != -1)
			{
				if (m_uHtPrev == SB_THUMBTRACK)
				{
					HDC pDC = GetDC(m_hWnd);
					HDC memdc = CreateCompatibleDC(pDC);
					HGDIOBJ hOldBmp = SelectObject(memdc, m_hBmp);
					RECT rcDest = GetRect(SB_THUMBTRACK);
					RECT rcSour = GetImageRect(SB_THUMBTRACK, 0);
					DrawThumb(pDC, &rcDest, memdc, &rcSour);
					ReleaseDC(m_hWnd, pDC);
					SelectObject(memdc, hOldBmp);
				}
				else if (m_uHtPrev == SB_LINEUP || m_uHtPrev == SB_LINEDOWN)
				{
					DrawArrow(m_uHtPrev, 0);
				}
			}
			if (uHit != -1)
			{
				if (uHit == SB_THUMBTRACK)
				{
					HDC pDC = GetDC(m_hWnd);
					HDC memdc =	CreateCompatibleDC(pDC);
					HGDIOBJ hOldBmp = SelectObject(memdc, m_hBmp);
					RECT rcDest = GetRect(SB_THUMBTRACK);
					RECT rcSour = GetImageRect(SB_THUMBTRACK, 1);
					DrawThumb(pDC, &rcDest, memdc, &rcSour);
					ReleaseDC(m_hWnd, pDC);
					SelectObject(memdc, hOldBmp);
				}
				else if (uHit == SB_LINEUP || uHit == SB_LINEDOWN)
				{
					DrawArrow(uHit, 1);
				}
			}
			m_uHtPrev = uHit;
		}
	}
}

LRESULT CSkinScrollBar::LocalScrollBarProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg)
	{
	case WM_SIZE:
		{
			m_nHei = IsVertical() ? HIWORD(lParam) : LOWORD(lParam);
		}
		break;
	case WM_ERASEBKGND:
		return 1;
	case WM_PAINT:
		{
			PAINTSTRUCT ps;
			HDC hdc = BeginPaint(hWnd, &ps); // device context for painting
			if (m_hBmp)
			{
				HDC memdc;
				memdc = CreateCompatibleDC(hdc);
				HGDIOBJ hOldBmp = ::SelectObject(memdc, m_hBmp);

				RECT rcSour = { 0,0,m_nWid,m_nWid };
				if (!IsVertical()) OffsetRect(&rcSour, m_nWid * 4, 0);
				RECT rcDest;
				rcDest = GetRect(SB_LINEUP);
				if ((rcDest.right - rcDest.left != rcSour.right - rcSour.left)
					|| (rcDest.bottom - rcDest.top != rcSour.bottom - rcSour.top))
					StretchBlt(hdc, rcDest.left, rcDest.top, rcDest.right - rcDest.left, rcDest.bottom - rcDest.top,
						memdc,
						rcSour.left, rcSour.top, rcSour.right - rcSour.left, rcSour.bottom - rcSour.top,
						SRCCOPY);
				else
					BitBlt(hdc, rcDest.left, rcDest.top, m_nWid, m_nWid, memdc, rcSour.left, rcSour.top, SRCCOPY);
				rcDest = GetRect(SB_LINEDOWN);
				OffsetRect(&rcSour, m_nWid, 0);
				if ((rcDest.right - rcDest.left != rcSour.right - rcSour.left)
					|| (rcDest.bottom - rcDest.top != rcSour.bottom - rcSour.top))
					StretchBlt(hdc, rcDest.left, rcDest.top, rcDest.right - rcDest.left, rcDest.bottom - rcDest.top,
						memdc,
						rcSour.left, rcSour.top, rcSour.right - rcSour.left, rcSour.bottom - rcSour.top,
						SRCCOPY);
				else
					BitBlt(hdc, rcDest.left, rcDest.top, m_nWid, m_nWid, memdc, rcSour.left, rcSour.top, SRCCOPY);
				rcDest = GetRect(SB_THUMBTRACK);
				OffsetRect(&rcSour, m_nWid, 0);
				DrawThumb(hdc, &rcDest, memdc, &rcSour);
				OffsetRect(&rcSour, m_nWid, 0);
				rcDest = GetRect(SB_PAGEUP);
				TileBlt(hdc, &rcDest, memdc, &rcSour);
				rcDest = GetRect(SB_PAGEDOWN);
				TileBlt(hdc, &rcDest, memdc, &rcSour);

				::SelectObject(memdc, hOldBmp);
			}
		}
		return 0;
	case WM_LBUTTONDOWN:
		{
			POINT point = {LOWORD(lParam), HIWORD(lParam)};
			SetCapture(hWnd);
			UINT uHit = HitTest(point);
			if (uHit == SB_THUMBTRACK)
			{
				m_bDrag = TRUE;
				m_ptDrag = point;
				m_si.nTrackPos = m_si.nPos;
				m_nDragPos = m_si.nPos;
			}
			else
			{
				m_uClicked = uHit;
				SendMessage(GetParent(hWnd), IsVertical() ? WM_VSCROLL : WM_HSCROLL, MAKELONG(m_uClicked, 0), (LPARAM)m_hWnd);
				SetTimer(hWnd, TIMERID_DELAY, TIME_DELAY, NULL);
				m_bPause = FALSE;
				if (uHit == SB_LINEUP || uHit == SB_LINEDOWN) DrawArrow(uHit, 2);
			}
		}
		return 0;
	case WM_MOUSEMOVE:
		{
			int nFlags = (int)wParam;
			POINT point = { LOWORD(lParam), HIWORD(lParam) };
			if (!m_bTrace && nFlags != -1)
			{
				m_bTrace = TRUE;
				TRACKMOUSEEVENT tme;
				tme.cbSize = sizeof(tme);
				tme.hwndTrack = m_hWnd;
				tme.dwFlags = TME_LEAVE;
				tme.dwHoverTime = 1;
				m_bTrace = TrackMouseEvent(&tme);
			}

			if (m_bDrag)
			{
				int nInterHei = m_nHei - 2 * m_nWid;
				int	nSlideHei = m_si.nPage * nInterHei / (m_si.nMax - m_si.nMin + 1);
				if (nSlideHei < THUMB_MINSIZE) nSlideHei = THUMB_MINSIZE;
				if (nInterHei < THUMB_MINSIZE) nSlideHei = 0;
				int nEmptyHei = nInterHei - nSlideHei;
				int nDragLen = IsVertical() ? point.y - m_ptDrag.y : point.x - m_ptDrag.x;
				int nSlide = (nEmptyHei == 0) ? 0 : (nDragLen * (int)(m_si.nMax - m_si.nMin - m_si.nPage + 1) / nEmptyHei);
				int nNewTrackPos = m_nDragPos + nSlide;
				if (nNewTrackPos < m_si.nMin)
				{
					nNewTrackPos = m_si.nMin;
				}
				else if (nNewTrackPos > (int)(m_si.nMax - m_si.nMin - m_si.nPage + 1))
				{
					nNewTrackPos = m_si.nMax - m_si.nMin - m_si.nPage + 1;
				}
				if (nNewTrackPos != m_si.nTrackPos)
				{
					HDC pDC = GetDC(m_hWnd);
					HDC memdc = CreateCompatibleDC(pDC);
					HGDIOBJ hOldBmp = SelectObject(memdc, m_hBmp);
					RECT rcThumb1 = GetRect(SB_THUMBTRACK);
					m_si.nTrackPos = nNewTrackPos;
					RECT rcThumb2 = GetRect(SB_THUMBTRACK);

					RECT rcSourSlide = GetImageRect(SB_PAGEUP, 0);
					RECT rcSourThumb = GetImageRect(SB_THUMBTRACK, 2);
					RECT rcOld;
					if (IsVertical())
					{
						rcOld.left = 0, rcOld.right = m_nWid;
						if (rcThumb2.bottom > rcThumb1.bottom)
						{
							rcOld.top = rcThumb1.top;
							rcOld.bottom = rcThumb2.top;
						}
						else
						{
							rcOld.top = rcThumb2.bottom;
							rcOld.bottom = rcThumb1.bottom;
						}
					}
					else
					{
						rcOld.top = 0, rcOld.bottom = m_nWid;
						if (rcThumb2.right > rcThumb1.right)
						{
							rcOld.left = rcThumb1.left;
							rcOld.right = rcThumb2.left;
						}
						else
						{
							rcOld.left = rcThumb2.right;
							rcOld.right = rcThumb1.right;
						}
					}
					TileBlt(pDC, &rcOld, memdc, &rcSourSlide);
					DrawThumb(pDC, &rcThumb2, memdc, &rcSourThumb);
					SelectObject(memdc, hOldBmp);
					ReleaseDC(m_hWnd, pDC);

					SendMessage(GetParent(hWnd), IsVertical() ? WM_VSCROLL : WM_HSCROLL, MAKELONG(SB_THUMBTRACK, m_si.nTrackPos), (LPARAM)m_hWnd);
				}
			}
			else if (m_uClicked != -1)
			{
				RECT rc = GetRect(m_uClicked);
				m_bPause = PtInRect(&rc, point);
				if (m_uClicked == SB_LINEUP || m_uClicked == SB_LINEDOWN)
				{
					DrawArrow(m_uClicked, m_bPause ? 0 : 2);
				}
			}
			else
			{
				UINT uHit = HitTest(point);
				if (uHit != m_uHtPrev)
				{
					if (m_uHtPrev != -1)
					{
						if (m_uHtPrev == SB_THUMBTRACK)
						{
							HDC pDC = GetDC(m_hWnd);
							HDC memdc = CreateCompatibleDC(pDC);
							HGDIOBJ hOldBmp = SelectObject(memdc, m_hBmp);
							RECT rcDest = GetRect(SB_THUMBTRACK);
							RECT rcSour = GetImageRect(SB_THUMBTRACK, 0);
							DrawThumb(pDC, &rcDest, memdc, &rcSour);
							ReleaseDC(m_hWnd, pDC);
							SelectObject(memdc, hOldBmp);
						}
						else if (m_uHtPrev == SB_LINEUP || m_uHtPrev == SB_LINEDOWN)
						{
							DrawArrow(m_uHtPrev, 0);
						}
					}
					if (uHit != -1)
					{
						if (uHit == SB_THUMBTRACK)
						{
							HDC pDC = GetDC(m_hWnd);
							HDC memdc = CreateCompatibleDC(pDC);
							HGDIOBJ hOldBmp = SelectObject(memdc, m_hBmp);
							RECT rcDest = GetRect(SB_THUMBTRACK);
							RECT rcSour = GetImageRect(SB_THUMBTRACK, 1);
							DrawThumb(pDC, &rcDest, memdc, &rcSour);
							ReleaseDC(m_hWnd, pDC);
							SelectObject(memdc, hOldBmp);
						}
						else if (uHit == SB_LINEUP || uHit == SB_LINEDOWN)
						{
							DrawArrow(uHit, 1);
						}
					}
					m_uHtPrev = uHit;
				}
			}
		}
		return 0;
	case WM_LBUTTONUP:
		{
			POINT point = {LOWORD(lParam), HIWORD(lParam)};
			ReleaseCapture();
			if (m_bDrag)
			{
				m_bDrag = FALSE;
				SendMessage(GetParent(m_hWnd), IsVertical() ? WM_VSCROLL : WM_HSCROLL, MAKELONG(SB_THUMBPOSITION, m_si.nTrackPos), (LPARAM)m_hWnd);
				HDC pDC = GetDC(m_hWnd);
				HDC memdc = CreateCompatibleDC(pDC);
				HGDIOBJ hOldBmp = SelectObject(memdc, m_hBmp);
				if (m_si.nTrackPos != m_si.nPos)
				{
					RECT rcThumb = GetRect(SB_THUMBTRACK);
					RECT rcSour = { m_nWid * 3,0,m_nWid * 4,m_nWid };
					if (!IsVertical())  OffsetRect(&rcSour, m_nWid * 4, 0);
					TileBlt(pDC, &rcThumb, memdc, &rcSour);
				}
				m_si.nTrackPos = -1;

				RECT rcThumb = GetRect(SB_THUMBTRACK);
				RECT rcSour = { m_nWid * 2,0,m_nWid * 3,m_nWid };
				if (PtInRect(&rcThumb, point)) OffsetRect(&rcSour, 0, m_nWid);
				if (!IsVertical()) OffsetRect(&rcSour, m_nWid * 4, 0);
				DrawThumb(pDC, &rcThumb, memdc, &rcSour);
				SelectObject(memdc, hOldBmp);
				ReleaseDC(m_hWnd, pDC);
			}
			else if (m_uClicked != -1)
			{
				if (m_bNotify)
				{
					KillTimer(m_hWnd, TIMERID_NOTIFY);
					m_bNotify = FALSE;
				}
				else
				{
					KillTimer(m_hWnd, TIMERID_DELAY);
				}
				if (m_uClicked == SB_LINEUP || m_uClicked == SB_LINEDOWN) DrawArrow(m_uClicked, 0);
				m_uClicked = -1;
			}
		}
		return 0;
	case WM_TIMER:
		{
			// TODO: Add your message handler code here and/or call default
			int nIDEvent = (int)wParam;
			if (nIDEvent == TIMERID_DELAY)
			{
				m_bNotify = TRUE;
				nIDEvent = TIMERID_NOTIFY;
				KillTimer(m_hWnd, TIMERID_DELAY);
				SetTimer(m_hWnd, TIMERID_NOTIFY, TIME_INTER, NULL);
			}
			if (nIDEvent == TIMERID_NOTIFY && !m_bPause)
			{
				//ASSERT(m_uClicked != -1 && m_uClicked != SB_THUMBTRACK);

				switch (m_uClicked)
				{
				case SB_LINEUP:
					if (m_si.nPos == m_si.nMin)
					{
						KillTimer(m_hWnd, TIMERID_NOTIFY);
						break;
					}
					SendMessage(GetParent(m_hWnd), IsVertical() ? WM_VSCROLL : WM_HSCROLL, MAKELONG(SB_LINEUP, 0), (LPARAM)m_hWnd);
					break;
				case SB_LINEDOWN:
					if (m_si.nPos == m_si.nMax)
					{
						KillTimer(m_hWnd, TIMERID_NOTIFY);
						break;
					}
					SendMessage(GetParent(m_hWnd), IsVertical() ? WM_VSCROLL : WM_HSCROLL, MAKELONG(SB_LINEDOWN, 0), (LPARAM)m_hWnd);
					break;
				case SB_PAGEUP:
				case SB_PAGEDOWN:
					{
						POINT pt;
						GetCursorPos(&pt);
						ScreenToClient(m_hWnd, &pt);
						RECT rc = GetRect(SB_THUMBTRACK);
						if (PtInRect(&rc, pt))
						{
							KillTimer(m_hWnd, TIMERID_NOTIFY);
							break;
						}
						SendMessage(GetParent(m_hWnd), IsVertical() ? WM_VSCROLL : WM_HSCROLL, MAKELONG(m_uClicked, 0), (LPARAM)m_hWnd);
					}
					break;
				default:
					//ASSERT(FALSE);
					break;
				}
			}
		}
		break;
	case WM_MOUSELEAVE:
		{
			m_bTrace = FALSE;
			OnMouseMove(-1, POINT{ -1, -1 });
		}
		return 0;
	case WM_LBUTTONDBLCLK:
		return 1;
	case SBM_SETSCROLLINFO:
		{
			BOOL bRedraw = (BOOL)wParam;
			LPSCROLLINFO lpScrollInfo = (LPSCROLLINFO)lParam;
			if (lpScrollInfo->fMask & SIF_PAGE) m_si.nPage = lpScrollInfo->nPage;
			if (lpScrollInfo->fMask & SIF_POS) m_si.nPos = lpScrollInfo->nPos;
			if (lpScrollInfo->fMask & SIF_RANGE)
			{
				m_si.nMin = lpScrollInfo->nMin;
				m_si.nMax = lpScrollInfo->nMax;
			}
			if (bRedraw)
			{
				HDC pDC = GetDC(m_hWnd);
				HDC memdc;
				memdc = CreateCompatibleDC(pDC);
				HGDIOBJ hOldBmp = ::SelectObject(memdc, m_hBmp);

				RECT rcSour = GetImageRect(SB_PAGEUP);
				RECT rcDest = GetRect(SB_PAGEUP);
				TileBlt(pDC, &rcDest, memdc, &rcSour);
				rcDest = GetRect(SB_THUMBTRACK);
				rcSour = GetImageRect(SB_THUMBTRACK);
				DrawThumb(pDC, &rcDest, memdc, &rcSour);
				rcDest = GetRect(SB_PAGEDOWN);
				rcSour = GetImageRect(SB_PAGEDOWN);
				TileBlt(pDC, &rcDest, memdc, &rcSour);
				::SelectObject(memdc, hOldBmp);
				ReleaseDC(m_hWnd, pDC);
			}
			return TRUE;
		}
		break;
	case SBM_GETSCROLLINFO:
		{
			LPSCROLLINFO lpScrollInfo = (LPSCROLLINFO)lParam;
			int nMask = lpScrollInfo->fMask;
			if (nMask & SIF_PAGE) lpScrollInfo->nPage = m_si.nPage;
			if (nMask & SIF_POS) lpScrollInfo->nPos = m_si.nPos;
			if (nMask & SIF_TRACKPOS) lpScrollInfo->nTrackPos = m_si.nTrackPos;
			if (nMask & SIF_RANGE)
			{
				lpScrollInfo->nMin = m_si.nMin;
				lpScrollInfo->nMax = m_si.nMax;
			}
			return TRUE;
		}
		break;
	default:
		break;
	}
	return CallWindowProc(defScrollBarWndProc, hWnd, msg, wParam, lParam);
}

//class CSkinScrollWnd
//{
//	// Construction
//public:
//	HWND m_hWnd;
//	CSkinScrollWnd();
//
//	// Attributes
//public:
//	int			m_nScrollWid;
//	CSkinScrollBar	m_sbHorz, m_sbVert;
//	HWND		m_wndLimit;
//	HBITMAP		m_hBmpScroll;
//	BOOL		m_bOp;
//	int			m_nAngleType;
//	// Operations
//public:
//	WNDPROC		m_funOldProc;
//	// Overrides
//		// ClassWizard generated virtual function overrides
//		//{{AFX_VIRTUAL(CSkinScrollWnd)
//protected:
//	//}}AFX_VIRTUAL
//
//// Implementation
//public:
//	BOOL SkinWindow(HWND hWnd, HBITMAP hBmpScroll);
//	virtual ~CSkinScrollWnd();
//
//	// Generated message map functions
//protected:
//	LRESULT OnDestMove(WPARAM wParam, LPARAM lParam);
//	//{{AFX_MSG(CSkinScrollWnd)
////	afx_msg void OnSize(UINT nType, int cx, int cy);
////	afx_msg void OnVScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);
////	afx_msg void OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);
////	afx_msg void OnPaint();
////	afx_msg void OnTimer(UINT nIDEvent);
//	//}}AFX_MSG
////	DECLARE_MESSAGE_MAP()
//};

//#define TIMER_UPDATE 100
//#define UM_DESTMOVE	(WM_USER+100)
//
//extern "C" BOOL UnskinWndScroll(HWND hWnd)
//{
//	CSkinScrollWnd* pFrame = (CSkinScrollWnd*)GetWindowLongPtr(hWnd, GWLP_USERDATA);
//	if (pFrame)
//	{
//		RECT rc;
//		HWND pParent = GetParent(pFrame->m_hWnd);
//		GetWindowRect(pFrame->m_hWnd, &rc);
//		ScreenToClient(pParent, (LPPOINT)&rc.left);
//		ScreenToClient(pParent, (LPPOINT)&rc.right);
//		SetWindowLongPtr(hWnd, GWLP_WNDPROC, (LONG_PTR)pFrame->m_funOldProc);
//		SetWindowLongPtr(hWnd, GWLP_USERDATA, 0);
//		SetParent(hWnd, pParent);
//		MoveWindow(hWnd, rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top, FALSE);
//		DestroyWindow(pFrame->m_hWnd);
//		delete pFrame;
//	}
//	return TRUE;
//}
//
//static LRESULT CALLBACK
//HookWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
//{
//	CSkinScrollWnd* pSkin = (CSkinScrollWnd*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
//	if (msg == WM_DESTROY)
//	{
//		WNDPROC procOld = pSkin->m_funOldProc;
//		UnskinWndScroll(hwnd);
//		return ::CallWindowProc(procOld, hwnd, msg, wp, lp);
//	}
//
//	LRESULT lr = ::CallWindowProc(pSkin->m_funOldProc, hwnd, msg, wp, lp);
//	if (pSkin->m_bOp) return lr;
//	if (msg == WM_ERASEBKGND)
//	{
//		SCROLLINFO si;
//		DWORD dwStyle = ::GetWindowLong(hwnd, GWL_STYLE);
//		if (dwStyle & WS_VSCROLL)
//		{
//			memset(&si, 0, sizeof(si));
//			si.cbSize = sizeof(si);
//			si.fMask = SIF_ALL;
//			::GetScrollInfo(hwnd, SB_VERT, &si);
//			if (si.nMax != pSkin->m_sbVert.m_si.nMax
//				|| si.nMin != pSkin->m_sbVert.m_si.nMin
//				|| si.nPos != pSkin->m_sbVert.m_si.nPos
//				|| si.nPage != pSkin->m_sbVert.m_si.nPage)
//			{
//				SetScrollInfo(pSkin->m_sbVert.m_hWnd, SB_VERT, &si, !pSkin->m_bOp);
//				EnableWindow(pSkin->m_sbVert.m_hWnd, (BOOL)((UINT)si.nMax >= si.nPage));
//			}
//		}
//		if (dwStyle & WS_HSCROLL)
//		{
//			memset(&si, 0, sizeof(si));
//			si.cbSize = sizeof(si);
//			si.fMask = SIF_ALL;
//			::GetScrollInfo(hwnd, SB_HORZ, &si);
//			if (si.nMax != pSkin->m_sbHorz.m_si.nMax
//				|| si.nMin != pSkin->m_sbHorz.m_si.nMin
//				|| si.nPos != pSkin->m_sbHorz.m_si.nPos
//				|| si.nPage != pSkin->m_sbHorz.m_si.nPage)
//			{
//				SetScrollInfo(pSkin->m_sbHorz.m_hWnd, SB_HORZ, &si, !pSkin->m_bOp);
//				EnableWindow(pSkin->m_sbHorz.m_hWnd, (BOOL)((UINT)si.nMax >= si.nPage));
//			}
//		}
//	}
//	else if (msg == WM_NCCALCSIZE && wp)
//	{
//		LPNCCALCSIZE_PARAMS pNcCalcSizeParam = (LPNCCALCSIZE_PARAMS)lp;
//		DWORD dwStyle = ::GetWindowLong(hwnd, GWL_STYLE);
//		DWORD dwExStyle = ::GetWindowLong(hwnd, GWL_EXSTYLE);
//		BOOL  bLeftScroll = dwExStyle & WS_EX_LEFTSCROLLBAR;
//		int nWid = ::GetSystemMetrics(SM_CXVSCROLL);
//		if (dwStyle & WS_VSCROLL)
//		{
//			if (bLeftScroll)
//				pNcCalcSizeParam->rgrc[0].left -= nWid - pSkin->m_nScrollWid;
//			else
//				pNcCalcSizeParam->rgrc[0].right += nWid - pSkin->m_nScrollWid;
//		}
//		if (dwStyle & WS_HSCROLL) pNcCalcSizeParam->rgrc[0].bottom += nWid - pSkin->m_nScrollWid;
//
//		RECT rc, rcVert, rcHorz;
//		::GetWindowRect(hwnd, &rc);
//		::OffsetRect(&rc, -rc.left, -rc.top);
//
//		nWid = pSkin->m_nScrollWid;
//		if (bLeftScroll)
//		{
//			int nLeft = pNcCalcSizeParam->rgrc[0].left;
//			int nBottom = pNcCalcSizeParam->rgrc[0].bottom;
//			rcVert.right = nLeft;
//			rcVert.left = nLeft - nWid;
//			rcVert.top = 0;
//			rcVert.bottom = nBottom;
//			rcHorz.left = nLeft;
//			rcHorz.right = pNcCalcSizeParam->rgrc[0].right;
//			rcHorz.top = nBottom;
//			rcHorz.bottom = nBottom + nWid;
//		}
//		else
//		{
//			int nRight = pNcCalcSizeParam->rgrc[0].right;
//			int nBottom = pNcCalcSizeParam->rgrc[0].bottom;
//			rcVert.left = nRight;
//			rcVert.right = nRight + nWid;
//			rcVert.top = 0;
//			rcVert.bottom = nBottom;
//			rcHorz.left = 0;
//			rcHorz.right = nRight;
//			rcHorz.top = nBottom;
//			rcHorz.bottom = nBottom + nWid;
//		}
//		if (dwStyle & WS_VSCROLL && dwStyle & WS_HSCROLL)
//		{
//			pSkin->m_nAngleType = bLeftScroll ? 1 : 2;
//		}
//		else
//		{
//			pSkin->m_nAngleType = 0;
//		}
//		if (dwStyle & WS_VSCROLL)
//		{
//			MoveWindow(pSkin->m_sbVert.m_hWnd, rcVert.left, rcVert.top, rcVert.right - rcVert.left, rcVert.bottom - rcVert.top, FALSE);
//			ShowWindow(pSkin->m_sbVert.m_hWnd, SW_SHOW);
//		}
//		else
//		{
//			ShowWindow(pSkin->m_sbVert.m_hWnd, SW_HIDE);
//		}
//		if (dwStyle & WS_HSCROLL)
//		{
//			MoveWindow(pSkin->m_sbHorz.m_hWnd, rcHorz.left, rcHorz.top, rcHorz.right - rcHorz.left, rcHorz.bottom - rcHorz.top, FALSE);
//			ShowWindow(pSkin->m_sbHorz.m_hWnd, SW_SHOW);
//		}
//		else
//		{
//			ShowWindow(pSkin->m_sbHorz.m_hWnd, SW_HIDE);
//		}
//		PostMessage(pSkin->m_hWnd, UM_DESTMOVE, dwStyle & WS_VSCROLL, bLeftScroll);
//	}
//	return lr;
//}
//
//extern "C" CSkinScrollWnd * SkinWndScroll(HWND hWnd, HBITMAP hBmpScroll)
//{
//	CSkinScrollWnd* pFrame = new CSkinScrollWnd;
//	pFrame->SkinWindow(hWnd, hBmpScroll);
//	return pFrame;
//}
//
///////////////////////////////////////////////////////////////////////////////
//// CSkinScrollWnd
//
//CSkinScrollWnd::CSkinScrollWnd()
//{
//	m_funOldProc = NULL;
//	m_bOp = FALSE;
//	m_nScrollWid = 16;
//	m_hBmpScroll = 0;
//	m_nAngleType = 0;
//}
//
//CSkinScrollWnd::~CSkinScrollWnd()
//{
//}
//
//
////BEGIN_MESSAGE_MAP(CSkinScrollWnd, CWnd)
////	//{{AFX_MSG_MAP(CSkinScrollWnd)
////	ON_WM_SIZE()
////	ON_WM_VSCROLL()
////	ON_WM_HSCROLL()
////	ON_WM_PAINT()
////	ON_WM_TIMER()
////	//}}AFX_MSG_MAP
////	ON_MESSAGE(UM_DESTMOVE, OnDestMove)
////END_MESSAGE_MAP()
//
//
///////////////////////////////////////////////////////////////////////////////
//// CSkinScrollWnd message handlers
//
//
//BOOL CSkinScrollWnd::SkinWindow(HWND pWnd, HBITMAP hBmpScroll)
//{
//	//ASSERT(m_hWnd == NULL);
//	m_hBmpScroll = hBmpScroll;
//	BITMAP bm;
//	GetObject(hBmpScroll, sizeof(bm), &bm);
//	m_nScrollWid = bm.bmWidth / 9;
//
//	HWND pParent = GetParent(pWnd);
//	//ASSERT(pParent);
//	RECT rcFrm, rcWnd;
//	GetWindowRect(pWnd, &rcFrm);
//	ScreenToClient(pParent, (LPPOINT)&rcFrm.left);
//	ScreenToClient(pParent, (LPPOINT)&rcFrm.right);
//	rcWnd = rcFrm;
//	OffsetRect(&rcWnd, -rcWnd.left, -rcWnd.top);
//	UINT uID = GetDlgCtrlID(pWnd);
//	DWORD_PTR dwStyle = (DWORD)GetWindowLongPtr(pWnd, GWL_STYLE);
//	DWORD dwExStyle = (DWORD)GetWindowLongPtr(pWnd, GWL_EXSTYLE);
//	DWORD dwFrmStyle = WS_CHILD | SS_NOTIFY;
//	DWORD dwFrmStyleEx = 0;
//	if (dwExStyle & WS_EX_TOOLWINDOW)
//	{
//		dwFrmStyleEx |= WS_EX_TOOLWINDOW;
//		dwFrmStyleEx |= WS_EX_TOPMOST;
//	}
//	if (dwStyle & WS_VISIBLE) dwFrmStyle |= WS_VISIBLE;
//
//	if (dwStyle & WS_BORDER)
//	{
//		dwFrmStyle |= WS_BORDER;
//		//pWnd->ModifyStyle(WS_BORDER, 0);
//		SetWindowLongPtr(pWnd, GWL_STYLE, GetWindowLongPtr(pWnd, GWL_STYLE) & (~WS_BORDER));
//		int nBorder = ::GetSystemMetrics(SM_CXBORDER);
//		rcWnd.right -= nBorder * 2;
//		rcWnd.bottom -= nBorder * 2;
//	}
//	if (dwExStyle & WS_EX_CLIENTEDGE)
//	{
//		//pWnd->ModifyStyleEx(WS_EX_CLIENTEDGE, 0);
//		SetWindowLongPtr(pWnd, GWL_EXSTYLE, GetWindowLongPtr(pWnd, GWL_EXSTYLE) & (~WS_EX_CLIENTEDGE));
//		int nBorder = ::GetSystemMetrics(SM_CXEDGE);
//		rcWnd.right -= nBorder * 2;
//		rcWnd.bottom -= nBorder * 2;
//		dwFrmStyleEx |= WS_EX_CLIENTEDGE;
//	}
//
//	//this->CreateEx(dwFrmStyleEx, AfxRegisterWndClass(NULL), "SkinScrollBarFrame", dwFrmStyle, rcFrm, pParent, uID);
//	{
//		LPCWSTR lpszClassName = L"SkinScrollBarFrame";
//		WNDCLASS wndclass = { 0,DefWindowProc,0,0,GetModuleHandle(0),0,LoadCursor(0,IDC_ARROW),0,0,szClassName };
//		RegisterClass(&wndclass);
//		this->m_hWnd = CreateWindowEx(dwFrmStyleEx, L"SkinScrollBarFrame", 0, dwFrmStyle, rcFrm.left, rcFrm.top, rcFrm.right - rcFrm.left, rcFrm.bottom - rcFrm.top, pParent, (HMENU)uID, GetModuleHandle(0), 0);
//	}
//
//	//m_wndLimit.Create(NULL, "LIMIT", WS_CHILD | WS_VISIBLE, CRect(0, 0, 0, 0), this, 200);
//	{
//		LPCWSTR lpszClassName = L"LIMIT";
//		WNDCLASS wndclass = { 0,DefWindowProc,0,0,GetModuleHandle(0),0,LoadCursor(0,IDC_ARROW),0,0,szClassName };
//		RegisterClass(&wndclass);
//		this->m_hWnd = CreateWindowEx(0, L"SkinScrollBarFrame", 0, WS_CHILD | WS_VISIBLE, rcFrm.left, rcFrm.top, rcFrm.right - rcFrm.left, rcFrm.bottom - rcFrm.top, pParent, (HMENU)uID, GetModuleHandle(0), 0);
//	}
//
//
//	//m_sbHorz.Create(WS_CHILD, CRect(0, 0, 0, 0), this, 100);
//	{
//		m_sbHorz.m_hWnd = CreateWindow(L"SCROLLBAR", 0, WS_CHILD | WS_VISIBLE | SBS_HORZ, 0, 0, 0, 0, m_hWnd, (HMENU)100, GetModuleHandle(0), 0);
//	}
//	//m_sbVert.Create(WS_CHILD | SBS_VERT, CRect(0, 0, 0, 0), this, 101);
//	{
//		m_sbVert.m_hWnd = CreateWindow(L"SCROLLBAR", 0, WS_CHILD | WS_VISIBLE | SBS_VERT, 0, 0, 0, 0, m_hWnd, (HMENU)100, GetModuleHandle(0), 0);
//	}
//	m_sbHorz.SetBitmap(m_hBmpScroll);
//	m_sbVert.SetBitmap(m_hBmpScroll);
//
//	SetParent(pWnd, m_wndLimit);
//	SetWindowLongPtr(pWnd, GWLP_USERDATA, (LONG)this);
//	m_funOldProc = (WNDPROC)SetWindowLongPtr(pWnd, GWLP_WNDPROC, (LONG_PTR)HookWndProc);
//
//	MoveWindow(pWnd, rcWnd.left, rcWnd.top, rcWnd.right - rcWnd.left, rcWnd.bottom - rcWnd.top, FALSE);
//	SetTimer(m_hWnd, TIMER_UPDATE, 500, NULL);
//	return TRUE;
//}
//
//void CSkinScrollWnd::OnSize(UINT nType, int cx, int cy)
//{
//	CWnd::OnSize(nType, cx, cy);
//	if (cx == 0 || cy == 0) return;
//	if (m_wndLimit.m_hWnd)
//	{
//		CWnd* pWnd = m_wndLimit.GetWindow(GW_CHILD);
//		pWnd->MoveWindow(0, 0, cx, cy);
//	}
//}
//
//void CSkinScrollWnd::OnVScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar)
//{
//	CWnd* pWnd = m_wndLimit.GetWindow(GW_CHILD);
//	m_bOp = TRUE;
//	pWnd->SendMessage(WM_VSCROLL, MAKELONG(nSBCode, nPos), 0);
//	m_bOp = FALSE;
//	if (nSBCode == SB_THUMBTRACK) return;
//	SCROLLINFO si = { 0 };
//	si.cbSize = sizeof(si);
//	si.fMask = SIF_ALL;
//	pWnd->GetScrollInfo(SB_VERT, &si);
//	pScrollBar->SetScrollInfo(&si, TRUE);
//}
//
//void CSkinScrollWnd::OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar)
//{
//	CWnd* pWnd = m_wndLimit.GetWindow(GW_CHILD);
//	m_bOp = TRUE;
//	pWnd->SendMessage(WM_HSCROLL, MAKELONG(nSBCode, nPos), 0);
//	m_bOp = FALSE;
//	if (nSBCode == SB_THUMBTRACK) return;
//	SCROLLINFO si = { 0 };
//	si.cbSize = sizeof(si);
//	si.fMask = SIF_ALL;
//	pWnd->GetScrollInfo(SB_HORZ, &si);
//	pScrollBar->SetScrollInfo(&si, TRUE);
//}
//
//void CSkinScrollWnd::OnPaint()
//{
//	CPaintDC dc(this); // device context for painting
//	CDC memdc;
//	memdc.CreateCompatibleDC(&dc);
//	HGDIOBJ hOldBmp = ::SelectObject(memdc, m_hBmpScroll);
//	RECT rc;
//	GetClientRect(&rc);
//	if (m_nAngleType == 1)
//		dc.BitBlt(rc.left, rc.bottom - m_nScrollWid, m_nScrollWid, m_nScrollWid, &memdc, m_nScrollWid * 8, m_nScrollWid * 1, SRCCOPY);
//	else if (m_nAngleType == 2)
//		dc.BitBlt(rc.right - m_nScrollWid, rc.bottom - m_nScrollWid, m_nScrollWid, m_nScrollWid, &memdc, m_nScrollWid * 8, m_nScrollWid * 0, SRCCOPY);
//	::SelectObject(memdc, hOldBmp);
//}
//
//void CSkinScrollWnd::OnTimer(UINT nIDEvent)
//{
//	CWnd* pWnd = m_wndLimit.GetWindow(GW_CHILD);
//	SCROLLINFO si1 = { 0 };
//	si1.cbSize = sizeof(SCROLLINFO);
//	si1.fMask = SIF_ALL;
//	SCROLLINFO si2 = { 0 };
//	if (GetWindowLong(m_sbVert.m_hWnd, GWL_STYLE) & WS_VISIBLE && !m_sbVert.m_bDrag)
//	{
//		pWnd->GetScrollInfo(SB_VERT, &si1);
//		m_sbVert.GetScrollInfo(&si2, SIF_ALL);
//		if (si1.nMax != si2.nMax || si1.nMin != si2.nMin || si1.nPos != si2.nPos || si1.nPage != si2.nPage)
//			m_sbVert.SetScrollInfo(&si1);
//	}
//	if (GetWindowLong(m_sbHorz.m_hWnd, GWL_STYLE) & WS_VISIBLE && !m_sbHorz.m_bDrag)
//	{
//		pWnd->GetScrollInfo(SB_HORZ, &si1);
//		m_sbHorz.GetScrollInfo(&si2, SIF_ALL);
//		if (si1.nMax != si2.nMax || si1.nMin != si2.nMin || si1.nPos != si2.nPos || si1.nPage != si2.nPage)
//			m_sbHorz.SetScrollInfo(&si1);
//	}
//}
//
//LRESULT CSkinScrollWnd::OnDestMove(WPARAM wParam, LPARAM lParam)
//{
//	m_bOp = TRUE;
//	BOOL bVScroll = wParam;
//	BOOL bLeftScroll = lParam;
//	RECT rcClient;
//	GetClientRect(&rcClient);
//	RECT rcLimit, rcWnd;
//	rcWnd = rcClient;
//	rcLimit = rcClient;
//	if (GetWindowLong(m_sbHorz.m_hWnd, GWL_STYLE) & WS_VISIBLE) rcLimit.bottom -= m_nScrollWid;
//	if (bLeftScroll)
//	{
//		if (bVScroll)
//		{
//			rcLimit.left += m_nScrollWid;
//			OffsetRect(&rcWnd, -m_nScrollWid, 0);
//		}
//	}
//	else
//	{
//		if (bVScroll) rcLimit.right -= m_nScrollWid;
//	}
//	m_wndLimit.MoveWindow(&rcLimit);
//	m_wndLimit.GetWindow(GW_CHILD)->SetWindowPos(NULL, rcWnd.left, 0, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
//	m_bOp = FALSE;
//	return 0;
//}

LRESULT CALLBACK StaticProc1(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	static HWND hEdit;
	static CSkinScrollBar* pSbmVert;

	switch (msg)
	{
	case WM_APP + 1:
		pSbmVert = (CSkinScrollBar*)lParam;
		break;
	case WM_COMMAND:
		if (pSbmVert)
		{
			if (LOWORD(wParam) == 1001)
			{
				if (HIWORD(wParam) == EN_VSCROLL || HIWORD(wParam) == EN_UPDATE)
				{
					SCROLLINFO si = { 0 };
					si.cbSize = sizeof(si);
					si.fMask = SIF_ALL;
					GetScrollInfo(hEdit, SB_VERT, &si);
					SetScrollInfo(pSbmVert->m_hWnd, SB_CTL, &si, TRUE);
				}
			}
		}
		break;
	case WM_APP:
		hEdit = (HWND)lParam;
		break;
	case WM_SIZE:
		if (hEdit)
		{
			int nScrollBarWidth = GetSystemMetrics(SM_CXVSCROLL);
			int nWidth = LOWORD(lParam);
			int nHeight = HIWORD(lParam);
			MoveWindow(hEdit, 0, 0, nWidth + nScrollBarWidth, nHeight, FALSE);
		}
		break;
	default:
		break;
	}
	return CallWindowProc(defStaticWndProc, hWnd, msg, wParam, lParam);
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	static HWND hEdit;
	static HWND hStatic;
	static HFONT hFont;
	static UINT uDpiX = DEFAULT_DPI, uDpiY = DEFAULT_DPI;
	static HBITMAP m_hBmpScroll;
	static CSkinScrollBar m_sbHorz;
	switch (msg)
	{
	case WM_CREATE:
		m_hBmpScroll = LoadBitmap(((LPCREATESTRUCT)lParam)->hInstance, MAKEINTRESOURCE(IDB_BITMAP1));

		hStatic = CreateWindow(TEXT("STATIC"), 0, WS_VISIBLE | WS_CHILD | WS_CLIPCHILDREN, 0, 0, 0, 0, hWnd, (HMENU)1000, ((LPCREATESTRUCT)lParam)->hInstance, 0);
		hEdit = CreateWindow(TEXT("EDIT"), 0, WS_VISIBLE | WS_CHILD | WS_VSCROLL | ES_MULTILINE | ES_AUTOHSCROLL | ES_AUTOVSCROLL, 0, 0, 0, 0, hStatic, (HMENU)1001, ((LPCREATESTRUCT)lParam)->hInstance, 0);

		defStaticWndProc = (WNDPROC)SetWindowLongPtr(hStatic, GWLP_WNDPROC, (LONG_PTR)StaticProc1);
		SendMessage(hStatic, WM_APP, 0, (LPARAM)hEdit);

		m_sbHorz.m_hWnd = CreateWindow(L"SCROLLBAR", 0, WS_CHILD | WS_VISIBLE | SBS_VERT, 0, 0, 0, 0, hWnd, (HMENU)100, GetModuleHandle(0), 0);
		m_sbHorz.SetBitmap(m_hBmpScroll);

		SendMessage(hStatic, WM_APP + 1, 0, (LPARAM)&m_sbHorz);

		{
			defScrollBarWndProc = (WNDPROC)SetWindowLongPtr(m_sbHorz.m_hWnd, GWLP_WNDPROC, (LONG_PTR)m_sbHorz.GlobalScrollBarProc);
			SetProp(m_sbHorz.m_hWnd, SZCECOWIZSCROLLBARPROC, (HANDLE)&m_sbHorz);
		}
		SendMessage(hWnd, WM_DPICHANGED, 0, 0);
		break;
	case WM_SIZE:
		MoveWindow(hStatic, POINT2PIXEL(10), POINT2PIXEL(10), LOWORD(lParam) - POINT2PIXEL(40), HIWORD(lParam) - POINT2PIXEL(20), TRUE);
		MoveWindow(m_sbHorz.m_hWnd, POINT2PIXEL(10) + LOWORD(lParam) - POINT2PIXEL(40), POINT2PIXEL(10), POINT2PIXEL(10), HIWORD(lParam) - POINT2PIXEL(20), TRUE);
		break;
	case WM_VSCROLL:
		{
			SendMessage(hEdit, WM_VSCROLL, wParam, lParam);
			if (LOWORD(wParam) != SB_THUMBTRACK)
			{
				SCROLLINFO si = { 0 };
				si.cbSize = sizeof(si);
				si.fMask = SIF_ALL;
				GetScrollInfo(hEdit, SB_VERT, &si);
				SetScrollInfo(m_sbHorz.m_hWnd, SB_CTL, &si, TRUE);
			}
		}
		break;
	case WM_COMMAND:
		if (LOWORD(wParam) == IDOK)
		{
			SetWindowText(hEdit, 0);
			TCHAR szText[1024];
			wsprintf(szText, TEXT("%d"), GetTickCount());
			SendMessage(hEdit, EM_REPLACESEL, 0, (LPARAM)szText);
		}
		break;
	case WM_NCCREATE:
		{
			const HMODULE hModUser32 = GetModuleHandle(TEXT("user32.dll"));
			if (hModUser32)
			{
				typedef BOOL(WINAPI*fnTypeEnableNCScaling)(HWND);
				const fnTypeEnableNCScaling fnEnableNCScaling = (fnTypeEnableNCScaling)GetProcAddress(hModUser32, "EnableNonClientDpiScaling");
				if (fnEnableNCScaling)
				{
					fnEnableNCScaling(hWnd);
				}
			}
		}
		return DefWindowProc(hWnd, msg, wParam, lParam);
	case WM_DPICHANGED:
		GetScaling(hWnd, &uDpiX, &uDpiY);
		DeleteObject(hFont);
		hFont = CreateFontW(-POINT2PIXEL(10), 0, 0, 0, FW_NORMAL, 0, 0, 0, SHIFTJIS_CHARSET, 0, 0, 0, 0, L"MS Shell Dlg");
		SendMessage(hEdit, WM_SETFONT, (WPARAM)hFont, 0);
		break;
	case WM_DESTROY:
		DeleteObject(m_hBmpScroll);
		DeleteObject(hFont);
		PostQuitMessage(0);
		break;
	default:
		return DefWindowProc(hWnd, msg, wParam, lParam);
	}
	return 0;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPreInst, LPSTR pCmdLine, int nCmdShow)
{
	MSG msg;
	WNDCLASS wndclass = {
		CS_HREDRAW | CS_VREDRAW,
		WndProc,
		0,
		0,
		hInstance,
		0,
		LoadCursor(0,IDC_ARROW),
		(HBRUSH)(COLOR_WINDOW + 1),
		0,
		szClassName
	};
	RegisterClass(&wndclass);
	HWND hWnd = CreateWindow(
		szClassName,
		TEXT("Window"),
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
	while (GetMessage(&msg, 0, 0, 0))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
	return (int)msg.wParam;
}
