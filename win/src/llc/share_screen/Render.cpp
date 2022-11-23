#include <llc/share_screen/Render.hpp>
#include <llc/share_screen/FrontThread.hpp>
#include <llc/share_screen/BackThread.hpp>

#define QUERY_GL_FN(_P, _NAME)                                     \
    _P = reinterpret_cast<decltype(_P)>(wglGetProcAddress(_NAME)); \
    THROW_IF(_P, "query gl fn " _NAME "fail")

namespace llc {
namespace share_screen {
GlRender::GlRender() : Render(RenderType::GL) {
    try {
        mDc = GetDC(FrontThread::GetSingleton()->getHwnd());
        THROW_IF(mDc, "GetDC fail: " << Error::GetWin32String());

        PIXELFORMATDESCRIPTOR pfd = {};
        memset(&pfd, 0, sizeof(PIXELFORMATDESCRIPTOR));
        pfd.nSize = sizeof(PIXELFORMATDESCRIPTOR);
        pfd.nVersion = 1;
        pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
        pfd.iPixelType = PFD_TYPE_RGBA;
        pfd.cColorBits = 32;
        pfd.iLayerType = PFD_MAIN_PLANE;
        int pixelFormat = ChoosePixelFormat(mDc, &pfd);
        THROW_IF(pixelFormat, "ChoosePixelFormat fail: " << Error::GetWin32String());
        THROW_IF(SetPixelFormat(mDc, pixelFormat, &pfd),
                 "SetPixelFormat fail: " << Error::GetWin32String());

        HGLRC rc_1_0 = wglCreateContext(mDc);
        THROW_IF(rc_1_0, "wglCreateContext fail: " << Error::GetWin32String());
        THROW_IF(wglMakeCurrent(mDc, rc_1_0), "wglMakeCurrent fail: " << Error::GetWin32String());

        QUERY_GL_FN(wglGetExtensionsStringEXT, "wglGetExtensionsStringEXT");
        QUERY_GL_FN(wglCreateContextAttribsARB, "wglCreateContextAttribsARB");
        QUERY_GL_FN(glCreateShader, "glCreateShader");
        QUERY_GL_FN(glDeleteShader, "glDeleteShader");
        QUERY_GL_FN(glShaderSource, "glShaderSource");
        QUERY_GL_FN(glCompileShader, "glCompileShader");
        QUERY_GL_FN(glGetShaderiv, "glGetShaderiv");
        QUERY_GL_FN(glGetShaderInfoLog, "glGetShaderInfoLog");
        QUERY_GL_FN(glCreateProgram, "glCreateProgram");
        QUERY_GL_FN(glDeleteProgram, "glDeleteProgram");
        QUERY_GL_FN(glAttachShader, "glAttachShader");
        QUERY_GL_FN(glLinkProgram, "glLinkProgram");
        QUERY_GL_FN(glGetProgramiv, "glGetProgramiv");
        QUERY_GL_FN(glGetProgramInfoLog, "glGetProgramInfoLog");
        QUERY_GL_FN(glUseProgram, "glUseProgram");
        QUERY_GL_FN(glGetUniformLocation, "glGetUniformLocation");
        QUERY_GL_FN(glUniform1i, "glUniform1i");
        QUERY_GL_FN(glVertexAttribPointer, "glVertexAttribPointer");
        QUERY_GL_FN(glEnableVertexAttribArray, "glEnableVertexAttribArray");
        QUERY_GL_FN(glActiveTexture, "glActiveTexture");

        GLint result = GL_FALSE;

        static constexpr int contextAttribs[] = {
            WGL_CONTEXT_MAJOR_VERSION_ARB,
            2,
            WGL_CONTEXT_MINOR_VERSION_ARB,
            1,
            WGL_CONTEXT_FLAGS_ARB,
            0,
            0,
        };

        mRc = wglCreateContextAttribsARB(mDc, 0, contextAttribs);
        THROW_IF(mRc, "wglCreateContextAttribsARB");
        THROW_IF(wglMakeCurrent(NULL, NULL), "wglMakeCurrent fail: " << Error::GetWin32String());
        THROW_IF(wglDeleteContext(rc_1_0), "wglDeleteContext fail: " << Error::GetWin32String());
        THROW_IF(wglMakeCurrent(mDc, mRc), "wglMakeCurrent fail: " << Error::GetWin32String());

        if (strstr(wglGetExtensionsStringEXT(), "WGL_EXT_swap_control") != nullptr) {
            QUERY_GL_FN(wglSwapIntervalEXT, "wglSwapIntervalEXT");
            log::i() << "disable vsync";
            wglSwapIntervalEXT(0);
        } else {
            log::i() << "WGL_EXT_swap_control not found";
        }

        static constexpr const char* VS = R"(
attribute vec2 iPos;
attribute vec2 iUv;
varying vec2 uv;
void main() {
    gl_Position = vec4(iPos, 1, 1);
    uv = iUv;
})";
        mVs = glCreateShader(GL_VERTEX_SHADER);
        THROW_IF(mVs, "create vs shader");
        glShaderSource(mVs, 1, &VS, NULL);
        glCompileShader(mVs);
        glGetShaderiv(mVs, GL_COMPILE_STATUS, &result);
        if (result == GL_FALSE) {
            GLint errStrLen = 0;
            glGetShaderiv(mVs, GL_INFO_LOG_LENGTH, &errStrLen);
            if (errStrLen) {
                std::string errStr((size_t)errStrLen, 0);
                glGetShaderInfoLog(mVs, errStrLen, &errStrLen, &errStr[0]);
                log::e() << "compile vs fail:\n" << errStr;
            }
            THROW_IF(0, "compile vs fail");
        }

        static const char* const FS = R"(
varying vec2 uv;
uniform sampler2D texY;
uniform sampler2D texU;
uniform sampler2D texV;
void main() {
    vec4 yuv;
    yuv.x = texture2D(texY, uv).x;
    yuv.y = texture2D(texU, uv).x;
    yuv.z = texture2D(texV, uv).x;
    yuv.w = 1;
    mat4 m = mat4(
        1.0,  0.0,    1.402, -0.701,
        1.0, -0.344, -0.714,  0.529,
        1.0,  1.772,  0.0,   -0.886,
        0,    0,      0,      1);
    gl_FragColor = yuv * m;
})";
        mFs = glCreateShader(GL_FRAGMENT_SHADER);
        THROW_IF(mFs, "create fs shader fail");
        glShaderSource(mFs, 1, &FS, NULL);
        glCompileShader(mFs);
        glGetShaderiv(mFs, GL_COMPILE_STATUS, &result);
        if (result == GL_FALSE) {
            GLint errStrLen = 0;
            glGetShaderiv(mFs, GL_INFO_LOG_LENGTH, &errStrLen);
            if (errStrLen) {
                std::string errStr((size_t)errStrLen, 0);
                glGetShaderInfoLog(mFs, errStrLen, &errStrLen, &errStr[0]);
                log::e() << "compile fs fail:\n" << errStr;
            }
            THROW_IF(0, "compile fs fail");
        }

