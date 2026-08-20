#pragma once

#include <d2d1_1.h>
#include <dwrite.h>
#include <wrl/client.h>
#include <string>

using Microsoft::WRL::ComPtr;

class Renderer {
public:
    Renderer(ID2D1RenderTarget* renderTarget, IDWriteFactory* dwriteFactory);
    
    void DrawTextW(const std::wstring& text, float x, float y, float width, float height, float fontSize = 14.0f, D2D1::ColorF color = D2D1::ColorF::White, DWRITE_FONT_WEIGHT weight = DWRITE_FONT_WEIGHT_NORMAL, bool centerAlign = false);
    void DrawIcon(wchar_t iconGlyph, float x, float y, float size, D2D1::ColorF color);
    void FillRectangle(float x, float y, float width, float height, D2D1::ColorF color);
    void DrawRectangle(float x, float y, float width, float height, D2D1::ColorF color, float strokeWidth = 1.0f);
    void FillRoundedRectangle(float x, float y, float width, float height, float radiusX, float radiusY, D2D1::ColorF color);
    void DrawRoundedRectangle(float x, float y, float width, float height, float radiusX, float radiusY, D2D1::ColorF color, float strokeWidth = 1.0f);
    void DrawImage(const std::wstring& imagePath, float x, float y, float width, float height);
    
    ID2D1RenderTarget* GetRenderTarget() const { return m_renderTarget; }

private:
    ID2D1RenderTarget* m_renderTarget;
    IDWriteFactory* m_dwriteFactory;
    ComPtr<ID2D1SolidColorBrush> m_brush;
    ComPtr<IDWriteTextFormat> m_textFormat;
};
