#include "sailfish/src/search_model.hpp"

#include "sailfish/src/trip_origin.hpp"

#include "map/everywhere_search_params.hpp"
#include "map/framework.hpp"

#include "search/result.hpp"

#include <QtGui/QGuiApplication>
#include <QtGui/QInputMethod>

namespace sailfish
{
namespace
{
Framework * g_searchFramework = nullptr;
TripOrigin * g_tripOrigin = nullptr;

std::string InputLocale()
{
  QString locale = QGuiApplication::inputMethod()->locale().name();
  locale.replace('_', '-');
  return locale.toStdString();
}
}  // namespace

void SearchModel::SetFramework(Framework * framework)
{
  g_searchFramework = framework;
}

void SearchModel::SetTripOrigin(TripOrigin * origin)
{
  g_tripOrigin = origin;
}

SearchModel::SearchModel(QObject * parent)
  : QAbstractListModel(parent)
  , m_framework(*g_searchFramework)
  , m_results(std::make_unique<search::Results>())
{
  LoadHistory();
}

SearchModel::~SearchModel()
{
  m_framework.GetSearchAPI().CancelAllSearches();
}

void SearchModel::SetQuery(QString const & query)
{
  if (query == m_query)
    return;
  // The rows switch between recent searches and results as the query goes empty or not.
  bool const wasHistory = ShowingHistory();
  if (wasHistory != query.trimmed().isEmpty())
    beginResetModel();
  m_query = query;
  if (wasHistory != ShowingHistory())
    endResetModel();
  emit queryChanged();

  // Results of older queries arrive late; the timestamp tells them apart.
  auto const timestamp = ++m_timestamp;
  std::string const normalized = query.trimmed().toStdString();
  if (normalized.empty())
  {
    m_framework.GetSearchAPI().CancelAllSearches();
    OnResults(timestamp, {});
    return;
  }

  search::EverywhereSearchParams params{
      normalized, InputLocale(), {} /* timeout */, false /* isCategory */,
      [this, timestamp](search::Results results, std::vector<search::ProductInfo>)
  { OnResults(timestamp, std::move(results)); }};
  if (m_framework.GetSearchAPI().SearchEverywhere(std::move(params)))
    SetBusy(true);
}

void SearchModel::OnResults(uint64_t timestamp, search::Results && results)
{
  // Delivered on the GUI thread, possibly several times per query as results improve.
  if (timestamp != m_timestamp)
    return;

  beginResetModel();
  *m_results = std::move(results);
  LoadHistory();
  endResetModel();

  m_framework.FillSearchResultsMarks(true /* clear */, *m_results);
  SetBusy(!m_query.trimmed().isEmpty() && !m_results->IsEndMarker());
}

void SearchModel::SetBusy(bool busy)
{
  if (busy == m_busy)
    return;
  m_busy = busy;
  emit busyChanged();
}

void SearchModel::LoadHistory()
{
  m_history.clear();
  for (auto const & request : m_framework.GetSearchAPI().GetLastSearchQueries())
    m_history << QString::fromStdString(request.second);
}

void SearchModel::clearHistory()
{
  m_framework.GetSearchAPI().ClearSearchHistory();
  beginResetModel();
  LoadHistory();
  endResetModel();
}

int SearchModel::rowCount(QModelIndex const & parent) const
{
  if (parent.isValid())
    return 0;
  return ShowingHistory() ? m_history.size() : static_cast<int>(m_results->GetCount());
}

QVariant SearchModel::data(QModelIndex const & index, int role) const
{
  if (!index.isValid() || index.row() >= rowCount())
    return {};

  // With an empty query the rows are recent searches; as suggestions they refill the query.
  if (ShowingHistory())
  {
    QString const & query = m_history[index.row()];
    switch (role)
    {
    case NameRole:
    case SuggestionRole: return query;
    default: return QString();
    }
  }

  auto const & result = (*m_results)[index.row()];
  switch (role)
  {
  case NameRole: return QString::fromStdString(result.GetString());
  case AddressRole: return QString::fromStdString(result.GetAddress());
  case TypeRole:
    if (result.GetResultType() == search::Result::Type::Feature)
      return QString::fromStdString(result.GetLocalizedFeatureType());
    return QString();
  case SuggestionRole:
    return result.IsSuggest() ? QString::fromStdString(result.GetSuggestionString()) : QString();
  default: return {};
  }
}

QHash<int, QByteArray> SearchModel::roleNames() const
{
  return {{NameRole, "name"}, {TypeRole, "type"}, {AddressRole, "address"}, {SuggestionRole, "suggestion"}};
}

void SearchModel::show(int row)
{
  if (row < 0 || row >= rowCount() || ShowingHistory())
    return;
  // Picking a result is what makes a query worth remembering.
  m_framework.GetSearchAPI().SaveSearchQuery({InputLocale(), m_query.trimmed().toStdString()});
  m_framework.ShowSearchResult((*m_results)[row]);
}

void SearchModel::pickOrigin(int row)
{
  if (!g_tripOrigin || row < 0 || row >= rowCount() || ShowingHistory())
    return;
  auto const & result = (*m_results)[row];
  if (!result.HasPoint())
    return;
  m2::PointD const center = result.GetFeatureCenter();
  g_tripOrigin->Set(QString::fromStdString(result.GetString()), center.x, center.y);
}

void SearchModel::clearMarks()
{
  m_framework.FillSearchResultsMarks(true /* clear */, search::Results());
}
}  // namespace sailfish