        mProgram = glCreateProgram();
        THROW_IF(mProgram, "create gl program fail");
        glAttachShader(mProgram, mVs);
        glAttachShader(mProgram, mFs);
        glLinkProgram(mProgram);
        glGetProgramiv(mProgram, GL_LINK_STATUS, &result);
        if (result == GL_FALSE) {
            GLint errStrLen = 0;
            glGetProgramiv(mProgram, GL_INFO_LOG_LENGTH, &errStrLen);
            if (errStrLen) {
                std::string errStr((size_t)errStrLen, 0);
                glGetProgramInfoLog(mProgram, errStrLen, &errStrLen, &errStr[0]);
                log::i() << "link shaders fail:\n" << errStr;
            }
            THROW_IF(0, "link gl program fail");
        }
        glUseProgram(mProgram);

        GLint locY = glGetUniformLocation(mProgram, "texY");
        GLint locU = glGetUniformLocation(mProgram, "texU");
        GLint locV = glGetUniformLocation(mProgram, "texV");
        glUniform1i(locY, 0);
        glUniform1i(locU, 1);
        glUniform1i(locV, 2);

        static const GLfloat poss[] = {
            -1.0f,
            -1.0f,
            //
            1.0f,
            -1.0f,
            //
            -1.0f,
            1.0f,
            //
            1.0f,
            -1.0f,
            //
            1.0f,
            1.0f,
            //
            -1.0f,
            1.0f,
        };
        glVertexAttribPointer(0, 2, GL_FLOAT, 0, 0, poss);
        glEnableVertexAttribArray(0);

