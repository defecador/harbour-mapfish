#include "sailfish/src/egl_context_factory.hpp"

#include "base/assert.hpp"
#include "base/logging.hpp"

#include <EGL/eglext.h>

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <utility>
#include <vector>

#ifndef EGL_OPENGL_ES3_BIT_KHR
#define EGL_OPENGL_ES3_BIT_KHR 0x00000040
#endif

namespace sailfish
{
namespace
{
EGLint const kContextAttribs[] = {EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE};

EGLint const kPbufferAttribs[] = {EGL_WIDTH, 1, EGL_HEIGHT, 1, EGL_NONE};

EGLConfig ChooseConfig(EGLDisplay display)
{
  EGLint const attribs[] = {EGL_RED_SIZE,        8,
                            EGL_GREEN_SIZE,      8,
                            EGL_BLUE_SIZE,       8,
                            EGL_ALPHA_SIZE,      8,
                            EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT_KHR,
                            EGL_SURFACE_TYPE,    EGL_PBUFFER_BIT,
                            EGL_NONE};
  EGLConfig config = nullptr;
  EGLint count = 0;
  if (eglChooseConfig(display, attribs, &config, 1, &count) != EGL_TRUE || count == 0)
  {
    LOG(LERROR, ("No EGL config for an OpenGL ES 3 pbuffer, error", eglGetError()));
    return nullptr;
  }
  return config;
}

// Drape's contexts only ever draw into framebuffer objects, but EGL still wants a surface to make
// a context current. A 1x1 pbuffer avoids depending on EGL_KHR_surfaceless_context.
std::pair<EGLSurface, EGLContext> CreateContext(EGLDisplay display, EGLConfig config, EGLContext shareContext)
{
  EGLSurface const surface = eglCreatePbufferSurface(display, config, kPbufferAttribs);
  CHECK(surface != EGL_NO_SURFACE, ("eglCreatePbufferSurface failed, error", eglGetError()));
  EGLContext const context = eglCreateContext(display, config, shareContext, kContextAttribs);
  CHECK(context != EGL_NO_CONTEXT, ("eglCreateContext failed, error", eglGetError()));
  return {surface, context};
}

void DestroyContext(EGLDisplay display, EGLSurface surface, EGLContext context)
{
  if (eglGetCurrentContext() == context)
    eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
  if (context != EGL_NO_CONTEXT)
    eglDestroyContext(display, context);
  if (surface != EGL_NO_SURFACE)
    eglDestroySurface(display, surface);
}
}  // namespace

DrawContext::DrawContext(EGLDisplay display, EGLConfig config, EGLContext shareContext, int width, int height,
                         std::function<void()> const & onFrameReady)
  : m_display(display)
  , m_width(width)
  , m_height(height)
  , m_onFrameReady(onFrameReady)
{
  std::tie(m_surface, m_context) = CreateContext(display, config, shareContext);
  // Debugging aid: shows what drape draws even while the display is off.
  if (char const * dumpPath = std::getenv("COMAPS_DUMP_FRAME"))
    m_dumpPath = dumpPath;
}

DrawContext::~DrawContext()
{
  // The framebuffers and textures go away with the share group; there may be no context current
  // here to delete them with.
  DestroyContext(m_display, m_surface, m_context);
}

void DrawContext::MakeCurrent()
{
  CHECK(eglMakeCurrent(m_display, m_surface, m_surface, m_context) == EGL_TRUE,
        ("eglMakeCurrent failed, error", eglGetError()));
}

void DrawContext::DoneCurrent()
{
  eglMakeCurrent(m_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
}

void DrawContext::SetRenderingEnabled(bool enabled)
{
  if (enabled)
    MakeCurrent();
  else
    DoneCurrent();
}

bool DrawContext::Validate()
{
  return eglGetCurrentContext() == m_context;
}

void DrawContext::SetFramebuffer(ref_ptr<dp::BaseFramebuffer> framebuffer)
{
  if (framebuffer)
  {
    framebuffer->Bind();
    return;
  }
  EnsureBackTarget();
  glBindFramebuffer(GL_FRAMEBUFFER, m_back.m_fbo);
}

void DrawContext::Resize(uint32_t w, uint32_t h)
{
  m_width = static_cast<int>(w);
  m_height = static_cast<int>(h);
  EnsureBackTarget();
}

void DrawContext::DumpFrame()
{
  auto const now = std::chrono::steady_clock::now();
  if (now - m_lastDump < std::chrono::seconds(1))
    return;
  m_lastDump = now;

  int const w = m_back.m_width;
  int const h = m_back.m_height;
  std::vector<uint8_t> rgba(static_cast<size_t>(w) * h * 4);
  glBindFramebuffer(GL_FRAMEBUFFER, m_back.m_fbo);
  glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());

  std::string const tmp = m_dumpPath + ".tmp";
  std::ofstream out(tmp, std::ios::binary);
  out << "P6\n" << w << " " << h << "\n255\n";
  for (int y = h - 1; y >= 0; --y)
    for (int x = 0; x < w; ++x)
      out.write(reinterpret_cast<char const *>(&rgba[(static_cast<size_t>(y) * w + x) * 4]), 3);
  out.close();
  std::rename(tmp.c_str(), m_dumpPath.c_str());
}

void DrawContext::Present()
{
  if (!m_dumpPath.empty())
    DumpFrame();

  // The scene graph samples this frame from another context: it must be complete before we
  // publish it.
  glFinish();
  {
    std::lock_guard lock(m_frameMutex);
    std::swap(m_back, m_ready);
    m_readyIsNew = true;
  }
  m_onFrameReady();
}

Frame DrawContext::AcquireFrame(bool & isNew)
{
  std::lock_guard lock(m_frameMutex);
  isNew = m_readyIsNew;
  if (m_readyIsNew)
  {
    std::swap(m_ready, m_front);
    m_readyIsNew = false;
  }
  return {m_front.m_color, m_front.m_width, m_front.m_height};
}

void DrawContext::EnsureBackTarget()
{
  // "back" is never the displayed frame, so it can be reallocated here without the lock: after a
  // resize, the older frames still carry their old size and are drawn at that size until replaced.
  if (m_back.m_fbo != 0 && m_back.m_width == m_width && m_back.m_height == m_height)
    return;
  if (m_width <= 0 || m_height <= 0)
    return;
  Release(m_back);
  Allocate(m_back, m_width, m_height);
}

void DrawContext::Allocate(Target & target, int width, int height)
{
  target.m_width = width;
  target.m_height = height;

  glGenTextures(1, &target.m_color);
  glBindTexture(GL_TEXTURE_2D, target.m_color);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glBindTexture(GL_TEXTURE_2D, 0);

  glGenRenderbuffers(1, &target.m_depth);
  glBindRenderbuffer(GL_RENDERBUFFER, target.m_depth);
  glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);
  glBindRenderbuffer(GL_RENDERBUFFER, 0);

