#pragma once

// Qt 5.6's moc can't parse the C++23 in CoMaps' headers: keep them out of this one.
#include <QtCore/QAbstractListModel>
#include <QtCore/QString>
#include <QtCore/QStringList>

#include <cstdint>
#include <memory>

class Framework;

namespace search
{
class Results;
}  // namespace search

namespace sailfish
{
class TripOrigin;

// Results of a search across all downloaded maps, refreshed as the query changes.
class SearchModel : public QAbstractListModel
{
  Q_OBJECT
  Q_PROPERTY(QString query READ Query WRITE SetQuery NOTIFY queryChanged)
  Q_PROPERTY(bool busy READ Busy NOTIFY busyChanged)
  // True while the query is empty and the rows are the recent searches rather than results.
  Q_PROPERTY(bool showingHistory READ ShowingHistory NOTIFY queryChanged)

public:
  enum Roles
  {
    NameRole = Qt::UserRole + 1,
    TypeRole,
    AddressRole,
    SuggestionRole,
  };

  explicit SearchModel(QObject * parent = nullptr);
  ~SearchModel() override;

  static void SetFramework(Framework * framework);
  static void SetTripOrigin(TripOrigin * origin);

  int rowCount(QModelIndex const & parent = QModelIndex()) const override;
  QVariant data(QModelIndex const & index, int role) const override;
  QHash<int, QByteArray> roleNames() const override;

  QString Query() const { return m_query; }
  void SetQuery(QString const & query);
  bool Busy() const { return m_busy; }
  bool ShowingHistory() const { return m_query.trimmed().isEmpty(); }

  // Moves the map to a result and marks it. Suggestions aren't places: QML replaces the query.
  Q_INVOKABLE void show(int row);
  // Makes a result the start of trips instead of the device's position. Does nothing for rows
  // that are not places.
  Q_INVOKABLE void pickOrigin(int row);
  // Forgets the recent searches shown while the query is empty.
  Q_INVOKABLE void clearHistory();
  // Leaves the markers of the current results on the map, or clears them.
  Q_INVOKABLE void clearMarks();

signals:
  void queryChanged();
  void busyChanged();

private:
  void OnResults(uint64_t timestamp, search::Results && results);
  void SetBusy(bool busy);
  void LoadHistory();

  Framework & m_framework;
  QString m_query;
  bool m_busy = false;
  uint64_t m_timestamp = 0;
  std::unique_ptr<search::Results> m_results;
  QStringList m_history;
};
}  // namespace sailfish
