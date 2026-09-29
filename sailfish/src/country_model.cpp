#include "sailfish/src/country_model.hpp"

#include "map/framework.hpp"

#include "storage/storage.hpp"
#include "storage/storage_defines.hpp"

#include <algorithm>

namespace sailfish
{
namespace
{
Framework * g_countryFramework = nullptr;

QString StatusName(storage::NodeStatus status)
{
  using storage::NodeStatus;
  switch (status)
  {
  case NodeStatus::Downloading:
  case NodeStatus::Applying: return QStringLiteral("downloading");
  case NodeStatus::InQueue: return QStringLiteral("queued");
  case NodeStatus::Error: return QStringLiteral("error");
  case NodeStatus::OnDiskOutOfDate: return QStringLiteral("outdated");
  case NodeStatus::OnDisk: return QStringLiteral("downloaded");
  case NodeStatus::Partly: return QStringLiteral("partly");
  case NodeStatus::NotDownloaded:
  case NodeStatus::Undefined: return QStringLiteral("available");
  }
  return QStringLiteral("available");
}

std::string ToStd(QString const & s)
{
  return s.toStdString();
}
}  // namespace

void CountryModel::SetFramework(Framework * framework)
{
  g_countryFramework = framework;
}

CountryModel::CountryModel(QObject * parent) : QAbstractListModel(parent), m_framework(*g_countryFramework)
{
  // Storage reports changes on the GUI thread; progress arrives per downloading leaf, and a
  // group row shows its leaves' sum, so every change refreshes the whole (short) list.
  m_subscription = m_framework.GetStorage().Subscribe(
      [this](storage::CountryId const & id) { OnCountryChanged(QString::fromStdString(id)); },
      [this](storage::CountryId const & id, downloader::Progress const &)
  { OnCountryChanged(QString::fromStdString(id)); });
  Reload();
}

CountryModel::~CountryModel()
{
  m_framework.GetStorage().Unsubscribe(m_subscription);
}

void CountryModel::SetParentId(QString const & parentId)
{
  if (parentId == m_parentId)
    return;
  m_parentId = parentId;
  Reload();
  emit parentIdChanged();
}

QString CountryModel::ParentName() const
{
  if (m_parentId.isEmpty())
    return {};
  storage::NodeAttrs attrs;
  m_framework.GetStorage().GetNodeAttrs(ToStd(m_parentId), attrs);
  return QString::fromStdString(attrs.m_nodeLocalName);
}

void CountryModel::Reload()
{
  auto & storage = m_framework.GetStorage();
  storage::CountriesVec children;
  storage.GetChildren(m_parentId.isEmpty() ? storage.GetRootId() : ToStd(m_parentId), children);

  std::vector<std::pair<std::string, std::string>> named;
  named.reserve(children.size());
  for (auto const & id : children)
  {
    storage::NodeAttrs attrs;
    storage.GetNodeAttrs(id, attrs);
    named.emplace_back(attrs.m_nodeLocalName, id);
  }
  std::sort(named.begin(), named.end());

  beginResetModel();
  m_children.clear();
  for (auto const & [name, id] : named)
    m_children.append(QString::fromStdString(id));
  endResetModel();
}

void CountryModel::OnCountryChanged(QString const &)
{
  if (!m_children.isEmpty())
    emit dataChanged(index(0), index(m_children.size() - 1));
}

int CountryModel::rowCount(QModelIndex const & parent) const
{
  return parent.isValid() ? 0 : m_children.size();
}

QVariant CountryModel::data(QModelIndex const & index, int role) const
{
  if (!index.isValid() || index.row() >= m_children.size())
    return {};

  QString const & id = m_children[index.row()];
  if (role == IdRole)
    return id;

  auto const & storage = m_framework.GetStorage();
  if (role == ExpandableRole)
    return !storage.IsLeaf(ToStd(id));

  storage::NodeAttrs attrs;
  storage.GetNodeAttrs(ToStd(id), attrs);
  switch (role)
  {
  case NameRole: return QString::fromStdString(attrs.m_nodeLocalName);
  case StatusRole: return StatusName(attrs.m_status);
  case SizeRole: return static_cast<double>(attrs.m_mwmSize);
  case ProgressRole:
  {
    auto const & p = attrs.m_downloadingProgress;
    return p.m_bytesTotal > 0 ? static_cast<double>(p.m_bytesDownloaded) / p.m_bytesTotal : 0.0;
  }
  default: return {};
  }
}

QHash<int, QByteArray> CountryModel::roleNames() const
{
  return {
      {IdRole, "countryId"},  {NameRole, "name"}, {ExpandableRole, "expandable"},
      {StatusRole, "status"}, {SizeRole, "size"}, {ProgressRole, "progress"},
  };
}

void CountryModel::download(QString const & countryId)
{
  m_framework.GetStorage().DownloadNode(ToStd(countryId));
}

void CountryModel::cancel(QString const & countryId)
{
  m_framework.GetStorage().CancelDownloadNode(ToStd(countryId));
}

void CountryModel::remove(QString const & countryId)
{
  m_framework.GetStorage().DeleteNode(ToStd(countryId));
}

void CountryModel::show(QString const & countryId)
{
  m_framework.ShowNode(ToStd(countryId));
}
}  // namespace sailfish
