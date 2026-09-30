#pragma once

// Qt 5.6's moc can't parse the C++23 in CoMaps' headers: keep them out of this one.
#include <QtCore/QObject>
#include <QtCore/QString>

class Framework;

namespace sailfish
{
// Mirrors what is selected on the map for QML. There is one selection, so there is one instance.
class PlacePage : public QObject
{
  Q_OBJECT
  Q_PROPERTY(bool open READ Open NOTIFY changed)
  Q_PROPERTY(QString title READ Title NOTIFY changed)
  Q_PROPERTY(QString subtitle READ Subtitle NOTIFY changed)
  Q_PROPERTY(QString address READ Address NOTIFY changed)
  Q_PROPERTY(QString openingHours READ OpeningHours NOTIFY changed)
  Q_PROPERTY(QString phone READ Phone NOTIFY changed)
  Q_PROPERTY(QString website READ Website NOTIFY changed)
  Q_PROPERTY(QString email READ Email NOTIFY changed)
  Q_PROPERTY(QString coordinates READ Coordinates NOTIFY changed)
  Q_PROPERTY(QString description READ Description NOTIFY changed)

public:
  explicit PlacePage(Framework & framework, QObject * parent = nullptr);

  bool Open() const { return m_open; }
  QString Title() const { return m_title; }
  QString Subtitle() const { return m_subtitle; }
  QString Address() const { return m_address; }
  QString OpeningHours() const { return m_openingHours; }
  QString Phone() const { return m_phone; }
  QString Website() const { return m_website; }
  QString Email() const { return m_email; }
  QString Coordinates() const { return m_coordinates; }
  QString Description() const { return m_description; }

  // Deselects the object on the map, which closes the page.
  Q_INVOKABLE void close();

signals:
  void changed();

private slots:
  void Refresh();

private:
  Framework & m_framework;
  bool m_open = false;
  QString m_title;
  QString m_subtitle;
  QString m_address;
  QString m_openingHours;
  QString m_phone;
  QString m_website;
  QString m_email;
  QString m_coordinates;
  QString m_description;
};
}  // namespace sailfish
