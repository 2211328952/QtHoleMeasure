#pragma once

#include "HoleMeasureCore.h"
#include "LPVCommon.h"
#include <QLabel>

#include <windows.h>
#include <QtWin>

class DisplayHelper
{
public:
    DisplayHelper(int imageWidth, int imageHeight, QLabel* label, double viewScale = 1.0, double viewPanX = 0.0, double viewPanY = 0.0)
    {
        const QRect rect = label->contentsRect();
        const hm::ViewTransform transform = hm::makeViewTransform(
            imageWidth, imageHeight, rect.width(), rect.height(), hm::ViewState{ viewScale, viewPanX, viewPanY });
        zoomx = transform.zoomX;
        zoomy = transform.zoomY;
        panx = transform.panX;
        pany = transform.panY;
        pic = label;

        pdc = GetDC(nullptr);
        hdc = CreateCompatibleDC(pdc);
        hbmp = CreateCompatibleBitmap(pdc, rect.width(), rect.height());
        SelectObject(hdc, hbmp);

        RECT bg = { 0, 0, rect.width(), rect.height() };
        FillRect(hdc, &bg, static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)));
    }

    ~DisplayHelper()
    {
        pic->setPixmap(QtWin::fromHBITMAP(hbmp));
        ReleaseDC(nullptr, pdc);
        DeleteObject(hdc);
        DeleteObject(hbmp);
    }

    double zoomx = 0.0;
    double zoomy = 0.0;
    double panx = 0.0;
    double pany = 0.0;
    QLabel* pic = nullptr;
    HDC pdc = nullptr;
    HDC hdc = nullptr;
    HBITMAP hbmp = nullptr;
};
