#pragma once
#include <ss/Common.hpp>

namespace ss {
/** opengl impl for Render, support AV_PIX_FMT_YUV420P. */
class GlRender : public xm::NonCopyable {
public:
    GlRender();

    ~GlRender() {}

    void paint(PaintFrame* paintFrame);

private:
    void checkImgResize(int w, int h, int hw, int hh);

    class RaiiImage : public xm::NonCopyable {
    public:
        RaiiImage(GLuint id) : mId(id) {}

        ~RaiiImage() {
            glDeleteTextures(1, &mId);
        }

        GLuint id() const {
            return mId;
        }

    private:
        GLuint mId;
    };

    class RaiiShader : public xm::NonCopyable {
    public:
        RaiiShader(GLuint id) : mId(id) {}

        ~RaiiShader() {
            glDeleteShader(mId);
        }

        GLuint id() const {
            return mId;
        }

    private:
        GLuint mId;
    };

    class RaiiProgram : public xm::NonCopyable {
    public:
        RaiiProgram(GLuint id) : mId(id) {}

        ~RaiiProgram() {
            glDeleteProgram(mId);
        }

        GLuint id() const {
            return mId;
        }

    private:
        GLuint mId;
    };

    // ----

    /** current image width. */
    int mImgW = 0;
    /** current image height. */
    int mImgH = 0;

    std::optional<RaiiImage> mImgY;
    std::optional<RaiiImage> mImgU;
    std::optional<RaiiImage> mImgV;
    std::optional<RaiiShader> mVs;
    std::optional<RaiiShader> mFs;
    std::optional<RaiiProgram> mProgram;
};
}  // namespace ss
