#include "sailfish/src/transit_route_controller.hpp"

#include "sailfish/src/trip_origin.hpp"

#include "map/framework.hpp"
#include "map/place_page_info.hpp"

#include "drape_frontend/drape_api.hpp"

#include "geometry/mercator.hpp"

#include <QtCore/QDateTime>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QUrlQuery>
#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkReply>
#include <QtNetwork/QNetworkRequest>

#include <algorithm>
#include <optional>
#include <set>
#include <cmath>

namespace sailfish
{
namespace
{
// api.transitous.org requires a User-Agent identifying the app and a way to reach its author;
// there's no per-user identity in it, just the project. TODO: point this at Mapfish's own
// repo/contact once it has one published.
char const kUserAgent[] = "harbour-mapfish/0.1 (Sailfish OS; unofficial CoMaps-based port)";
char const kApiUrl[] = "https://api.transitous.org/api/v6/plan";

QString FormatTransitDuration(int seconds)
{
  int const minutes = (seconds + 59) / 60;
  if (minutes < 60)
    return QObject::tr("%1 min").arg(minutes);
  return QObject::tr("%1 h %2 min").arg(minutes / 60).arg(minutes % 60);
}

QString FormatDistance(double meters)
{
  if (meters >= 1000)
    return QObject::tr("%1 km").arg(meters / 1000.0, 0, 'f', 1);
  return QObject::tr("%1 m").arg(static_cast<int>(meters));
}

// Decodes Transitous' leg polylines (Google's encoded polyline algorithm), returning (lon, lat)
// pairs; |precision| is the number of decimal digits the encoder scaled by (6 for the v2+ API).
QVector<QPointF> DecodePolyline(QString const & encoded, int precision)
{
  QVector<QPointF> result;
  double const factor = std::pow(10.0, precision);
  int index = 0;
  int const len = encoded.size();
  int lat = 0;
  int lng = 0;
  while (index < len)
  {
    int shift = 0;
    int value = 0;
    int b;
    do
    {
      b = encoded.at(index++).unicode() - 63;
      value |= (b & 0x1f) << shift;
      shift += 5;
    } while (b >= 0x20 && index < len);
    lat += (value & 1) ? ~(value >> 1) : (value >> 1);

    shift = 0;
    value = 0;
    do
    {
      b = encoded.at(index++).unicode() - 63;
      value |= (b & 0x1f) << shift;
      shift += 5;
    } while (b >= 0x20 && index < len);
    lng += (value & 1) ? ~(value >> 1) : (value >> 1);

    result.append(QPointF(lng / factor, lat / factor));
  }
  return result;
}

// The leaf transit modes Transitous distinguishes that are relevant to public transport (leaves
// out the on-demand/ride-sharing/"other" modes, which aren't what "no bus" etc. means here).
QStringList TransitModes(bool excludeTram, bool excludeBus, bool excludeSubway, bool excludeRail)
{
  QStringList modes;
  if (!excludeTram)
    modes << "TRAM";
  if (!excludeBus)
    modes << "BUS" << "COACH";
  if (!excludeSubway)
    modes << "SUBWAY";
  if (!excludeRail)
    modes << "SUBURBAN" << "REGIONAL_RAIL" << "LONG_DISTANCE" << "HIGHSPEED_RAIL" << "NIGHT_RAIL";
  modes << "FERRY" << "FUNICULAR" << "AERIAL_LIFT";
  return modes;
}

dp::Color ColorForLeg(TransitLeg const & leg)
{
  if (leg.m_color.size() == 7 && leg.m_color.startsWith('#'))
  {
    bool ok = false;
    uint32_t const rgb = leg.m_color.mid(1).toUInt(&ok, 16);
    if (ok)
      return dp::Color(static_cast<uint8_t>((rgb >> 16) & 0xff), static_cast<uint8_t>((rgb >> 8) & 0xff),
                        static_cast<uint8_t>(rgb & 0xff), 255);
  }
  // Transitous only knows the colour of some lines (Vienna's U-Bahn, not its buses or trams), so the
  // rest take the colour of their icon in the steps list.
  if (leg.m_mode == "WALK")
    return dp::Color(107, 114, 128, 255);
  if (leg.m_mode == "TRAM")
    return dp::Color(214, 58, 58, 255);
  if (leg.m_mode == "BUS" || leg.m_mode == "COACH")
    return dp::Color(30, 111, 217, 255);
  if (leg.m_mode == "SUBWAY")
    return dp::Color(123, 63, 160, 255);
  if (leg.m_mode == "FERRY")
    return dp::Color(24, 149, 168, 255);
  return dp::Color(46, 155, 94, 255);
}

// Transitous gives ISO 8601 timestamps in UTC; the traveller wants the clock on the wall.
QString LocalTime(QString const & iso)
{
  QDateTime const time = QDateTime::fromString(iso, Qt::ISODate);
  return time.isValid() ? time.toLocalTime().toString("HH:mm") : QString();
}

TransitItinerary ParseItinerary(QJsonObject const & obj)
{
  TransitItinerary itinerary;
  itinerary.m_durationSec = obj.value("duration").toInt();
  itinerary.m_transfers = obj.value("transfers").toInt();

  for (auto const & legValue : obj.value("legs").toArray())
  {
    QJsonObject const legObj = legValue.toObject();
    TransitLeg leg;
    leg.m_mode = legObj.value("mode").toString();
    leg.m_line = legObj.value("routeShortName").toString();
    if (leg.m_line.isEmpty())
      leg.m_line = legObj.value("headsign").toString();
    leg.m_fromName = legObj.value("from").toObject().value("name").toString();
    leg.m_toName = legObj.value("to").toObject().value("name").toString();
    leg.m_departure = LocalTime(legObj.value("startTime").toString());
    leg.m_arrival = LocalTime(legObj.value("endTime").toString());
    leg.m_durationSec = legObj.value("duration").toInt();
    leg.m_distanceM = legObj.value("distance").toDouble();
    QString color = legObj.value("routeColor").toString();
    if (!color.isEmpty() && !color.startsWith('#'))
      color = "#" + color;
    leg.m_color = color;

    QJsonObject const geometry = legObj.value("legGeometry").toObject();
    QString const points = geometry.value("points").toString();
    int const precision = geometry.value("precision").toInt(6);
    if (!points.isEmpty())
      leg.m_points = DecodePolyline(points, precision);

    if (leg.m_mode == "WALK")
      itinerary.m_walkDistanceM += leg.m_distanceM;

    itinerary.m_legs.append(leg);
  }
  return itinerary;
}
}  // namespace

TransitRouteController::TransitRouteController(Framework & framework, TripOrigin & origin, QObject * parent)
  : QObject(parent), m_framework(framework), m_origin(origin), m_net(new QNetworkAccessManager(this))
{
  connect(&m_origin, &TripOrigin::changed, this, &TransitRouteController::OnOriginChanged);
}

void TransitRouteController::SetSort(int sort)
{
  if (sort == m_sort)
    return;
  m_sort = sort;
  if (!m_itineraries.isEmpty())
    ApplyBestItinerary();
  else
    emit changed();
}

void TransitRouteController::routeToSelection()
{
  if (!m_framework.HasPlacePageInfo())
    return;

  auto const & destination = m_framework.GetCurrentPlacePageInfo().GetMercator();
  m_hasDestination = true;
  m_destinationX = destination.x;
  m_destinationY = destination.y;
  Query();
}

void TransitRouteController::OnOriginChanged()
{
  if (m_hasDestination)
    Query();
}

void TransitRouteController::Query()
{
  std::optional<m2::PointD> const start =
      m_origin.Custom() ? std::optional<m2::PointD>(m2::PointD(m_origin.X(), m_origin.Y()))
                        : m_framework.GetCurrentPosition();
  if (!start)
  {
    m_error = tr("Waiting for GPS position");
    m_active = false;
    emit changed();
    return;
  }

  QStringList const modes = TransitModes(m_excludeTram, m_excludeBus, m_excludeSubway, m_excludeRail);
  if (modes.isEmpty())
  {
    m_error = tr("Enable at least one transport type");
    m_active = false;
    emit changed();
    return;
  }

  ms::LatLon const from = mercator::ToLatLon(*start);
  ms::LatLon const to = mercator::ToLatLon(m2::PointD(m_destinationX, m_destinationY));

  QUrl url(kApiUrl);
  QUrlQuery query;
  query.addQueryItem("fromPlace", QString("%1,%2").arg(from.m_lat, 0, 'f', 6).arg(from.m_lon, 0, 'f', 6));
  query.addQueryItem("toPlace", QString("%1,%2").arg(to.m_lat, 0, 'f', 6).arg(to.m_lon, 0, 'f', 6));
  query.addQueryItem("transitModes", modes.join(','));
  query.addQueryItem("numItineraries", "6");
  url.setQuery(query);

  QNetworkRequest request(url);
  request.setRawHeader("User-Agent", kUserAgent);

  m_itineraries.clear();
  m_building = true;
  m_error.clear();
  emit changed();

  // A newer query replaces one still in flight; its answer would be for the wrong origin.
  if (m_reply)
  {
    m_reply->disconnect(this);
    m_reply->abort();
    m_reply->deleteLater();
  }
  m_reply = m_net->get(request);
  connect(m_reply, &QNetworkReply::finished, this, &TransitRouteController::OnReply);
}

void TransitRouteController::OnReply()
{
  auto * reply = qobject_cast<QNetworkReply *>(sender());
  if (!reply)
    return;
  reply->deleteLater();
  m_reply = nullptr;

  m_building = false;

  if (reply->error() != QNetworkReply::NoError)
  {
    m_active = false;
    m_error = tr("Could not reach the journey planner");
    emit changed();
    return;
  }

  QJsonDocument const doc = QJsonDocument::fromJson(reply->readAll());
  QJsonArray const itineraries = doc.object().value("itineraries").toArray();
  if (itineraries.isEmpty())
  {
    m_active = false;
    m_error = tr("No route found");
    emit changed();
    return;
  }

  for (auto const & value : itineraries)
    m_itineraries.append(ParseItinerary(value.toObject()));

  ApplyBestItinerary();
}

void TransitRouteController::ApplyBestItinerary()
{
  switch (m_sort)
  {
  case 1:  // Fewest changes.
    std::stable_sort(m_itineraries.begin(), m_itineraries.end(), [](auto const & a, auto const & b)
    { return a.m_transfers != b.m_transfers ? a.m_transfers < b.m_transfers : a.m_durationSec < b.m_durationSec; });
    break;
  case 2:  // Least walking.
    std::stable_sort(m_itineraries.begin(), m_itineraries.end(), [](auto const & a, auto const & b)
    {
      return a.m_walkDistanceM != b.m_walkDistanceM ? a.m_walkDistanceM < b.m_walkDistanceM
                                                      : a.m_durationSec < b.m_durationSec;
    });
    break;
  default:  // Fastest.
    std::stable_sort(m_itineraries.begin(), m_itineraries.end(),
                      [](auto const & a, auto const & b) { return a.m_durationSec < b.m_durationSec; });
    break;
  }

  TransitItinerary const & best = m_itineraries.front();
  m_active = true;
  m_error.clear();

  QString changesText;
  if (best.m_transfers == 0)
    changesText = tr("direct");
  else if (best.m_transfers == 1)
    changesText = tr("1 change");
  else
    changesText = tr("%1 changes").arg(best.m_transfers);
  m_summary = tr("%1 · %2 · %3 walking")
                  .arg(FormatTransitDuration(best.m_durationSec))
                  .arg(changesText)
                  .arg(FormatDistance(best.m_walkDistanceM));

  m_legs.clear();
  for (auto const & leg : best.m_legs)
  {
    QString text;
    if (leg.m_mode == "WALK")
      text = tr("Walk %1 to %2").arg(FormatTransitDuration(leg.m_durationSec)).arg(leg.m_toName);
    else
      text = tr("%1 to %2 (%3)")
                 .arg(leg.m_line.isEmpty() ? leg.m_mode : leg.m_line)
                 .arg(leg.m_toName)
                 .arg(FormatTransitDuration(leg.m_durationSec));
    if (!leg.m_departure.isEmpty())
      text = leg.m_departure + "  " + text;
    QVariantMap item;
    item["mode"] = leg.m_mode;
    item["text"] = text;
    m_legs.append(item);
  }

  DrawSelected(best);
  emit changed();
}

void TransitRouteController::DrawSelected(TransitItinerary const & itinerary)
{
  auto & drapeApi = m_framework.GetDrapeApi();
  drapeApi.Clear();

  // Drape API lines are keyed by id, and a marker's id is its text: keep identical labels apart.
  std::set<std::string> markerIds;
  auto const addMarker = [&](QString const & label, m2::PointD const & point, dp::Color const & color)
  {
    std::string id = label.toStdString();
    while (markerIds.count(id))
      id += ' ';
    markerIds.insert(id);
    drapeApi.AddLine(id, df::DrapeApiLineData({point}, color).Width(7.0f).ShowPoints(false).ShowId());
  };

  m2::RectD bounds;
  int legIndex = 0;
  bool boarded = false;
  m2::PointD lastPoint;
  bool hasLast = false;
  for (auto const & leg : itinerary.m_legs)
  {
    if (leg.m_points.size() < 2)
      continue;

    std::vector<m2::PointD> points;
    points.reserve(static_cast<size_t>(leg.m_points.size()));
    for (auto const & p : leg.m_points)
    {
      points.push_back(mercator::FromLatLon(p.y(), p.x()));
      bounds.Add(points.back());
    }
    lastPoint = points.back();
    hasLast = true;

    drapeApi.AddLine("transit_leg_" + std::to_string(legIndex++),
                      df::DrapeApiLineData(points, ColorForLeg(leg)).Width(leg.m_mode == "WALK" ? 8.0f : 16.0f));

    // Mark where each ride starts: the first boarding and every change after it, labelled with the
    // line, the stop and the time. The id of a drape API line is the text drawn at its first point.
    if (leg.m_mode != "WALK")
    {
      QString label = leg.m_line.isEmpty() ? leg.m_mode : leg.m_line;
      label = (boarded ? tr("Change to %1").arg(label) : label) + "  " + leg.m_fromName;
      if (!leg.m_departure.isEmpty())
        label += "\n" + leg.m_departure;
      boarded = true;
      addMarker(label, points.front(), ColorForLeg(leg));
    }
  }

  if (hasLast && !itinerary.m_legs.isEmpty() && !itinerary.m_legs.back().m_arrival.isEmpty())
    addMarker(tr("Arrive %1").arg(itinerary.m_legs.back().m_arrival), lastPoint, dp::Color(40, 40, 40, 255));

  if (bounds.IsValid())
  {
    // Show the whole journey. The bars cover the bottom of the map, so leave room there.
    double const w = bounds.SizeX();
    double const h = bounds.SizeY();
    bounds.Inflate(w * 0.2, h * 0.2);
    bounds.setMinY(bounds.minY() - h * 0.7);
    m_framework.ShowRect(bounds);
  }
}

void TransitRouteController::showStep(int index)
{
  if (m_itineraries.isEmpty())
    return;
  auto const & legs = m_itineraries.front().m_legs;
  if (index < 0 || index >= legs.size() || legs[index].m_points.isEmpty())
    return;

  QPointF const end = legs[index].m_points.back();
  m2::PointD const point = mercator::FromLatLon(end.y(), end.x());
  // Frame a few hundred metres around it, shifted so the point lands above the bars.
  m2::RectD rect = mercator::RectByCenterXYAndSizeInMeters(point, 350.0);
  rect.setMinY(rect.minY() - rect.SizeY() * 0.7);
  m_framework.ShowRect(rect, 17 /* maxScale */);
}

void TransitRouteController::clear()
{
  m_hasDestination = false;
  if (m_reply)
  {
    m_reply->disconnect(this);
    m_reply->abort();
    m_reply->deleteLater();
    m_reply = nullptr;
  }
  m_framework.GetDrapeApi().Clear();
  m_itineraries.clear();
  m_active = false;
  m_building = false;
  m_error.clear();
  m_summary.clear();
  m_legs.clear();
  emit changed();
}
}  // namespace sailfish
