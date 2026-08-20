#include <wincodec.h>
#include <combaseapi.h>
#include "Renderer.h"
#pragma comment(lib, "windowscodecs.lib")

Renderer::Renderer(ID2D1RenderTarget* renderTarget, IDWriteFactory* dwriteFactory)
    : m_renderTarget(renderTarget), m_dwriteFactory(dwriteFactory) {
    m_renderTarget->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::White), &m_brush);
}

void Renderer::DrawTextW(const std::wstring& text, float x, float y, float width, float height, float fontSize, D2D1::ColorF color, DWRITE_FONT_WEIGHT weight, bool centerAlign) {
    m_brush->SetColor(color);
    
    ComPtr<IDWriteTextFormat> format;
    m_dwriteFactory->CreateTextFormat(
        L"Segoe UI Variable Text", nullptr, weight, DWRITE_FONT_STYLE_NORMAL, 
        DWRITE_FONT_STRETCH_NORMAL, fontSize, L"en-us", &format);
    
    if (!format) {
        // Fallback
        m_dwriteFactory->CreateTextFormat(
            L"Segoe UI", nullptr, weight, DWRITE_FONT_STYLE_NORMAL, 
            DWRITE_FONT_STRETCH_NORMAL, fontSize, L"en-us", &format);
    }

    if (format && centerAlign) {
        format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
        format->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    }
        
    m_renderTarget->DrawText(
        text.c_str(),
        text.length(),
        format.Get(),
        D2D1::RectF(x, y, x + width, y + height),
        m_brush.Get(),
        D2D1_DRAW_TEXT_OPTIONS_NONE,
        DWRITE_MEASURING_MODE_NATURAL
    );
}

void Renderer::DrawIcon(wchar_t iconGlyph, float x, float y, float size, D2D1::ColorF color) {
    ComPtr<IDWriteTextFormat> iconFormat;
    m_dwriteFactory->CreateTextFormat(
        L"Segoe Fluent Icons",
        nullptr,
        DWRITE_FONT_WEIGHT_NORMAL,
        DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL,
        size,
        L"en-us",
        &iconFormat
    );
    
    m_brush->SetColor(color);
    std::wstring text(1, iconGlyph);
    m_renderTarget->DrawText(
        text.c_str(),
        1,
        iconFormat.Get(),
        D2D1::RectF(x, y, x + size * 1.5f, y + size * 1.5f),
        m_brush.Get(),
        D2D1_DRAW_TEXT_OPTIONS_NONE,
        DWRITE_MEASURING_MODE_NATURAL
    );
}

void Renderer::FillRectangle(float x, float y, float width, float height, D2D1::ColorF color) {
    m_brush->SetColor(color);
    m_renderTarget->FillRectangle(D2D1::RectF(x, y, x + width, y + height), m_brush.Get());
}

void Renderer::DrawRectangle(float x, float y, float width, float height, D2D1::ColorF color, float strokeWidth) {
    m_brush->SetColor(color);
    m_renderTarget->DrawRectangle(D2D1::RectF(x, y, x + width, y + height), m_brush.Get(), strokeWidth);
}

void Renderer::FillRoundedRectangle(float x, float y, float width, float height, float radiusX, float radiusY, D2D1::ColorF color) {
    m_brush->SetColor(color);
    D2D1_ROUNDED_RECT roundedRect = D2D1::RoundedRect(D2D1::RectF(x, y, x + width, y + height), radiusX, radiusY);
    m_renderTarget->FillRoundedRectangle(roundedRect, m_brush.Get());
}

void Renderer::DrawRoundedRectangle(float x, float y, float width, float height, float radiusX, float radiusY, D2D1::ColorF color, float strokeWidth) {
    m_brush->SetColor(color);
    D2D1_ROUNDED_RECT roundedRect = D2D1::RoundedRect(D2D1::RectF(x, y, x + width, y + height), radiusX, radiusY);
    m_renderTarget->DrawRoundedRectangle(roundedRect, m_brush.Get(), strokeWidth);
}

// Draw image from file using WIC
void Renderer::DrawImage(const std::wstring& imagePath, float x, float y, float width, float height) {
    // Ensure COM is initialized for this thread
    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) {
        return; // Initialization failed
    }
    // Create WIC factory if not already created
    static IWICImagingFactory* wicFactory = nullptr;
    if (!wicFactory) {
        hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&wicFactory));
        if (FAILED(hr)) return;
    }
    ComPtr<IWICBitmapDecoder> decoder;
    hr = wicFactory->CreateDecoderFromFilename(imagePath.c_str(), nullptr, GENERIC_READ, WICDecodeMetadataCacheOnLoad, &decoder);
    if (FAILED(hr)) return;
    ComPtr<IWICBitmapFrameDecode> frame;
    hr = decoder->GetFrame(0, &frame);
    if (FAILED(hr)) return;
    ComPtr<IWICFormatConverter> converter;
    hr = wicFactory->CreateFormatConverter(&converter);
    if (FAILED(hr)) return;
    hr = converter->Initialize(frame.Get(), GUID_WICPixelFormat32bppPBGRA, WICBitmapDitherTypeNone, nullptr, 0.f, WICBitmapPaletteTypeCustom);
    if (FAILED(hr)) return;
    ComPtr<ID2D1Bitmap> bitmap;
    hr = m_renderTarget->CreateBitmapFromWicBitmap(converter.Get(), nullptr, &bitmap);
    if (FAILED(hr)) return;
    // Draw the bitmap
    m_renderTarget->DrawBitmap(bitmap.Get(), D2D1::RectF(x, y, x + width, y + height), 1.0f);
}
