#pragma once

#include <QtCore/QObject>
#include <QtCore/QPointF>
#include <QtCore/QString>
#include <QtCore/QVector>

class Framework;
class QNetworkAccessManager;
class QNetworkReply;

namespace sailfish
{
// A single leg of a public-transport itinerary (a walk, or a ride on one line). Plain Qt types
// only, so this can live in the moc'd header alongside TransitRouteController.
struct TransitLeg
{
  QString m_mode;      // Transitous' mode string: WALK, TRAM, BUS, SUBWAY, RAIL flavours, ...
  QString m_line;      // Route short name or headsign, e.g. "U1" or "13A". Empty for a walk.
  QString m_toName;    // Name of the stop/place this leg ends at.
  int m_durationSec = 0;
  double m_distanceM = 0;  // Only meaningful (and only used) for WALK legs.
  QString m_color;         // "#RRGGBB", or empty if Transitous didn't supply one.
  QVector<QPointF> m_points;  // (lon, lat) pairs decoded from the leg's polyline.
};

struct TransitItinerary
{
  int m_durationSec = 0;
  int m_transfers = 0;
  double m_walkDistanceM = 0;
  QVector<TransitLeg> m_legs;
};

// Public-transport directions from the device's position to the current place-page selection,
// queried from Transitous (api.transitous.org), a free community-run journey planner. Requires
// network access; CoMaps' own offline data only has the subway/S-Bahn layer, not full timetabled
// routing. Exposed to QML as "transitRoute". There is one route at a time, so there is one
// instance.
class TransitRouteController : public QObject
{
  Q_OBJECT
  Q_PROPERTY(bool active READ Active NOTIFY changed)
  Q_PROPERTY(bool building READ Building NOTIFY changed)
  Q_PROPERTY(QString error READ Error NOTIFY changed)
  Q_PROPERTY(QString summary READ Summary NOTIFY changed)
  Q_PROPERTY(QString legs READ Legs NOTIFY changed)
  // 0 = fastest, 1 = fewest changes, 2 = least walking. Re-sorts the already-fetched itineraries;
  // does not re-query.
  Q_PROPERTY(int sort READ Sort WRITE SetSort NOTIFY changed)
  Q_PROPERTY(bool excludeTram READ ExcludeTram WRITE SetExcludeTram NOTIFY changed)
  Q_PROPERTY(bool excludeBus READ ExcludeBus WRITE SetExcludeBus NOTIFY changed)
  Q_PROPERTY(bool excludeSubway READ ExcludeSubway WRITE SetExcludeSubway NOTIFY changed)
  Q_PROPERTY(bool excludeRail READ ExcludeRail WRITE SetExcludeRail NOTIFY changed)

public:
  explicit TransitRouteController(Framework & framework, QObject * parent = nullptr);

  bool Active() const { return m_active; }
  bool Building() const { return m_building; }
  QString Error() const { return m_error; }
  QString Summary() const { return m_summary; }
  QString Legs() const { return m_legs; }

  int Sort() const { return m_sort; }
  void SetSort(int sort);

  bool ExcludeTram() const { return m_excludeTram; }
  void SetExcludeTram(bool v)
  {
    m_excludeTram = v;
    emit changed();
  }
  bool ExcludeBus() const { return m_excludeBus; }
  void SetExcludeBus(bool v)
  {
    m_excludeBus = v;
    emit changed();
  }
  bool ExcludeSubway() const { return m_excludeSubway; }
  void SetExcludeSubway(bool v)
  {
    m_excludeSubway = v;
    emit changed();
  }
  bool ExcludeRail() const { return m_excludeRail; }
  void SetExcludeRail(bool v)
  {
    m_excludeRail = v;
    emit changed();
  }

  // Queries Transitous for the place-page selection. Does nothing without a selection or a GPS
  // fix; does nothing if every transport type has been excluded.
  Q_INVOKABLE void routeToSelection();
  Q_INVOKABLE void clear();

signals:
  void changed();

private slots:
  void OnReply();

private:
  void ApplyBestItinerary();
  void DrawSelected(TransitItinerary const & itinerary);

  Framework & m_framework;
  QNetworkAccessManager * m_net;
  QVector<TransitItinerary> m_itineraries;

  bool m_active = false;
  bool m_building = false;
  int m_sort = 0;
  bool m_excludeTram = false;
  bool m_excludeBus = false;
  bool m_excludeSubway = false;
  bool m_excludeRail = false;
  QString m_error;
  QString m_summary;
  QString m_legs;
};
}  // namespace sailfish
