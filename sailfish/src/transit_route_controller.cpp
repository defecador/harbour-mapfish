#include "sailfish/src/transit_route_controller.hpp"

#include "map/framework.hpp"
#include "map/place_page_info.hpp"

#include "drape_frontend/drape_api.hpp"

#include "geometry/mercator.hpp"

#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QUrlQuery>
#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkReply>
#include <QtNetwork/QNetworkRequest>

#include <algorithm>
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
  if (leg.m_mode == "WALK")
    return dp::Color(120, 120, 120, 255);
  if (leg.m_mode == "TRAM")
    return dp::Color(200, 30, 30, 255);
  if (leg.m_mode == "BUS" || leg.m_mode == "COACH")
    return dp::Color(130, 30, 150, 255);
  if (leg.m_mode == "SUBWAY")
    return dp::Color(30, 100, 200, 255);
  return dp::Color(30, 150, 90, 255);
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
    leg.m_toName = legObj.value("to").toObject().value("name").toString();
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

TransitRouteController::TransitRouteController(Framework & framework, QObject * parent)
  : QObject(parent), m_framework(framework), m_net(new QNetworkAccessManager(this))
{
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

  auto const myPosition = m_framework.GetCurrentPosition();
  if (!myPosition)
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

  ms::LatLon const from = mercator::ToLatLon(*myPosition);
  ms::LatLon const to = mercator::ToLatLon(m_framework.GetCurrentPlacePageInfo().GetMercator());

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

  connect(m_net->get(request), &QNetworkReply::finished, this, &TransitRouteController::OnReply);
}

void TransitRouteController::OnReply()
{
  auto * reply = qobject_cast<QNetworkReply *>(sender());
  if (!reply)
    return;
  reply->deleteLater();

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

  int legIndex = 0;
  for (auto const & leg : itinerary.m_legs)
  {
    if (leg.m_points.size() < 2)
      continue;

    std::vector<m2::PointD> points;
    points.reserve(static_cast<size_t>(leg.m_points.size()));
    for (auto const & p : leg.m_points)
      points.push_back(mercator::FromLatLon(p.y(), p.x()));

    drapeApi.AddLine("transit_leg_" + std::to_string(legIndex++),
                      df::DrapeApiLineData(points, ColorForLeg(leg)).Width(leg.m_mode == "WALK" ? 3.0f : 5.0f));
  }
}

void TransitRouteController::clear()
{
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
