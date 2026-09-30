#include "platform/location_service/location_service.hpp"
#include "platform/location_service/qt_position_source.hpp"

#include "platform/location.hpp"

#include "std/target_os.hpp"

#include <memory>

namespace
{
location::GpsInfo gpsInfoFromQGeoPositionInfo(QGeoPositionInfo const & i, location::TLocationSource source)
{
  location::GpsInfo info;
  info.m_source = source;

  info.m_latitude = i.coordinate().latitude();
  info.m_longitude = i.coordinate().longitude();
#if defined(OMIM_OS_SAILFISH)
  // Sailfish's Qt predates QDateTime::toSecsSinceEpoch (added in Qt 5.8).
  info.m_timestamp = static_cast<double>(i.timestamp().toMSecsSinceEpoch()) / 1000.0;
#else
  info.m_timestamp = static_cast<double>(i.timestamp().toSecsSinceEpoch());
#endif

  if (i.hasAttribute(QGeoPositionInfo::HorizontalAccuracy))
    info.m_horizontalAccuracy = static_cast<double>(i.attribute(QGeoPositionInfo::HorizontalAccuracy));

  if (i.hasAttribute(QGeoPositionInfo::VerticalAccuracy))
    info.m_verticalAccuracy = static_cast<double>(i.attribute(QGeoPositionInfo::VerticalAccuracy));

  if (i.hasAttribute(QGeoPositionInfo::Direction))
    info.m_bearing = static_cast<double>(i.attribute(QGeoPositionInfo::Direction));

  if (i.hasAttribute(QGeoPositionInfo::GroundSpeed))
    info.m_speed = static_cast<double>(i.attribute(QGeoPositionInfo::GroundSpeed));

  return info;
}

location::TLocationError tLocationErrorFromQGeoPositionInfoError(QGeoPositionInfoSource::Error error)
{
  location::TLocationError result = location::TLocationError::ENotSupported;
  switch (error)
  {
  case QGeoPositionInfoSource::AccessError: result = location::TLocationError::EDenied; break;
  case QGeoPositionInfoSource::ClosedError: result = location::TLocationError::EGPSIsOff; break;
  case QGeoPositionInfoSource::NoError: result = location::TLocationError::ENoError; break;
#if !defined(OMIM_OS_SAILFISH)
  // Sailfish's QtPositioning module predates this enumerator.
  case QGeoPositionInfoSource::UpdateTimeoutError: result = location::TLocationError::ETimeout; break;
#endif
  case QGeoPositionInfoSource::UnknownSourceError: result = location::TLocationError::EUnknown; break;
  default: break;
  }
  return result;
}

location::TLocationSource qStringToTLocationSource(std::string const & sourceName)
{
  // GpsInfo::IsValid() is keyed off this field, not the coordinates, so every recognized Qt
  // Positioning backend must map to something other than EUndefined. Sailfish's old geoclue 0.12
  // plugin reports its source as "geoclue", not "geoclue2".
  if (sourceName == "geoclue2" || sourceName == "geoclue")
    return location::TLocationSource::EGeoClue2;

  return location::TLocationSource::EUndefined;
}

// Adapts QtPositionSource (plain Qt types only, so moc can parse it on Qt 5.6) to
// location::LocationService. Not itself a QObject, so it is never handed to moc, which is what
// lets it freely include platform/location.hpp.
class QtLocationService : public location::LocationService
{
public:
  QtLocationService(location::LocationObserver & observer, std::string const & sourceName)
    : LocationService(observer), m_source(sourceName)
  {
    QObject::connect(&m_source, &QtPositionSource::locationUpdated, &m_source,
                      [this](QGeoPositionInfo const & info)
    { m_observer.OnLocationUpdated(gpsInfoFromQGeoPositionInfo(info, qStringToTLocationSource(m_source.SourceName()))); });
    QObject::connect(&m_source, &QtPositionSource::errorOccurred, &m_source, [this](QGeoPositionInfoSource::Error error)
    { m_observer.OnLocationError(tLocationErrorFromQGeoPositionInfoError(error)); });
  }

  void Start() override
  {
    if (m_source.IsValid())
      m_source.Start();
  }

  void Stop() override { m_source.Stop(); }

private:
  QtPositionSource m_source;
};
}  // namespace

std::unique_ptr<location::LocationService> CreateQtLocationService(location::LocationObserver & observer,
                                                                   std::string const & sourceName)
{
  return std::make_unique<QtLocationService>(observer, sourceName);
}
