#pragma once

#include <QtCore/QObject>

class Framework;

namespace sailfish
{
// Map layers the user can switch on and off. Exposed to QML as "layers".
class MapLayers : public QObject
{
  Q_OBJECT
  Q_PROPERTY(bool transit READ Transit WRITE SetTransit NOTIFY transitChanged)

public:
  explicit MapLayers(Framework & framework, QObject * parent = nullptr);

  // The metro/S-Bahn scheme, drawn over the public transport map style.
  bool Transit() const;
  void SetTransit(bool enabled);

signals:
  void transitChanged();

private:
  Framework & m_framework;
};
}  // namespace sailfish
