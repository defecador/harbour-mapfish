#include "sailfish/src/map_item.hpp"

#include "map/framework.hpp"

#include "platform/platform.hpp"

#include "base/logging.hpp"

#include <QtGui/QGuiApplication>
#include <QtGui/QSurfaceFormat>
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
  QCoreApplication::setOrganizationName(QStringLiteral("app.comaps"));
  QCoreApplication::setApplicationName(QStringLiteral("comaps"));

  Platform & platform = GetPlatform();
  LOG(LINFO, ("CoMaps", platform.Version(), "on Sailfish OS, Qt", qVersion()));
  LOG(LINFO, ("Resources:", platform.ResourcesDir(), "Writable:", platform.WritableDir()));

  int result = 0;
  {
    Framework framework;
    sailfish::MapItem::SetFramework(&framework);
    qmlRegisterType<sailfish::MapItem>("app.comaps", 1, 0, "MapItem");

    std::unique_ptr<QQuickView> view(SailfishApp::createView());
    // Drape's contexts are OpenGL ES 3 and share textures with Qt Quick's; the Mali driver only
    // shares between contexts of the same version.
    QSurfaceFormat format = view->format();
    format.setVersion(3, 0);
    view->setFormat(format);
    view->setSource(SailfishApp::pathToMainQml());
    view->show();

    result = app->exec();

    // The map item owns the drape engine and must go before the framework does.
    view.reset();
  }
  return result;
}
