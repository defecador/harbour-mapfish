#pragma once

#include <QtCore/QObject>
#include <QtCore/QString>

namespace sailfish
{
// Where trips start from: the device's position unless the user picked another place. Shared by the
// walk/bike and transit routes, which rebuild when it changes. Exposed to QML as "origin".
class TripOrigin : public QObject
{
  Q_OBJECT
  // Name of the chosen place; empty while the origin is the device's position.
  Q_PROPERTY(QString name READ Name NOTIFY changed)
  Q_PROPERTY(bool custom READ Custom NOTIFY changed)

public:
  explicit TripOrigin(QObject * parent = nullptr) : QObject(parent) {}

  QString Name() const { return m_name; }
  bool Custom() const { return m_custom; }
  // Mercator coordinates, meaningful only while Custom().
  double X() const { return m_x; }
  double Y() const { return m_y; }

  void Set(QString const & name, double x, double y);
  // Back to the device's position.
  Q_INVOKABLE void reset();

signals:
  void changed();

private:
  QString m_name;
  bool m_custom = false;
  double m_x = 0;
  double m_y = 0;
};
}  // namespace sailfish
