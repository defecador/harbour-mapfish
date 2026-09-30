#include "platform/location_service/qt_position_source.hpp"

#include "base/logging.hpp"

#include "std/target_os.hpp"

QtPositionSource::QtPositionSource(std::string const & sourceName, QObject * parent) : QObject(parent)
{
#if defined(OMIM_OS_SAILFISH)
  // Sailfish only ships one positioning plugin (geoclue), but its registered source name isn't
  // documented anywhere and guessing it wrong makes createSource() return null; every Sailfish
  // app that uses QtPositioning just takes the default source instead.
  (void)sourceName;
  m_source = QGeoPositionInfoSource::createDefaultSource(this);
#else
  QVariantMap params;
  params["desktopId"] = "app.comaps.comaps";
  m_source = QGeoPositionInfoSource::createSource(QString::fromStdString(sourceName), params, this);
#endif

  if (!m_source)
  {
    LOG(LWARNING, ("Failed to acquire QGeoPositionInfoSource from ", sourceName));
    return;
  }

  if (!connect(m_source, &QGeoPositionInfoSource::positionUpdated, this, &QtPositionSource::OnLocationUpdate))
  {
    LOG(LERROR, ("Failed to connect the signal:", "positionUpdated"));
    return;
  }
  LOG(LDEBUG, ("Signal successfully connected:", "positionUpdated"));

  // Qt 5.6 (Sailfish) only has the pre-5.15 "error" signal; errorOccurred is the later rename.
#if defined(OMIM_OS_SAILFISH)
  bool const errorConnected = connect(m_source, SIGNAL(error(QGeoPositionInfoSource::Error)), this,
                                       SLOT(OnErrorOccurred(QGeoPositionInfoSource::Error)));
#else
  bool const errorConnected =
      connect(m_source, &QGeoPositionInfoSource::errorOccurred, this, &QtPositionSource::OnErrorOccurred);
#endif
  if (!errorConnected)
  {
    LOG(LERROR, ("Failed to connect the signal:", "errorOccurred"));
    return;
  }
  LOG(LDEBUG, ("Signal successfully connected:", "errorOccurred"));

  // Sailfish's QtPositioning module predates this signal entirely.
#if !defined(OMIM_OS_SAILFISH)
  if (!connect(m_source, &QGeoPositionInfoSource::supportedPositioningMethodsChanged, this,
               &QtPositionSource::OnSupportedPositioningMethodsChanged))
  {
    LOG(LERROR, ("Failed to connect the signal:", "supportedPositioningMethodsChanged"));
    return;
  }
  LOG(LDEBUG, ("Signal successfully connected:", "supportedPositioningMethodsChanged"));
#endif

  m_source->setUpdateInterval(1000);
  m_source->setPreferredPositioningMethods(QGeoPositionInfoSource::AllPositioningMethods);
}

std::string QtPositionSource::SourceName() const
{
  return m_source ? m_source->sourceName().toStdString() : std::string();
}

void QtPositionSource::Start()
{
  if (!m_source)
    return;
  LOG(LDEBUG, ("Starting Updates from:", SourceName()));
#if defined(OMIM_OS_SAILFISH)
  // Sailfish's geoclue 0.12 Qt plugin doesn't answer one-shot requestUpdate() calls (they just
  // never complete); every other app on this platform gets a fix via continuous updates instead.
  m_clientIsActive = true;
  m_source->startUpdates();
#else
  // Request the first update with a timeout to 30 minutes which is needed on devices that don't make use of `A-GNSS`
  // and can't get a lock within Qt's default `UPDATE_TIMEOUT_COLDSTART` (currently 2 minutes).
  m_source->requestUpdate(1800000);
#endif
}

void QtPositionSource::Stop()
{
  if (m_source && m_clientIsActive)
  {
    LOG(LDEBUG, ("Stopping Updates from:", SourceName()));
    m_source->stopUpdates();
  }
}

void QtPositionSource::OnLocationUpdate(QGeoPositionInfo const & info)
{
  if (!info.isValid())
  {
    LOG(LWARNING, ("Location update with Invalid timestamp or coordinates from:", SourceName()));
    return;
  }
  emit locationUpdated(info);
  if (!m_clientIsActive)
  {
    m_clientIsActive = true;
    m_source->startUpdates();
  }
}

void QtPositionSource::OnErrorOccurred(QGeoPositionInfoSource::Error error)
{
  LOG(LWARNING, ("Location error occured QGeoPositionInfoSource::Error code:", static_cast<int>(error)));
  m_clientIsActive = false;
  emit errorOccurred(error);
}

void QtPositionSource::OnSupportedPositioningMethodsChanged()
{
  auto const methods = m_source->supportedPositioningMethods();
  LOG(LDEBUG, ("Supported Positioning Method changed for:", SourceName(), "to:", methods));
  if (methods == QGeoPositionInfoSource::NoPositioningMethods)
  {
    m_clientIsActive = false;
    emit errorOccurred(QGeoPositionInfoSource::ClosedError);
  }
}
