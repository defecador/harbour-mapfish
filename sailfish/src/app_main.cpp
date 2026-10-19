#include "sailfish/src/country_model.hpp"
#include "sailfish/src/location_controller.hpp"
#include "sailfish/src/map_layers.hpp"
#include "sailfish/src/map_item.hpp"
#include "sailfish/src/place_page.hpp"
#include "sailfish/src/route_controller.hpp"
#include "sailfish/src/search_model.hpp"
#include "sailfish/src/transit_route_controller.hpp"
#include "sailfish/src/trip_origin.hpp"

#include "map/framework.hpp"

#include "platform/platform.hpp"

#include "base/logging.hpp"

#include <QtGui/QGuiApplication>
#include <QtGui/QSurfaceFormat>
#include <QtQml/QQmlContext>
#include <QtQml/QQmlEngine>
#include <QtQuick/QQuickView>

#include <sailfishapp.h>

#include <cstdlib>
#include <memory>

__attribute__((visibility("default"))) int ComapsMain(int argc, char * argv[])
{
  // CoMaps parses doubles expecting '.' as the decimal separator.
  (void)::setenv("LC_NUMERIC", "C", 1);

  // Sets the organization and application names from the desktop file, which the platform layer
  // needs to find the directories sailjail allows.
  std::unique_ptr<QGuiApplication> app(SailfishApp::application(argc, argv));
  // SailfishApp doesn't always pick these up from the desktop file; they must match its
  // [X-Sailjail] section, or the sandbox forbids the data directories.
  QCoreApplication::setOrganizationName(QStringLiteral("app.mapfish"));
  QCoreApplication::setApplicationName(QStringLiteral("mapfish"));

  Platform & platform = GetPlatform();
  LOG(LINFO, ("Mapfish, CoMaps", platform.Version(), "on Sailfish OS, Qt", qVersion()));
  LOG(LINFO, ("Resources:", platform.ResourcesDir(), "Writable:", platform.WritableDir()));

  int result = 0;
  {
    Framework framework;
    sailfish::MapItem::SetFramework(&framework);
    qmlRegisterType<sailfish::MapItem>("app.mapfish", 1, 0, "MapItem");
    sailfish::CountryModel::SetFramework(&framework);
    qmlRegisterType<sailfish::CountryModel>("app.mapfish", 1, 0, "CountryModel");
    sailfish::SearchModel::SetFramework(&framework);
    qmlRegisterType<sailfish::SearchModel>("app.mapfish", 1, 0, "SearchModel");

    sailfish::PlacePage placePage(framework);
    sailfish::LocationController locationController(framework);
    sailfish::MapLayers mapLayers(framework);
    sailfish::TripOrigin tripOrigin;
    sailfish::SearchModel::SetTripOrigin(&tripOrigin);
    sailfish::RouteController routeController(framework, tripOrigin);
    sailfish::TransitRouteController transitRouteController(framework, tripOrigin);

    std::unique_ptr<QQuickView> view(SailfishApp::createView());
    // Drape's contexts are OpenGL ES 3 and share textures with Qt Quick's; the Mali driver only
    // shares between contexts of the same version.
    QSurfaceFormat format = view->format();
    format.setVersion(3, 0);
    view->setFormat(format);
    view->rootContext()->setContextProperty(QStringLiteral("placePage"), &placePage);
    view->rootContext()->setContextProperty(QStringLiteral("location"), &locationController);
    view->rootContext()->setContextProperty(QStringLiteral("layers"), &mapLayers);
    view->rootContext()->setContextProperty(QStringLiteral("origin"), &tripOrigin);
    view->rootContext()->setContextProperty(QStringLiteral("route"), &routeController);
    view->rootContext()->setContextProperty(QStringLiteral("transitRoute"), &transitRouteController);
    view->setSource(SailfishApp::pathToMainQml());
    view->show();

    result = app->exec();

    // The map item owns the drape engine and must go before the framework does.
    view.reset();
  }
  return result;
}
