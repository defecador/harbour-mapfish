#include "sailfish/src/trip_origin.hpp"

namespace sailfish
{
void TripOrigin::Set(QString const & name, double x, double y)
{
  m_name = name;
  m_custom = true;
  m_x = x;
  m_y = y;
  emit changed();
}

void TripOrigin::reset()
{
  if (!m_custom)
    return;
  m_name.clear();
  m_custom = false;
  emit changed();
}
}  // namespace sailfish
