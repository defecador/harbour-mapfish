#include "sailfish/src/place_page.hpp"

#include "map/framework.hpp"
#include "map/place_page_info.hpp"

#include "indexer/feature_meta.hpp"

namespace sailfish
{
namespace
{
QString ToQString(std::string_view s)
{
  return QString::fromUtf8(s.data(), static_cast<int>(s.size()));
}
}  // namespace

PlacePage::PlacePage(Framework & framework, QObject * parent) : QObject(parent), m_framework(framework)
{
  // The framework may call these from its own threads; Refresh runs on the GUI thread.
  auto const refresh = [this]() { QMetaObject::invokeMethod(this, "Refresh", Qt::QueuedConnection); };
  m_framework.SetPlacePageListeners(refresh, refresh, refresh, {} /* onSwitchFullScreen */);
}

void PlacePage::close()
{
  m_framework.DeactivateMapSelection();
}

void PlacePage::Refresh()
{
  m_open = m_framework.HasPlacePageInfo();
  if (!m_open)
  {
    emit changed();
    return;
  }

  auto const & info = m_framework.GetCurrentPlacePageInfo();
  using feature::Metadata;
  m_title = ToQString(info.GetTitle());
  if (m_title.isEmpty())
    m_title = ToQString(info.GetSecondaryTitle());
  m_subtitle = ToQString(info.GetSubtitle());
  m_address = ToQString(info.GetSecondarySubtitle());
  m_openingHours = ToQString(info.GetOpeningHours());
  m_phone = ToQString(info.GetMetadata(Metadata::FMD_PHONE_NUMBER));
  m_website = ToQString(info.GetMetadata(Metadata::FMD_WEBSITE));
  m_email = ToQString(info.GetMetadata(Metadata::FMD_EMAIL));
  m_coordinates = ToQString(info.GetFormattedCoordinate(place_page::CoordinatesFormat::LatLonDecimal));
  m_description = ToQString(info.GetWikiDescription());
  emit changed();
}
}  // namespace sailfish
