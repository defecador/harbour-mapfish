#pragma once

// Qt 5.6's moc can't parse the C++23 in CoMaps' headers: keep them out of this one.
#include <QtCore/QAbstractListModel>
#include <QtCore/QString>
#include <QtCore/QStringList>

class Framework;

namespace sailfish
{
// One level of the downloadable map tree: the children of |parentId|, or the top level if empty.
class CountryModel : public QAbstractListModel
{
  Q_OBJECT
  Q_PROPERTY(QString parentId READ ParentId WRITE SetParentId NOTIFY parentIdChanged)
  Q_PROPERTY(QString parentName READ ParentName NOTIFY parentIdChanged)

public:
  enum Roles
  {
    IdRole = Qt::UserRole + 1,
    NameRole,
    ExpandableRole,
    StatusRole,
    SizeRole,
    ProgressRole,
  };

  explicit CountryModel(QObject * parent = nullptr);
  ~CountryModel() override;

  static void SetFramework(Framework * framework);

  int rowCount(QModelIndex const & parent = QModelIndex()) const override;
  QVariant data(QModelIndex const & index, int role) const override;
  QHash<int, QByteArray> roleNames() const override;

  QString ParentId() const { return m_parentId; }
  void SetParentId(QString const & parentId);
  QString ParentName() const;

  Q_INVOKABLE void download(QString const & countryId);
  Q_INVOKABLE void cancel(QString const & countryId);
  Q_INVOKABLE void remove(QString const & countryId);
  Q_INVOKABLE void show(QString const & countryId);

signals:
  void parentIdChanged();

private:
  void Reload();
  void OnCountryChanged(QString const & countryId);

  Framework & m_framework;
  QString m_parentId;
  QStringList m_children;
  int m_subscription = 0;
};
}  // namespace sailfish