        static const GLfloat uvs[] = {
            0.0f,
            1.0f,
            //
            1.0f,
            1.0f,
            //
            0.0f,
            0.0f,
            //
            1.0f,
            1.0f,
            //
            1.0f,
            0.0f,
            //
            0.0f,
            0.0f,
        };
        glVertexAttribPointer(1, 2, GL_FLOAT, 0, 0, uvs);
        glEnableVertexAttribArray(1);
    } catch (...) {
        this->~GlRender();
        throw;
    }
}

GlRender::~GlRender() {
    if (mProgram) {
        glDeleteProgram(mProgram);
        mProgram = NULL;
    }
    if (mVs) {
        glDeleteShader(mVs);
        mVs = NULL;
    }
    if (mFs) {
        glDeleteShader(mFs);
        mFs = NULL;
    }
    if (mImgY) {
        glDeleteTextures(1, &mImgY);
        mImgY = NULL;
    }
    if (mImgU) {
        glDeleteTextures(1, &mImgU);
        mImgU = NULL;
    }
    if (mImgV) {
        glDeleteTextures(1, &mImgV);
        mImgV = NULL;
    }
    if (mRc) {
        wglMakeCurrent(NULL, NULL);
        wglDeleteContext(mRc);
        mRc = NULL;
    }
}

void GlRender::paint(PaintFrame* frame) {
    // not paint if minimum.
    if (mWinW == 0 || mWinH == 0)
        return;

    if (frame) {
        AVFrame* decodeF = frame->decodeFrame.get();

        if (decodeF->format != AV_PIX_FMT_YUV420P) {
            const char* formatName = av_get_pix_fmt_name((AVPixelFormat)frame->decodeFrame->format);
            if (!formatName)
                formatName = "unknown";
            THROW_IF(0, "format not supported, expect: AV_PIX_FMT_YUV420P, now: " << formatName);
        }

        int w = decodeF->width;
        int h = decodeF->height;
        int hw = w / 2;
        int hh = h / 2;
        checkImgResize(w, h, hw, hh);

        // update y-image data.
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, mImgY);
        glPixelStorei(GL_UNPACK_ROW_LENGTH, (GLint)decodeF->linesize[0]);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, w, h, GL_RED, GL_UNSIGNED_BYTE, decodeF->data[0]);

        // update v-image data.
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, mImgU);
        glPixelStorei(GL_UNPACK_ROW_LENGTH, (GLint)decodeF->linesize[1]);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, hw, hh, GL_RED, GL_UNSIGNED_BYTE, decodeF->data[1]);

        // update v-image data.
        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, mImgV);
        glPixelStorei(GL_UNPACK_ROW_LENGTH, (GLint)decodeF->linesize[2]);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, hw, hh, GL_RED, GL_UNSIGNED_BYTE, decodeF->data[2]);
    }

    if (mImgY) {
        glDrawArrays(GL_TRIANGLES, 0, 6);
    } else {
        glClearColor(0.5f, 0.5f, 0.5f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
    }

    SwapBuffers(mDc);
}

