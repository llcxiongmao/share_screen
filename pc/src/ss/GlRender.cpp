#include <ss/GlRender.hpp>

#define GLAD_GL_IMPLEMENTATION
#include <glad/gl.h>

namespace ss {
GlRender::GlRender() {
    GLint result = GL_FALSE;

    static constexpr const char* VS = R"(
attribute vec2 iPos;
attribute vec2 iUv;
varying vec2 uv;
void main() {
    gl_Position = vec4(iPos, 1.0, 1.0);
    uv = iUv;
})";
    GLuint _vs = glCreateShader(GL_VERTEX_SHADER);
    SS_THROW(_vs, "create vs shader fail");
    mVs.emplace(_vs);
    glShaderSource(mVs->id(), 1, &VS, NULL);
    glCompileShader(mVs->id());
    glGetShaderiv(mVs->id(), GL_COMPILE_STATUS, &result);
    if (result == GL_FALSE) {
        GLint errStrLen = 0;
        glGetShaderiv(mVs->id(), GL_INFO_LOG_LENGTH, &errStrLen);
        if (errStrLen) {
            std::string errStr((size_t)errStrLen, 0);
            glGetShaderInfoLog(mVs->id(), errStrLen, &errStrLen, &errStr[0]);
            Log::E("compiler output:\n%s", errStr.c_str());
        }
        SS_THROW(0, "compile vs fail");
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
    yuv.w = 1.0;
    mat4 m = mat4(
        1.0,  0.0,    1.402, -0.701,
        1.0, -0.344, -0.714,  0.529,
        1.0,  1.772,  0.0,   -0.886,
        0.0,  0.0,    0.0,    1.0);
    gl_FragColor = yuv * m;
})";
    GLuint _fs = glCreateShader(GL_FRAGMENT_SHADER);
    SS_THROW(_fs, "create fs shader fail");
    mFs.emplace(_fs);
    glShaderSource(mFs->id(), 1, &FS, NULL);
    glCompileShader(mFs->id());
    glGetShaderiv(mFs->id(), GL_COMPILE_STATUS, &result);
    if (result == GL_FALSE) {
        GLint errStrLen = 0;
        glGetShaderiv(mFs->id(), GL_INFO_LOG_LENGTH, &errStrLen);
        if (errStrLen) {
            std::string errStr((size_t)errStrLen, 0);
            glGetShaderInfoLog(mFs->id(), errStrLen, &errStrLen, &errStr[0]);
            Log::E("compiler output:\n%s", errStr.c_str());
        }
        SS_THROW(0, "compile fs fail");
    }

    GLuint _program = glCreateProgram();
    SS_THROW(_program, "create gl program fail");
    mProgram.emplace(_program);
    glAttachShader(mProgram->id(), mVs->id());
    glAttachShader(mProgram->id(), mFs->id());
    glLinkProgram(mProgram->id());
    glGetProgramiv(mProgram->id(), GL_LINK_STATUS, &result);
    if (result == GL_FALSE) {
        GLint errStrLen = 0;
        glGetProgramiv(mProgram->id(), GL_INFO_LOG_LENGTH, &errStrLen);
        if (errStrLen) {
            std::string errStr((size_t)errStrLen, 0);
            glGetProgramInfoLog(mProgram->id(), errStrLen, &errStrLen, &errStr[0]);
            Log::E("linker output:\n%s", errStr.c_str());
        }
        SS_THROW(0, "link gl program fail");
    }
    glUseProgram(mProgram->id());

    GLint locY = glGetUniformLocation(mProgram->id(), "texY");
    GLint locU = glGetUniformLocation(mProgram->id(), "texU");
    GLint locV = glGetUniformLocation(mProgram->id(), "texV");
    glUniform1i(locY, 0);
    glUniform1i(locU, 1);
    glUniform1i(locV, 2);

    GLint locPos = glGetAttribLocation(mProgram->id(), "iPos");
    GLint locUv = glGetAttribLocation(mProgram->id(), "iUv");

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
    glVertexAttribPointer(locPos, 2, GL_FLOAT, 0, 0, poss);
    glEnableVertexAttribArray(locPos);

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
    glVertexAttribPointer(locUv, 2, GL_FLOAT, 0, 0, uvs);
    glEnableVertexAttribArray(locUv);
}

void GlRender::paint(PaintFrame* paintFrame) {
    if (paintFrame) {
        AVFrame* decodeF = paintFrame->decodeFrame;

        if (decodeF->format != AV_PIX_FMT_YUV420P) {
            const char* formatName =
                av_get_pix_fmt_name((AVPixelFormat)paintFrame->decodeFrame->format);
            if (!formatName)
                formatName = "unknown";
            SS_THROW(0, "format not supported, expect: AV_PIX_FMT_YUV420P, now: %s", formatName);
        }

        int w = decodeF->width;
        int h = decodeF->height;
        int hw = w / 2;
        int hh = h / 2;
        checkImgResize(w, h, hw, hh);

        // update y-image data.
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, mImgY->id());
        glPixelStorei(GL_UNPACK_ROW_LENGTH, (GLint)decodeF->linesize[0]);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, w, h, GL_RED, GL_UNSIGNED_BYTE, decodeF->data[0]);

        // update v-image data.
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, mImgU->id());
        glPixelStorei(GL_UNPACK_ROW_LENGTH, (GLint)decodeF->linesize[1]);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, hw, hh, GL_RED, GL_UNSIGNED_BYTE, decodeF->data[1]);

        // update v-image data.
        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, mImgV->id());
        glPixelStorei(GL_UNPACK_ROW_LENGTH, (GLint)decodeF->linesize[2]);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, hw, hh, GL_RED, GL_UNSIGNED_BYTE, decodeF->data[2]);
    }

    if (mImgY) {
        glDrawArrays(GL_TRIANGLES, 0, 6);
    } else {
        glClearColor(0.5f, 0.5f, 0.5f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
    }
}

void GlRender::checkImgResize(int w, int h, int hw, int hh) {
    if (mImgW == w && mImgH == h) {
        return;
    }

    Log::I("frame image size changed: (%d, %d) -> (%d, %d)", mImgW, mImgH, w, h);

    mImgY = std::nullopt;
    mImgU = std::nullopt;
    mImgV = std::nullopt;

    GLuint _imgY = 0;
    glGenTextures(1, &_imgY);
    SS_THROW(_imgY, "create gl img-y fail");
    mImgY.emplace(_imgY);
    GLuint _imgU = 0;
    glGenTextures(1, &_imgU);
    SS_THROW(_imgU, "create gl img-u fail");
    mImgU.emplace(_imgU);
    GLuint _imgV = 0;
    glGenTextures(1, &_imgV);
    SS_THROW(_imgV, "create gl img-v fail");
    mImgV.emplace(_imgV);

    glBindTexture(GL_TEXTURE_2D, mImgY->id());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, w, h, 0, GL_RED, GL_UNSIGNED_BYTE, nullptr);

    glBindTexture(GL_TEXTURE_2D, mImgU->id());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, hw, hh, 0, GL_RED, GL_UNSIGNED_BYTE, nullptr);

    glBindTexture(GL_TEXTURE_2D, mImgV->id());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, hw, hh, 0, GL_RED, GL_UNSIGNED_BYTE, nullptr);

    mImgW = w;
    mImgH = h;
}
}  // namespace ss