  glGenFramebuffers(1, &target.m_fbo);
  glBindFramebuffer(GL_FRAMEBUFFER, target.m_fbo);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, target.m_color, 0);
  glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, target.m_depth);
  GLenum const status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
  CHECK_EQUAL(status, GL_FRAMEBUFFER_COMPLETE, ("Incomplete map framebuffer", width, height));
}

void DrawContext::Release(Target & target)
{
  if (target.m_fbo != 0)
    glDeleteFramebuffers(1, &target.m_fbo);
  if (target.m_depth != 0)
    glDeleteRenderbuffers(1, &target.m_depth);
  if (target.m_color != 0)
    glDeleteTextures(1, &target.m_color);
  target = {};
}

UploadContext::UploadContext(EGLDisplay display, EGLConfig config, EGLContext shareContext) : m_display(display)
{
  std::tie(m_surface, m_context) = CreateContext(display, config, shareContext);
}

UploadContext::~UploadContext()
{
  DestroyContext(m_display, m_surface, m_context);
}

void UploadContext::MakeCurrent()
{
  CHECK(eglMakeCurrent(m_display, m_surface, m_surface, m_context) == EGL_TRUE,
        ("eglMakeCurrent failed, error", eglGetError()));
}

void UploadContext::DoneCurrent()
{
  eglMakeCurrent(m_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
}

void UploadContext::Present()
{
  CHECK(false, ("The upload context never presents"));
}

void UploadContext::SetFramebuffer(ref_ptr<dp::BaseFramebuffer>)
{
  CHECK(false, ("The upload context never draws"));
}

EglContextFactory::EglContextFactory(int width, int height, std::function<void()> const & onFrameReady)
  : m_width(width)
  , m_height(height)
  , m_onFrameReady(onFrameReady)
{
  m_display = eglGetCurrentDisplay();
  EGLContext const qtContext = eglGetCurrentContext();
  if (m_display == EGL_NO_DISPLAY || qtContext == EGL_NO_CONTEXT)
  {
    LOG(LERROR, ("The scene graph's EGL context is not current"));
    return;
  }

  m_config = ChooseConfig(m_display);
  if (m_config == nullptr)
    return;

  EGLint qtClientVersion = 0;
  eglQueryContext(m_display, qtContext, EGL_CONTEXT_CLIENT_VERSION, &qtClientVersion);
  LOG(LINFO, ("EGL", eglQueryString(m_display, EGL_VENDOR), eglQueryString(m_display, EGL_VERSION),
              "- Qt Quick context is OpenGL ES", qtClientVersion));

  m_shareContext = qtContext;

  // Created here rather than on drape's threads: the driver only accepts a share context from
  // the thread it is current on. Drape makes them current on its own threads later.
  m_drawContext = std::make_unique<DrawContext>(m_display, m_config, m_shareContext, m_width, m_height,
                                                m_onFrameReady);
  m_uploadContext = std::make_unique<UploadContext>(m_display, m_config, m_shareContext);
}

dp::GraphicsContext * EglContextFactory::GetDrawContext()
{
  m_publishedDrawContext = m_drawContext.get();
  return m_drawContext.get();
}

dp::GraphicsContext * EglContextFactory::GetResourcesUploadContext()
{
  return m_uploadContext.get();
}

Frame EglContextFactory::AcquireFrame(bool & isNew)
{
  DrawContext * drawContext = m_publishedDrawContext;
  if (drawContext == nullptr)
  {
    isNew = false;
    return {};
  }
  return drawContext->AcquireFrame(isNew);
}
}  // namespace sailfish
