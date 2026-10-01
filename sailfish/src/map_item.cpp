#include "sailfish/src/map_item.hpp"

#include "sailfish/src/egl_context_factory.hpp"

#include "map/framework.hpp"

#include "drape_frontend/gui/skin.hpp"
#include "drape_frontend/user_event_stream.hpp"

#include "drape/graphics_context_factory.hpp"
#include "drape/pointers.hpp"
#include "drape/visual_scale.hpp"

#include "base/assert.hpp"
#include "base/logging.hpp"

#include <QtGui/QGuiApplication>
#include <QtGui/QScreen>
#include <QtGui/QTouchEvent>
#include <QtQuick/QQuickWindow>
#include <QtQuick/QSGSimpleTextureNode>

namespace sailfish
{
namespace
{
Framework * g_mapFramework = nullptr;

uint8_t constexpr kInvalidPointer = df::TouchEvent::INVALID_MASKED_POINTER;
}  // namespace

void MapItem::SetFramework(Framework * framework)
{
  g_mapFramework = framework;
}

MapItem::MapItem(QQuickItem * parent) : QQuickItem(parent), m_framework(*g_mapFramework)
{
  setFlag(ItemHasContents, true);
  setAcceptedMouseButtons(Qt::LeftButton);
  connect(qGuiApp, &QGuiApplication::applicationStateChanged, this, &MapItem::OnApplicationStateChanged);
}

MapItem::~MapItem()
{
  // Stops drape's threads, which still use the contexts the factory owns.
  if (m_engineCreated)
    m_framework.DestroyDrapeEngine();
  m_contextFactory.reset();
}

QSize MapItem::PixelSize() const
{
  qreal const ratio = window() != nullptr ? window()->devicePixelRatio() : 1.0;
  return QSize(qRound(width() * ratio), qRound(height() * ratio));
}

QSGNode * MapItem::updatePaintNode(QSGNode * oldNode, UpdatePaintNodeData *)
{
  // Runs on the render thread while the GUI thread is blocked, with Qt Quick's context current.

  // Qt Quick can tear down and rebuild its own GL context around the item's lifetime, e.g. when
  // the window is hidden (app minimized) and shown again. When that happens, m_eglFactory's
  // contexts are still perfectly valid, but they were shared with a context that no longer
  // exists, so their textures aren't visible to Qt's new one and the map goes blank. Detect the
  // mismatch and rebuild everything against the context that is actually current now.
  if (m_contextFactory && m_eglFactory->SharedWithContext() != eglGetCurrentContext())
  {
    if (!m_recreatePending.exchange(true))
      QMetaObject::invokeMethod(this, "RecreateEngine", Qt::QueuedConnection);
    return oldNode;
  }

  if (!m_contextFactory)
  {
    QSize const size = PixelSize();
    if (size.isEmpty())
      return oldNode;

    auto eglFactory = std::make_unique<EglContextFactory>(size.width(), size.height(), [this]()
    { QMetaObject::invokeMethod(this, "update", Qt::QueuedConnection); });
    if (!eglFactory->IsValid())
      return oldNode;

    m_eglFactory = eglFactory.get();
    m_contextFactory = std::make_unique<dp::ThreadSafeFactory>(eglFactory.release());
    QMetaObject::invokeMethod(this, "CreateEngine", Qt::QueuedConnection);
    return oldNode;
  }

  bool isNew = false;
  Frame const frame = m_eglFactory->AcquireFrame(isNew);
  if (frame.m_texture == 0)
    return oldNode;

  auto * node = static_cast<QSGSimpleTextureNode *>(oldNode);
  if (node == nullptr)
  {
    node = new QSGSimpleTextureNode();
    node->setOwnsTexture(true);
    // GL framebuffers are bottom-up, the scene graph is top-down.
    node->setTextureCoordinatesTransform(QSGSimpleTextureNode::MirrorVertically);
    isNew = true;
  }

  if (isNew)
  {
    // Wraps drape's texture without taking ownership of it.
    node->setTexture(window()->createTextureFromId(frame.m_texture, QSize(frame.m_width, frame.m_height)));
  }
  node->setRect(boundingRect());
  return node;
}

void MapItem::CreateEngine()
{
  QSize const size = PixelSize();
  double const dpi = window()->screen()->physicalDotsPerInch();

  Framework::DrapeCreationParams p;
  p.m_apiVersion = dp::ApiVersion::OpenGLES3;
  p.m_surfaceWidth = size.width();
  p.m_surfaceHeight = size.height();
  p.m_visualScale = static_cast<float>(dp::VisualScale(dpi));

  gui::Skin skin(gui::ResolveGuiSkinFile("default"), p.m_visualScale);
  skin.Resize(p.m_surfaceWidth, p.m_surfaceHeight);
  // The compass defaults to the top right corner, where the app's own buttons sit: push it down.
  float const compassDrop = static_cast<float>(2.0 / 2.54 * dpi);  // 2 cm in pixels
  skin.ForEach([&p, compassDrop](gui::EWidget widget, gui::Position const & pos)
  {
    p.m_widgetsInitInfo[widget] = pos;
    if (widget == gui::WIDGET_COMPASS)
      p.m_widgetsInitInfo[widget].m_pixelPivot.y += compassDrop;
  });

  LOG(LINFO, ("Creating the drape engine:", size.width(), "x", size.height(), "px,", dpi, "dpi, visual scale",
              p.m_visualScale));

  m_framework.CreateDrapeEngine(make_ref(m_contextFactory), std::move(p));
  m_framework.EnterForeground();
  m_engineCreated = true;
}

void MapItem::RecreateEngine()
{
  // GUI thread. Tear down the engine first: it owns drape's threads, and they are the only other
  // thing touching m_contextFactory's contexts. Only once they've stopped is it safe to free those
  // contexts (below, via reset()) and let updatePaintNode rebuild everything from scratch, the same
  // way it does on first use.
  LOG(LINFO, ("Qt Quick's GL context changed under the map; recreating the drape engine"));
  if (m_engineCreated)
  {
    m_framework.DestroyDrapeEngine();
    m_engineCreated = false;
  }
  m_contextFactory.reset();
  m_eglFactory = nullptr;
  m_recreatePending = false;
  update();
}

void MapItem::geometryChanged(QRectF const & newGeometry, QRectF const & oldGeometry)
{
  QQuickItem::geometryChanged(newGeometry, oldGeometry);
  if (newGeometry.size() == oldGeometry.size())
    return;
  if (m_engineCreated)
  {
    QSize const size = PixelSize();
    m_framework.OnSize(size.width(), size.height());
  }
  update();
}

void MapItem::OnApplicationStateChanged(Qt::ApplicationState state)
{
  if (!m_engineCreated)
    return;
  if (state == Qt::ApplicationActive)
    m_framework.EnterForeground();
  else
    m_framework.EnterBackground();
}

void MapItem::touchEvent(QTouchEvent * event)
{
  if (!m_engineCreated)
  {
    event->ignore();
    return;
  }

  auto const & points = event->touchPoints();
  int const count = std::min(points.size(), 2);
  if (count == 0)
    return;

  // Mirrors what the Android app sends: which finger went down or up, by index; nothing for moves.
  df::TouchEvent touch;
  uint8_t pointer = kInvalidPointer;
  if (event->type() == QEvent::TouchCancel)
  {
    touch.SetTouchType(df::TouchEvent::TOUCH_CANCEL);
  }
  else
  {
    touch.SetTouchType(df::TouchEvent::TOUCH_MOVE);
    for (int i = 0; i < count; ++i)
    {
      if (points[i].state() == Qt::TouchPointReleased)
      {
        touch.SetTouchType(df::TouchEvent::TOUCH_UP);
        pointer = static_cast<uint8_t>(i);
        break;
      }
      if (points[i].state() == Qt::TouchPointPressed)
      {
        touch.SetTouchType(df::TouchEvent::TOUCH_DOWN);
        pointer = static_cast<uint8_t>(i);
        break;
      }
    }
  }

  qreal const ratio = window()->devicePixelRatio();
  auto const toDf = [ratio](QTouchEvent::TouchPoint const & point)
  {
    df::Touch t;
    t.m_id = point.id();
    t.m_location = m2::PointF(static_cast<float>(point.pos().x() * ratio), static_cast<float>(point.pos().y() * ratio));
    return t;
  };
  touch.SetFirstTouch(toDf(points[0]));
  if (count == 2)
    touch.SetSecondTouch(toDf(points[1]));

  touch.SetFirstMaskedPointer(count == 1 ? 0 : pointer);
  m_framework.TouchEvent(touch);
  event->accept();
}
}  // namespace sailfish
