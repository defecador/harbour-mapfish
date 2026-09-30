#include "sailfish/src/route_controller.hpp"

#include "map/framework.hpp"
#include "map/place_page_info.hpp"
#include "map/routing_manager.hpp"
#include "map/routing_mark.hpp"

#include "routing/following_info.hpp"
#include "routing/router.hpp"
#include "routing/routing_callbacks.hpp"

namespace sailfish
{
namespace
{
QString FormatTime(int seconds)
{
  int const minutes = (seconds + 59) / 60;
  if (minutes < 60)
    return QObject::tr("%1 min").arg(minutes);
  return QObject::tr("%1 h %2 min").arg(minutes / 60).arg(minutes % 60);
}

routing::RouterType ToRouterType(int mode)
{
  return mode == 1 ? routing::RouterType::Bicycle : routing::RouterType::Pedestrian;
}

QString ErrorMessage(routing::RouterResultCode code)
{
  using routing::RouterResultCode;
  switch (code)
  {
  case RouterResultCode::NoCurrentPosition: return QObject::tr("Waiting for GPS position");
  case RouterResultCode::StartPointNotFound:
  case RouterResultCode::EndPointNotFound:
  case RouterResultCode::PointsInDifferentMWM: return QObject::tr("No route found nearby");
  case RouterResultCode::RouteNotFound:
  case RouterResultCode::RouteNotFoundRedressRouteError: return QObject::tr("No route found");
  case RouterResultCode::NeedMoreMaps: return QObject::tr("Download more maps to reach this point");
  case RouterResultCode::Cancelled: return QString();
  default: return QObject::tr("Could not build a route");
  }
}
}  // namespace

RouteController::RouteController(Framework & framework, QObject * parent) : QObject(parent), m_framework(framework)
{
  m_framework.GetRoutingManager().SetRouteBuildingListener(
      [this](routing::RouterResultCode code, storage::CountriesSet const &)
  {
    m_building = false;
    m_hasRoute = m_framework.GetRoutingManager().GetRoutePointsCount() >= 2;
    if (code == routing::RouterResultCode::NoError || code == routing::RouterResultCode::HasWarnings)
    {
      routing::FollowingInfo info;
      m_framework.GetRoutingManager().GetRouteFollowingInfo(info);
      m_active = info.IsValid();
      m_distance = QString::fromStdString(info.m_distToTarget.ToString());
      m_time = FormatTime(info.m_time);
      m_error.clear();
    }
    else
    {
      m_active = false;
      m_distance.clear();
      m_time.clear();
      m_error = ErrorMessage(code);
    }
    emit changed();
  });
}

void RouteController::SetMode(int mode)
{
  if (mode == m_mode)
    return;
  m_mode = mode;
  auto & routingManager = m_framework.GetRoutingManager();
  routingManager.SetRouter(ToRouterType(m_mode));
  if (m_hasRoute)
  {
    m_building = true;
    routingManager.BuildRoute();
  }
  emit changed();
}

void RouteController::routeToSelection()
{
  if (!m_framework.HasPlacePageInfo())
    return;

  auto & routingManager = m_framework.GetRoutingManager();
  routingManager.CloseRouting(true /* removeRoutePoints */);
  routingManager.SetRouter(ToRouterType(m_mode));

  RouteMarkData start;
  start.m_pointType = RouteMarkType::Start;
  start.m_isMyPosition = true;
  routingManager.AddRoutePoint(std::move(start));

  RouteMarkData finish;
  finish.m_pointType = RouteMarkType::Finish;
  finish.m_position = m_framework.GetCurrentPlacePageInfo().GetMercator();
  routingManager.AddRoutePoint(std::move(finish));

  m_building = true;
  m_hasRoute = true;
  m_error.clear();
  emit changed();
  routingManager.BuildRoute();
}

void RouteController::addStopAtSelection()
{
  if (!m_hasRoute || !m_framework.HasPlacePageInfo())
    return;

  auto & routingManager = m_framework.GetRoutingManager();
  if (!routingManager.CouldAddIntermediatePoint())
    return;

  RouteMarkData stop;
  stop.m_pointType = RouteMarkType::Intermediate;
  stop.m_position = m_framework.GetCurrentPlacePageInfo().GetMercator();
  routingManager.AddRoutePoint(std::move(stop), true /* reorderIntermediatePoints */);

  m_building = true;
  m_error.clear();
  emit changed();
  routingManager.BuildRoute();
}

void RouteController::clear()
{
  m_framework.GetRoutingManager().CloseRouting(true /* removeRoutePoints */);
  m_active = false;
  m_building = false;
  m_hasRoute = false;
  m_distance.clear();
  m_time.clear();
  m_error.clear();
  emit changed();
}
}  // namespace sailfish
