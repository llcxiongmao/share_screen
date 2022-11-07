#pragma once
#include <llc/share_screen/common.hpp>

namespace llc {
namespace share_screen {
enum class RenderType { GL, DX11 };

/** impl draw frame. */
class Render : public util::NonCopyable {
public:
    Render(RenderType type) : mType(type) {}

    virtual ~Render() {}

    RenderType getType() const {
        return mType;
    }

    /**
     * exec draw.
     *
     * @param frame frame to draw.
     * @throws Error if resize image fail or format not supported.
     */
    virtual void paint(PaintFrame* frame) = 0;

    /**
     * call when win resize.
     *
     * @param w current width.
     * @param h current height.
     * @throws Error if recreate window's resource fail.
     */
    virtual void resize(int w, int h) = 0;

private:
    RenderType mType;
};

//--

/** opengl impl for Render, support AV_PIX_FMT_YUV420P. */
class GlRender : public Render {
public:
    /**
     * init.
     *
     * @throws Error if create opengl object fail or sys call fail.
     */
    GlRender();

    ~GlRender() override;

    void paint(PaintFrame* frame) override;

    void resize(int w, int h) override;

private:
    void checkImgResize(int w, int h, int hw, int hh);

private:
    PFNWGLGETEXTENSIONSSTRINGEXTPROC wglGetExtensionsStringEXT = nullptr;
    PFNWGLCREATECONTEXTATTRIBSARBPROC wglCreateContextAttribsARB = nullptr;
    PFNGLCREATESHADERPROC glCreateShader = nullptr;
    PFNGLDELETESHADERPROC glDeleteShader = nullptr;
    PFNGLSHADERSOURCEPROC glShaderSource = nullptr;
    PFNGLCOMPILESHADERPROC glCompileShader = nullptr;
    PFNGLGETSHADERIVPROC glGetShaderiv = nullptr;
    PFNGLGETSHADERINFOLOGPROC glGetShaderInfoLog = nullptr;
    PFNGLCREATEPROGRAMPROC glCreateProgram = nullptr;
    PFNGLDELETEPROGRAMPROC glDeleteProgram = nullptr;
    PFNGLATTACHSHADERPROC glAttachShader = nullptr;
    PFNGLLINKPROGRAMPROC glLinkProgram = nullptr;
    PFNGLGETPROGRAMIVPROC glGetProgramiv = nullptr;
    PFNGLGETPROGRAMINFOLOGPROC glGetProgramInfoLog = nullptr;
    PFNGLUSEPROGRAMPROC glUseProgram = nullptr;
    PFNGLGETUNIFORMLOCATIONPROC glGetUniformLocation = nullptr;
    PFNGLUNIFORM1IPROC glUniform1i = nullptr;
    PFNGLVERTEXATTRIBPOINTERPROC glVertexAttribPointer = nullptr;
    PFNGLENABLEVERTEXATTRIBARRAYPROC glEnableVertexAttribArray = nullptr;
    PFNGLACTIVETEXTUREPROC glActiveTexture = nullptr;
    PFNWGLSWAPINTERVALEXTPROC wglSwapIntervalEXT = nullptr;

    /** current win width. */
    int mWinW = 0;
    /** current win height. */
    int mWinH = 0;

    HDC mDc = 0;
    HGLRC mRc = 0;
    GLuint mImgY = 0;
    GLuint mImgU = 0;
    GLuint mImgV = 0;
    int mImgW = 0;
    int mImgH = 0;
    GLuint mVs = 0;
    GLuint mFs = 0;
    GLuint mProgram = 0;
};

//--

/** Dx11 impl for Render, support AV_PIX_FMT_YUV420P, AV_PIX_FMT_D3D11. */
class Dx11Render : public Render {
public:
    /**
     * init.
     *
     * @throws Error if create dx11 object fail.
     */
    Dx11Render();

    ~Dx11Render() override;

    AVBufferRef* getHwCtx() const {
        return mHwCtx.get();
    }

    void paint(PaintFrame* frame) override;

    void resize(int w, int h) override;

private:
    void checkImgResize(int w, int h, int hw, int hh);

    void checkImgResize_Nv12(ID3D11Texture2D* decodeImg);

private:
    /** current win width. */
    int mWinW = 0;
    /** current win height. */
    int mWinH = 0;

    std::unique_ptr<AVBufferRef> mHwCtx;
    ComPtr<ID3D11Device> mDev;
    ComPtr<ID3D11DeviceContext> mDevCtx;
    ComPtr<IDXGIFactory> mGiFactory;
    ComPtr<IDXGISwapChain> mSwapChain;
    ComPtr<ID3D11RenderTargetView> mRt;

    /** last frame format. */
    AVPixelFormat mLastFormat = AV_PIX_FMT_NONE;

    ComPtr<ID3D11SamplerState> mSampler;

    int mImgW = 0;
    int mImgH = 0;
    ComPtr<ID3D11Texture2D> mImgY;
    ComPtr<ID3D11Texture2D> mImgU;
    ComPtr<ID3D11Texture2D> mImgV;
    ComPtr<ID3D11ShaderResourceView> mTexY;
    ComPtr<ID3D11ShaderResourceView> mTexU;
    ComPtr<ID3D11ShaderResourceView> mTexV;
    ComPtr<ID3D11VertexShader> mVs;
    ComPtr<ID3D11PixelShader> mPs;

    int mImgW_Nv12 = 0;
    int mImgH_Nv12 = 0;
    ComPtr<ID3D11VertexShader> mVs_Nv12;
    ComPtr<ID3D11PixelShader> mPs_Nv12;
    ComPtr<ID3D11Texture2D> mImg_Nv12;
    ComPtr<ID3D11ShaderResourceView> mTex_Y_Nv12;
    ComPtr<ID3D11ShaderResourceView> mTex_Uv_Nv12;
};
}  // namespace share_screen
}  // namespace llc
