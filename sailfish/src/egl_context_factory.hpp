#pragma once

#include "drape/graphics_context_factory.hpp"
#include "drape/oglcontext.hpp"

#include <EGL/egl.h>
#include <GLES3/gl3.h>

#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <string>

namespace sailfish
{
// A finished frame, ready to be sampled by the Qt Quick scene graph.
struct Frame
{
  GLuint m_texture = 0;
  int m_width = 0;
  int m_height = 0;
};

// Drape's render thread draws into offscreen framebuffers. The scene graph shows them as a
// texture, so there are three: drape draws into "back", Present() publishes it as "ready", and
// the scene graph takes "ready" as "front". Drape never touches the frame being displayed.
class DrawContext : public dp::OGLContext
{
public:
  DrawContext(EGLDisplay display, EGLConfig config, EGLContext shareContext, int width, int height,
              std::function<void()> const & onFrameReady);
  ~DrawContext() override;

  void Present() override;
  void MakeCurrent() override;
  void DoneCurrent() override;
  void SetFramebuffer(ref_ptr<dp::BaseFramebuffer> framebuffer) override;
  void Resize(uint32_t w, uint32_t h) override;
  void SetRenderingEnabled(bool enabled) override;
  bool Validate() override;

  // Called on the scene graph's render thread.
  Frame AcquireFrame(bool & isNew);

private:
  struct Target
  {
    GLuint m_fbo = 0;
    GLuint m_color = 0;
    GLuint m_depth = 0;
    int m_width = 0;
    int m_height = 0;
  };

  void EnsureBackTarget();
  void DumpFrame();
  static void Allocate(Target & target, int width, int height);
  static void Release(Target & target);

  EGLDisplay m_display;
  EGLSurface m_surface = EGL_NO_SURFACE;
  EGLContext m_context = EGL_NO_CONTEXT;

  int m_width;
  int m_height;
  Target m_back;
  Target m_ready;
  Target m_front;
  bool m_readyIsNew = false;
  std::mutex m_frameMutex;
  std::function<void()> m_onFrameReady;

  std::string m_dumpPath;
  std::chrono::steady_clock::time_point m_lastDump;
};

class UploadContext : public dp::OGLContext
{
public:
  UploadContext(EGLDisplay display, EGLConfig config, EGLContext shareContext);
  ~UploadContext() override;

  void Present() override;
  void MakeCurrent() override;
  void DoneCurrent() override;
  void SetFramebuffer(ref_ptr<dp::BaseFramebuffer> framebuffer) override;

private:
  EGLDisplay m_display;
  EGLSurface m_surface = EGL_NO_SURFACE;
  EGLContext m_context = EGL_NO_CONTEXT;
};

// Creates drape's contexts in the same share group as Qt Quick's own, so the frames drape renders
// can be shown by the scene graph without copying.
class EglContextFactory : public dp::GraphicsContextFactory
{
public:
  // Must be constructed on the scene graph's render thread, with its context current.
  EglContextFactory(int width, int height, std::function<void()> const & onFrameReady);

  bool IsValid() const { return m_shareContext != EGL_NO_CONTEXT; }
  // The Qt Quick context this factory's contexts were shared with, to detect Qt recreating its
  // own context later (e.g. after the window is hidden and shown again) and orphaning ours.
  EGLContext SharedWithContext() const { return m_shareContext; }

  dp::GraphicsContext * GetDrawContext() override;
  dp::GraphicsContext * GetResourcesUploadContext() override;
  bool IsDrawContextCreated() const override { return m_drawContext != nullptr; }
  bool IsUploadContextCreated() const override { return m_uploadContext != nullptr; }

  Frame AcquireFrame(bool & isNew);

private:
  EGLDisplay m_display = EGL_NO_DISPLAY;
  EGLContext m_shareContext = EGL_NO_CONTEXT;
  EGLConfig m_config = nullptr;
  int m_width;
  int m_height;
  std::function<void()> m_onFrameReady;
  std::unique_ptr<DrawContext> m_drawContext;
  // Created on drape's render thread, read on the scene graph's.
  std::atomic<DrawContext *> m_publishedDrawContext = nullptr;
  std::unique_ptr<UploadContext> m_uploadContext;
};
}  // namespace sailfish