void GlRender::resize(int w, int h) {
    log::i() << "win resize change, w: " << w << ", h: " << h;
    mWinW = w;
    mWinH = h;

    if (!mWinW || !mWinH)
        return;

    glViewport(0, 0, mWinW, mWinH);

    // force draw one frame.
    paint(nullptr);
}

void GlRender::checkImgResize(int w, int h, int hw, int hh) {
    if (mImgW == w && mImgH == h)
        return;

    if (mImgY != 0) {
        glDeleteTextures(1, &mImgV);
        glDeleteTextures(1, &mImgU);
        glDeleteTextures(1, &mImgY);
        mImgY = 0;
        mImgU = 0;
        mImgV = 0;
    }

    glGenTextures(1, &mImgY);
    THROW_IF(mImgY, "create gl img y fail");
    glGenTextures(1, &mImgU);
    THROW_IF(mImgU, "create gl img u fail");
    glGenTextures(1, &mImgV);
    THROW_IF(mImgV, "create gl img v fail");

    glBindTexture(GL_TEXTURE_2D, mImgY);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, w, h, 0, GL_RED, GL_UNSIGNED_BYTE, nullptr);

    glBindTexture(GL_TEXTURE_2D, mImgU);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, hw, hh, 0, GL_RED, GL_UNSIGNED_BYTE, nullptr);

    glBindTexture(GL_TEXTURE_2D, mImgV);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, hw, hh, 0, GL_RED, GL_UNSIGNED_BYTE, nullptr);

    mImgW = w;
    mImgH = h;
}

//--

Dx11Render::Dx11Render() : Render(RenderType::DX11) {
    AVBufferRef* pHwCtx = nullptr;
    THROW_IF_AV(av_hwdevice_ctx_create(&pHwCtx, AV_HWDEVICE_TYPE_D3D11VA, NULL, NULL, 0));
    mHwCtx.reset(pHwCtx);
    AVD3D11VADeviceContext* dx11HwCtx = reinterpret_cast<AVD3D11VADeviceContext*>(
        reinterpret_cast<AVHWDeviceContext*>(mHwCtx->data)->hwctx);
    mDev = dx11HwCtx->device;

    mDev->GetImmediateContext(&mDevCtx);

    ComPtr<IDXGIDevice> giDev;
    THROW_IF_DX11(mDev->QueryInterface(IID_PPV_ARGS(&giDev)));
    ComPtr<IDXGIAdapter> giAdapter;
    THROW_IF_DX11(giDev->GetAdapter(&giAdapter));
    THROW_IF_DX11(giAdapter->GetParent(IID_PPV_ARGS(&mGiFactory)));

    HRESULT hr = S_OK;
    ComPtr<ID3DBlob> dxbc;
    ComPtr<ID3DBlob> err;

    constexpr char VS[] = R"(
struct VsIn {
    uint vtxIndex : SV_VertexID;
};

struct VsOut {
    float4 pos : SV_POSITION;
    float2 uv : UV;
};

VsOut main(VsIn vsIn) {
    float2 pos[] = {
        float2(-1,1),
        float2(1,1),
        float2(1,-1),
        float2(-1,1),
        float2(1,-1),
        float2(-1,-1)
    };
    float2 uv[] = {
        float2(0, 0),
        float2(1, 0),
        float2(1,1),
        float2(0, 0),
        float2(1,1),
        float2(0, 1),
    };
    VsOut vsOut = (VsOut)0;
    vsOut.pos = float4(pos[vsIn.vtxIndex], 0, 1);
    vsOut.uv = uv[vsIn.vtxIndex];
    return vsOut;
}
)";
    hr = D3DCompile(VS, sizeof(VS) - 1, "", nullptr, nullptr, "main", "vs_5_0", 0, 0, &dxbc, &err);
    if (hr != S_OK) {
        if (err)
            log::e() << "compile vs fail:\n" << (const char*)err->GetBufferPointer();
        THROW_IF(0, "compile vs fail");
    }
    THROW_IF_DX11(
        mDev->CreateVertexShader(dxbc->GetBufferPointer(), dxbc->GetBufferSize(), nullptr, &mVs));

    constexpr char PS[] = R"(
