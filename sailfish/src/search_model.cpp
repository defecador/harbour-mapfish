#include "sailfish/src/search_model.hpp"

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

SearchModel::SearchModel(QObject * parent)
  : QAbstractListModel(parent)
  , m_framework(*g_searchFramework)
  , m_results(std::make_unique<search::Results>())
{}

SearchModel::~SearchModel()
{
  m_framework.GetSearchAPI().CancelAllSearches();
}

void SearchModel::SetQuery(QString const & query)
{
  if (query == m_query)
    return;
  m_query = query;
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

int SearchModel::rowCount(QModelIndex const & parent) const
{
  return parent.isValid() ? 0 : static_cast<int>(m_results->GetCount());
}

QVariant SearchModel::data(QModelIndex const & index, int role) const
{
  if (!index.isValid() || index.row() >= rowCount())
    return {};

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
  if (row < 0 || row >= rowCount())
    return;
  m_framework.ShowSearchResult((*m_results)[row]);
}

void SearchModel::clearMarks()
{
  m_framework.FillSearchResultsMarks(true /* clear */, search::Results());
}
}  // namespace sailfish
