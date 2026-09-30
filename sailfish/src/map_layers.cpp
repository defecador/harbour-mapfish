#include "sailfish/src/map_layers.hpp"

#include "map/framework.hpp"

namespace sailfish
{
MapLayers::MapLayers(Framework & framework, QObject * parent) : QObject(parent), m_framework(framework) {}

bool MapLayers::Transit() const
{
  return m_framework.CurrentMapMode() == MapMode::PublicTransport;
}

void MapLayers::SetTransit(bool enabled)
{
  if (enabled == Transit())
    return;
  // The transit scheme is part of the public transport map mode, as on Android: that mode also
  // switches to a style that fades the roads so the lines stand out. It remembers the choice.
  m_framework.PublicTransportMapModeSetTransitLines(true);
  m_framework.SwitchToMapMode(enabled ? MapMode::PublicTransport : MapMode::Default);
  emit transitChanged();
}
}  // namespace sailfish
