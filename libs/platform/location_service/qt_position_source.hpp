#pragma once

// Deliberately Qt-only: CoMaps' headers pull in 3party/ankerl/unordered_dense.h, whose C++23
// moc on Qt 5.6 (Sailfish) can't parse. Keeping this the only moc'd file in this directory, and
// translating to/from platform/location.hpp types in qt_location_service.cpp (not moc'd), keeps
// that header out of anything moc has to read.
#include <QGeoPositionInfoSource>
#include <QObject>

#include <string>

class QtPositionSource : public QObject
{
  Q_OBJECT

public:
  explicit QtPositionSource(std::string const & sourceName, QObject * parent = nullptr);

  bool IsValid() const { return m_source != nullptr; }
  std::string SourceName() const;

  void Start();
  void Stop();

signals:
  void locationUpdated(QGeoPositionInfo const & info);
  void errorOccurred(QGeoPositionInfoSource::Error error);

private slots:
  void OnLocationUpdate(QGeoPositionInfo const & info);
  void OnErrorOccurred(QGeoPositionInfoSource::Error error);
  void OnSupportedPositioningMethodsChanged();

private:
  QGeoPositionInfoSource * m_source = nullptr;
  // Unfortunately when the source is `geoclue2`
  // we would need access to the `Active` D-Bus property
  // https://www.freedesktop.org/software/geoclue/docs
  // /gdbus-org.freedesktop.GeoClue2.Client.html#gdbus-property-org-freedesktop-GeoClue2-Client.Active
  // But `QGeoPositionInfoSource` doesn't expose that so we have to deduce its state.
  bool m_clientIsActive = false;
};
