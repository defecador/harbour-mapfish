#pragma once

#include <QtCore/QObject>

#include <memory>

class Framework;

namespace sailfish
{
// Bridges to location::LocationObserver/LocationService (location_controller.cpp). That header
// pulls in 3party/ankerl/unordered_dense.h, whose C++23 moc on Qt 5.6 (Sailfish) can't parse, so
// it must stay out of this file entirely; LocationControllerImpl is the PIMPL that keeps it out.
class LocationControllerImpl;

// Feeds GPS updates to the framework and exposes the "my position" button's state to QML. There
// is one device position, so there is one instance.
class LocationController : public QObject
{
  Q_OBJECT
  Q_PROPERTY(int mode READ Mode NOTIFY modeChanged)

public:
  explicit LocationController(Framework & framework, QObject * parent = nullptr);
  ~LocationController() override;

  // Matches location::EMyPositionMode: 0 pending, 1 no position, 2 not following, 3 following,
  // 4 following and rotating. QML reads this as a plain int to pick the button's icon.
  int Mode() const { return m_mode; }

  // Cycles the framework to its next my-position mode (off -> follow -> follow+rotate -> off).
  Q_INVOKABLE void locate();

signals:
  void modeChanged();

private:
  Framework & m_framework;
  std::unique_ptr<LocationControllerImpl> m_impl;
  int m_mode = 1;
};
}  // namespace sailfish
