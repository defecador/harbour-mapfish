#pragma once

// Qt 5.6's moc can't parse the C++23 in CoMaps' headers: keep them out of this one.
#include <QtQuick/QQuickItem>

#include <memory>

class Framework;

namespace dp
{
class ThreadSafeFactory;
}  // namespace dp

namespace sailfish
{
class EglContextFactory;

class MapItem : public QQuickItem
{
  Q_OBJECT

public:
  explicit MapItem(QQuickItem * parent = nullptr);
  ~MapItem() override;

  // The QML engine constructs map items, so the framework is handed over before QML loads.
  static void SetFramework(Framework * framework);

protected:
  QSGNode * updatePaintNode(QSGNode * oldNode, UpdatePaintNodeData *) override;
  void geometryChanged(QRectF const & newGeometry, QRectF const & oldGeometry) override;
  void touchEvent(QTouchEvent * event) override;

private slots:
  void CreateEngine();
  void OnApplicationStateChanged(Qt::ApplicationState state);

private:
  QSize PixelSize() const;

  Framework & m_framework;
  // Owned here, but built on the render thread: it must share Qt Quick's EGL context.
  std::unique_ptr<dp::ThreadSafeFactory> m_contextFactory;
  EglContextFactory * m_eglFactory = nullptr;
  bool m_engineCreated = false;
};
}  // namespace sailfish
