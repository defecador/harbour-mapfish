#include "sailfish/src/location_controller.hpp"

#include "platform/location.hpp"
#include "platform/location_service/location_service.hpp"

#include "map/framework.hpp"

#include "base/logging.hpp"

namespace sailfish
{
// Not a QObject: only LocationController needs to be moc'd, and keeping this class out of the
// header is what lets the header avoid platform/location.hpp (see location_controller.hpp).
class LocationControllerImpl : public location::LocationObserver
{
public:
  explicit LocationControllerImpl(Framework & framework)
    : m_service(CreateDesktopLocationService(*this)), m_framework(framework)
  {
  }

  std::unique_ptr<location::LocationService> m_service;

private:
  void OnLocationError(location::TLocationError errorCode) override
  {
    LOG(LWARNING, ("Location error:", errorCode));
    m_framework.OnLocationError(errorCode);
  }

  void OnLocationUpdated(location::GpsInfo const & info) override { m_framework.OnLocationUpdate(info); }

  Framework & m_framework;
};

LocationController::LocationController(Framework & framework, QObject * parent)
  : QObject(parent), m_framework(framework), m_impl(std::make_unique<LocationControllerImpl>(framework))
{
  // Must be set before the drape engine is created; app_main does that before loading QML.
  m_framework.SetMyPositionModeListener([this](location::EMyPositionMode mode, bool /* routingActive */)
  {
    LOG(LINFO, ("My-position mode changed to", static_cast<int>(mode)));
    m_mode = static_cast<int>(mode);
    emit modeChanged();
    // The framework only enters "pending" once the user asks to be located, mirroring the desktop
    // app: there is no point running the GPS before that.
    if (mode == location::PendingPosition)
      m_impl->m_service->Start();
  });
}

LocationController::~LocationController() = default;

void LocationController::locate()
{
  LOG(LINFO, ("locate() called, mode was", m_mode));
  m_framework.SwitchMyPositionNextMode();
}
}  // namespace sailfish