struct PsIn {
    float4 pos : SV_POSITION;
    float2 uv : UV;
};

struct PsOut {
    float4 color : SV_TARGET0;
};

Texture2D texY : register(t0);
Texture2D texU : register(t1);
Texture2D texV : register(t2);
SamplerState samplerState;

PsOut main(PsIn psIn) {
    PsOut psOut = (PsOut)0;
    float4 yuv;
    yuv.x = texY.Sample(samplerState, psIn.uv).x;
    yuv.y = texU.Sample(samplerState, psIn.uv).x;
    yuv.z = texV.Sample(samplerState, psIn.uv).x;
    yuv.w = 1;
    float4x4 m = {
         1.0,    1.0,    1.0,   0,
         0,     -0.344,  1.772, 0,
         1.402, -0.714,  0,     0,
        -0.701,  0.529, -0.886, 1
    };
    psOut.color = mul(yuv, m);
    return psOut;
}
)";
    hr = D3DCompile(PS, sizeof(PS) - 1, "", nullptr, nullptr, "main", "ps_5_0", 0, 0, &dxbc, &err);
    if (hr != S_OK) {
        if (err)
            log::e() << "compile ps fail:\n" << (const char*)err->GetBufferPointer();
        THROW_IF(0, "compile ps fail");
    }
    THROW_IF_DX11(
        mDev->CreatePixelShader(dxbc->GetBufferPointer(), dxbc->GetBufferSize(), nullptr, &mPs));

    //--

    constexpr char VS_NV12[] = R"(
struct VsIn {
    uint vtxIndex : SV_VertexID;
};

struct VsOut {
    float4 pos : SV_POSITION;
    float2 uv : UV;
};

VsOut main(VsIn vsIn) {
    float2 pos[] = {
        float2(-1,1),
        float2(1,1),
        float2(1,-1),
        float2(-1,1),
        float2(1,-1),
        float2(-1,-1)
    };
    float2 uv[] = {
        float2(0, 0),
        float2(1, 0),
        float2(1,1),
        float2(0, 0),
        float2(1,1),
        float2(0, 1),
    };
    VsOut vsOut = (VsOut)0;
    vsOut.pos = float4(pos[vsIn.vtxIndex], 0, 1);
    vsOut.uv = uv[vsIn.vtxIndex];
    return vsOut;
}
)";
    hr = D3DCompile(
        VS_NV12, sizeof(VS_NV12) - 1, "", nullptr, nullptr, "main", "vs_5_0", 0, 0, &dxbc, &err);
    if (hr != S_OK) {
        if (err)
            log::e() << "compile vs_nv12 fail:\n" << (const char*)err->GetBufferPointer();
        THROW_IF(0, "compile vs_nv12 fail");
    }
    THROW_IF_DX11(mDev->CreateVertexShader(
        dxbc->GetBufferPointer(), dxbc->GetBufferSize(), nullptr, &mVs_Nv12));

    constexpr char PS_NV12[] = R"(
struct PsIn {
    float4 pos : SV_POSITION;
    float2 uv : UV;
};

struct PsOut {
    float4 color : SV_TARGET0;
};

Texture2D texY : register(t0);
Texture2D texUv : register(t1);
SamplerState samplerState;

