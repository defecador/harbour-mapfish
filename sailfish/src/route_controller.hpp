#pragma once

#include <QtCore/QObject>
#include <QtCore/QString>

class Framework;

namespace sailfish
{
// Walking directions from the device's position to the current place-page selection. Exposed to
// QML as "route". There is one route at a time, so there is one instance.
class RouteController : public QObject
{
  Q_OBJECT
  Q_PROPERTY(bool active READ Active NOTIFY changed)
  Q_PROPERTY(bool building READ Building NOTIFY changed)
  Q_PROPERTY(bool hasRoute READ HasRoute NOTIFY changed)
  Q_PROPERTY(QString distance READ Distance NOTIFY changed)
  Q_PROPERTY(QString time READ Time NOTIFY changed)
  Q_PROPERTY(QString error READ Error NOTIFY changed)
  // 0 = walking, 1 = cycling. Matches routing::RouterType's ordering.
  Q_PROPERTY(int mode READ Mode WRITE SetMode NOTIFY changed)

public:
  explicit RouteController(Framework & framework, QObject * parent = nullptr);

  bool Active() const { return m_active; }
  bool Building() const { return m_building; }
  // True once there is a start and a destination, so a tapped place can be added as a stop.
  bool HasRoute() const { return m_hasRoute; }
  QString Distance() const { return m_distance; }
  QString Time() const { return m_time; }
  QString Error() const { return m_error; }
  int Mode() const { return m_mode; }
  void SetMode(int mode);

  // Starts a new route from the device's position to the place-page selection, discarding any
  // route in progress. Does nothing if there is no selection; the framework itself reports
  // NoCurrentPosition if GPS has no fix.
  Q_INVOKABLE void routeToSelection();
  // Inserts the place-page selection as a stop on the current route, in whichever position along
  // it fits best. Does nothing without an existing route.
  Q_INVOKABLE void addStopAtSelection();
  Q_INVOKABLE void clear();

signals:
  void changed();

private:
  Framework & m_framework;
  bool m_active = false;
  bool m_building = false;
  bool m_hasRoute = false;
  int m_mode = 0;
  QString m_distance;
  QString m_time;
  QString m_error;
};
}  // namespace sailfish
