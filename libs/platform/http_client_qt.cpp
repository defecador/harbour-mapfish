#include "platform/http_client.hpp"

#include "base/logging.hpp"
#include "base/string_utils.hpp"

#include <QtCore/QBuffer>
#include <QtCore/QByteArray>
#include <QtCore/QEventLoop>
#include <QtCore/QFile>
#include <QtCore/QTimer>
#include <QtCore/QUrl>
#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkReply>
#include <QtNetwork/QNetworkRequest>

#include <memory>

// Sailfish's app sandbox can't run the curl binary that the other Linux client spawns, so this one
// does the same synchronous request in-process. Callers run it on worker threads; a private
// manager and event loop keep it independent of the GUI thread.
namespace platform
{
bool HttpClient::RunHttpRequest()
{
  QNetworkAccessManager manager;
  QNetworkRequest request(QUrl(QString::fromStdString(m_urlRequested)));
  request.setAttribute(QNetworkRequest::FollowRedirectsAttribute, m_followRedirects);
  for (auto const & header : m_headers)
    request.setRawHeader(QByteArray::fromStdString(header.first), QByteArray::fromStdString(header.second));
  if (!m_cookies.empty())
    request.setRawHeader("Cookie", QByteArray::fromStdString(m_cookies));

  QByteArray body = QByteArray::fromStdString(m_bodyData);
  if (body.isEmpty() && !m_inputFile.empty())
  {
    QFile input(QString::fromStdString(m_inputFile));
    if (!input.open(QIODevice::ReadOnly))
    {
      LOG(LERROR, ("Can't read request body from", m_inputFile));
      return false;
    }
    body = input.readAll();
  }

  // Qt 5.6 only takes a request body as a device.
  QBuffer bodyDevice(&body);
  bodyDevice.open(QIODevice::ReadOnly);
  std::unique_ptr<QNetworkReply> reply(
      manager.sendCustomRequest(request, QByteArray::fromStdString(m_httpMethod), &bodyDevice));

  QEventLoop loop;
  QTimer timeout;
  timeout.setSingleShot(true);
  QObject::connect(&timeout, &QTimer::timeout, reply.get(), &QNetworkReply::abort);
  QObject::connect(reply.get(), &QNetworkReply::finished, &loop, &QEventLoop::quit);
  timeout.start(static_cast<int>(m_timeoutSec * 1000));
  loop.exec();

  QVariant const status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute);
  if (!status.isValid())
  {
    LOG(LWARNING, ("HTTP request to", m_urlRequested, "failed:", reply->errorString().toStdString()));
    m_errorCode = -1;
    return false;
  }
  m_errorCode = status.toInt();

  m_urlReceived = reply->url().toString().toStdString();
  if (!m_followRedirects)
  {
    QVariant const location = reply->attribute(QNetworkRequest::RedirectionTargetAttribute);
    if (location.isValid())
      m_urlReceived = reply->url().resolved(location.toUrl()).toString().toStdString();
  }

  m_headers.clear();
  std::string serverCookies;
  for (auto const & pair : reply->rawHeaderPairs())
  {
    std::string key = pair.first.toStdString();
    std::string const value = pair.second.toStdString();
    if (strings::EqualNoCase(key, "Set-Cookie"))
    {
      serverCookies += value + ", ";
    }
    else if (m_loadHeaders)
    {
      strings::AsciiToLower(key);
      m_headers.emplace(key, value);
    }
  }
  m_headers.emplace("Set-Cookie", NormalizeServerCookies(std::move(serverCookies)));

  QByteArray const response = reply->readAll();
  if (m_outputFile.empty())
  {
    m_serverResponse = response.toStdString();
    return true;
  }

  QFile output(QString::fromStdString(m_outputFile));
  if (!output.open(QIODevice::WriteOnly) || output.write(response) != response.size())
  {
    LOG(LERROR, ("Can't write HTTP response to", m_outputFile));
    return false;
  }
  return true;
}
}  // namespace platform