PsOut main(PsIn psIn) {
    PsOut psOut = (PsOut)0;
    float4 yuv;
    yuv.x = texY.Sample(samplerState, psIn.uv).x;
    yuv.y = texUv.Sample(samplerState, psIn.uv).x;
    yuv.z = texUv.Sample(samplerState, psIn.uv).y;
    yuv.w = 1;
    float4x4 m = {
         1.0,    1.0,    1.0,   0,
         0,     -0.344,  1.772, 0,
         1.402, -0.714,  0,     0,
        -0.701,  0.529, -0.886, 1
    };
    psOut.color = mul(yuv, m);
    return psOut;
}
)";
    hr = D3DCompile(
        PS_NV12, sizeof(PS_NV12) - 1, "", nullptr, nullptr, "main", "ps_5_0", 0, 0, &dxbc, &err);
    if (hr != S_OK) {
        if (err)
            log::e() << "compile ps_nv12 fail:\n" << (const char*)err->GetBufferPointer();
        THROW_IF(0, "compile ps_nv12 fail");
    }
    THROW_IF_DX11(mDev->CreatePixelShader(
        dxbc->GetBufferPointer(), dxbc->GetBufferSize(), nullptr, &mPs_Nv12));

    D3D11_SAMPLER_DESC samplerDesc = {};
    samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    samplerDesc.MinLOD = -100000;
    samplerDesc.MaxLOD = 100000;
    THROW_IF_DX11(mDev->CreateSamplerState(&samplerDesc, &mSampler));

    // D3D11_QUERY_DESC queryDesc = {};
    // queryDesc.Query = D3D11_QUERY_TIMESTAMP_DISJOINT;
    // mDev->CreateQuery(&queryDesc, &mQ0);
    // queryDesc.Query = D3D11_QUERY_TIMESTAMP;
    // mDev->CreateQuery(&queryDesc, &mQ1);
    // mDev->CreateQuery(&queryDesc, &mQ2);

    mDevCtx->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    ID3D11SamplerState* samplers[] = {mSampler.Get()};
    mDevCtx->PSSetSamplers(0, 1, samplers);
}

Dx11Render::~Dx11Render() {}

void Dx11Render::paint(PaintFrame* frame) {
    // not paint if minimum.
    if (mWinW == 0 || mWinH == 0)
        return;

    // mDevCtx->Begin(mQ0.Get());
    // mDevCtx->End(mQ1.Get());

    if (frame) {
        AVFrame* decodeF = frame->decodeFrame.get();
        if (decodeF->format == AV_PIX_FMT_YUV420P) {
            int w = decodeF->width;
            int h = decodeF->height;
            int hw = w / 2;
            int hh = h / 2;
            checkImgResize(w, h, hw, hh);

            uint8_t* src = decodeF->data[0];
            D3D11_MAPPED_SUBRESOURCE mapped = {};
            mDevCtx->Map(mImgY.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
            uint8_t* dst = (uint8_t*)mapped.pData;
            for (int i = 0; i < decodeF->height; ++i) {
                memcpy(dst, src, decodeF->width);
                src += decodeF->linesize[0];
                dst += mapped.RowPitch;
            }
            mDevCtx->Unmap(mImgY.Get(), 0);

            src = decodeF->data[1];
            mDevCtx->Map(mImgU.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
            dst = (uint8_t*)mapped.pData;
            for (int i = 0; i < hh; ++i) {
                memcpy(dst, src, hw);
                src += decodeF->linesize[1];
                dst += mapped.RowPitch;
            }
            mDevCtx->Unmap(mImgU.Get(), 0);

            src = decodeF->data[2];
            mDevCtx->Map(mImgV.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
            dst = (uint8_t*)mapped.pData;
            for (int i = 0; i < hh; ++i) {
                memcpy(dst, src, hw);
                src += decodeF->linesize[2];
                dst += mapped.RowPitch;
            }
            mDevCtx->Unmap(mImgV.Get(), 0);

            mLastFormat = AV_PIX_FMT_YUV420P;
        } else if (decodeF->format == AV_PIX_FMT_D3D11) {
            ID3D11Texture2D* decodeImg = (ID3D11Texture2D*)frame->decodeFrame->data[0];
            UINT decodeIndex = (UINT)(size_t)(frame->decodeFrame->data[1]);
            checkImgResize_Nv12(decodeImg);

            mDevCtx->CopySubresourceRegion(
                mImg_Nv12.Get(), 0, 0, 0, 0, decodeImg, decodeIndex, nullptr);

            mLastFormat = AV_PIX_FMT_D3D11;
        } else {
            const char* formatName = av_get_pix_fmt_name((AVPixelFormat)frame->decodeFrame->format);
            if (!formatName)
                formatName = "unknown";
            THROW_IF(0,
                     "format not supported, expect: AV_PIX_FMT_YUV420P or AV_PIX_FMT_D3D11, now: "
                         << formatName);
        }
    }

    switch (mLastFormat) {
        case AV_PIX_FMT_YUV420P: {
            mDevCtx->VSSetShader(mVs.Get(), nullptr, 0);
            mDevCtx->PSSetShader(mPs.Get(), nullptr, 0);
            ID3D11ShaderResourceView* srvs[] = {mTexY.Get(), mTexU.Get(), mTexV.Get()};
            mDevCtx->PSSetShaderResources(0, 3, srvs);
            mDevCtx->Draw(6, 0);
            break;
        }
        case AV_PIX_FMT_D3D11: {
            mDevCtx->VSSetShader(mVs_Nv12.Get(), nullptr, 0);
            mDevCtx->PSSetShader(mPs_Nv12.Get(), nullptr, 0);
            ID3D11ShaderResourceView* srvs[] = {mTex_Y_Nv12.Get(), mTex_Uv_Nv12.Get()};
            mDevCtx->PSSetShaderResources(0, 2, srvs);
            mDevCtx->Draw(6, 0);
            break;
        }
        case AV_PIX_FMT_NONE: {
            FLOAT clear[4] = {0.5f, 0.5f, 0.5f, 1.f};
            mDevCtx->ClearRenderTargetView(mRt.Get(), clear);
            break;
        }
        default:
            assert(0);
            break;
    }

    // mDevCtx->End(mQ2.Get());
    // mDevCtx->End(mQ0.Get());

    // D3D11_QUERY_DATA_TIMESTAMP_DISJOINT qd = {};
    // while (mDevCtx->GetData(mQ0.Get(), &qd, sizeof(qd), 0) != S_OK) {
    // }
    // uint64_t tpStart = 0;
    // while (mDevCtx->GetData(mQ1.Get(), &tpStart, sizeof(tpStart), 0) != S_OK) {
    // }
    // uint64_t tpEnd = 0;
    // while (mDevCtx->GetData(mQ2.Get(), &tpEnd, sizeof(tpEnd), 0) != S_OK) {
    // }
    // if (!qd.Disjoint) {
    //     log::i() << "timestamp: " << (static_cast<double>(tpEnd - tpStart) / qd.Frequency) *
    //     1000;
    // } else {
    //     log::i() << "timestamp: disjoint";
    // }

    mSwapChain->Present(0, 0);
}

void Dx11Render::resize(int w, int h) {
    log::i() << "win resize change, w: " << w << ", h: " << h;
    mWinW = w;
    mWinH = h;

    if (!mWinW || !mWinH)
        return;

    mRt = nullptr;
    mSwapChain = nullptr;
    DXGI_SWAP_CHAIN_DESC swapchainDesc = {};
    swapchainDesc.BufferCount = 2;
    swapchainDesc.BufferDesc.Width = mWinW;
    swapchainDesc.BufferDesc.Height = mWinH;
    swapchainDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapchainDesc.BufferDesc.RefreshRate.Numerator = 60;
    swapchainDesc.BufferDesc.RefreshRate.Denominator = 1;
    swapchainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapchainDesc.OutputWindow = FrontThread::GetSingleton()->getHwnd();
    swapchainDesc.SampleDesc.Count = 1;
    swapchainDesc.SampleDesc.Quality = 0;
    swapchainDesc.Windowed = TRUE;
    THROW_IF_DX11(mGiFactory->CreateSwapChain(mDev.Get(), &swapchainDesc, &mSwapChain));

    ComPtr<ID3D11Texture2D> backBuffer;
    mSwapChain->GetBuffer(0, IID_PPV_ARGS(&backBuffer));
    THROW_IF_DX11(mDev->CreateRenderTargetView(backBuffer.Get(), nullptr, &mRt));

    D3D11_VIEWPORT vp = {};
    vp.TopLeftX = 0.0f;
    vp.TopLeftY = 0.0f;
    vp.Width = (float)mWinW;
    vp.Height = (float)mWinH;
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    mDevCtx->RSSetViewports(1, &vp);

    ID3D11RenderTargetView* rts[] = {mRt.Get()};
    mDevCtx->OMSetRenderTargets(1, rts, nullptr);

    // force draw one frame.
    paint(nullptr);
}

void Dx11Render::checkImgResize(int w, int h, int hw, int hh) {
    if (mImgW == w && mImgH == h)
        return;

    D3D11_TEXTURE2D_DESC newImgDesc = {};
    newImgDesc.Width = w;
    newImgDesc.Height = h;
    newImgDesc.MipLevels = 1;
    newImgDesc.ArraySize = 1;
    newImgDesc.Format = DXGI_FORMAT_R8_UNORM;
    newImgDesc.SampleDesc = {1, 0};
    newImgDesc.Usage = D3D11_USAGE_DYNAMIC;
    newImgDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    newImgDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    newImgDesc.MiscFlags = 0;
    THROW_IF_DX11(mDev->CreateTexture2D(&newImgDesc, 0, &mImgY));

    newImgDesc.Width = hw;
    newImgDesc.Height = hh;
    THROW_IF_DX11(mDev->CreateTexture2D(&newImgDesc, 0, &mImgU));
    THROW_IF_DX11(mDev->CreateTexture2D(&newImgDesc, 0, &mImgV));

    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = DXGI_FORMAT_R8_UNORM;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D = {0, 1};
    THROW_IF_DX11(mDev->CreateShaderResourceView(mImgY.Get(), &srvDesc, &mTexY));
    THROW_IF_DX11(mDev->CreateShaderResourceView(mImgU.Get(), &srvDesc, &mTexU));
    THROW_IF_DX11(mDev->CreateShaderResourceView(mImgV.Get(), &srvDesc, &mTexV));

    mImgW = w;
    mImgH = h;
}

void Dx11Render::checkImgResize_Nv12(ID3D11Texture2D* decodeImg) {
    D3D11_TEXTURE2D_DESC decodeImgDesc = {};
    decodeImg->GetDesc(&decodeImgDesc);

    if (mImgW_Nv12 == decodeImgDesc.Width && mImgH_Nv12 == decodeImgDesc.Height)
        return;

    D3D11_TEXTURE2D_DESC newImgDesc = decodeImgDesc;
    newImgDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    newImgDesc.CPUAccessFlags = 0;
    newImgDesc.MiscFlags = 0;
    newImgDesc.ArraySize = 1;
    THROW_IF_DX11(mDev->CreateTexture2D(&newImgDesc, nullptr, &mImg_Nv12));

    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = DXGI_FORMAT_R8_UNORM;
    srvDesc.ViewDimension = D3D_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MostDetailedMip = 0;
    srvDesc.Texture2D.MipLevels = 1;
    THROW_IF_DX11(mDev->CreateShaderResourceView(mImg_Nv12.Get(), &srvDesc, &mTex_Y_Nv12));

    srvDesc.Format = DXGI_FORMAT_R8G8_UNORM;
    THROW_IF_DX11(mDev->CreateShaderResourceView(mImg_Nv12.Get(), &srvDesc, &mTex_Uv_Nv12));

    mImgW_Nv12 = decodeImgDesc.Width;
    mImgH_Nv12 = decodeImgDesc.Height;
}
}  // namespace share_screen
}  // namespace llc
