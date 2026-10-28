#include "gui/models/paginationmodel.h"
#include <QAbstractItemModel>
#include <algorithm>

PaginationModel::PaginationModel(int maxRows, QObject* parent)
    : QSortFilterProxyModel(parent),
      _maxRows(std::max(1, maxRows)),
      _currentPage(-1),
      _firstRow(0),
      _rangeEnd(0),
      _prePageCount(0),
      _preCurrentPage(-1),
      _preCanGoBack(false),
      _preCanGoForward(false)
{}

void PaginationModel::_emitCanChange(bool oldBack, bool oldFwd)
{
    bool newBack = canGoBack();
    if (oldBack != newBack) emit canGoBackChanged(newBack);

    bool newFwd = canGoForward();
    if (oldFwd != newFwd) emit canGoForwardChanged(newFwd);
}

void PaginationModel::_beginSourceChange()
{
    _prePageCount = pageCount();
    _preCurrentPage = _currentPage;
    _preCanGoBack = canGoBack();
    _preCanGoForward = canGoForward();
}

void PaginationModel::_endSourceChange()
{
    int newRowCount = sourceModel() ? sourceModel()->rowCount() : 0;
    int oldPageCount = _prePageCount;
    int oldPage = _preCurrentPage;
    bool oldBack = _preCanGoBack;
    bool oldFwd = _preCanGoForward;

    const int newPageCount = pageCount();
    _currentPage = newPageCount ? std::clamp(_currentPage, 0, newPageCount - 1) : -1;
    _firstRow = newPageCount ? _currentPage * _maxRows : 0;
    _rangeEnd = _firstRow + std::min(_maxRows, newRowCount - _firstRow);
    invalidateFilter();

    if (oldPageCount != newPageCount)
        emit pageCountChanged(newPageCount);

    if (oldPage != _currentPage)
        emit currentPageChanged(_currentPage);

    _emitCanChange(oldBack, oldFwd);

    // Qt clears sourceModel before our destroyed connection runs. Retain the
    // latest observable state so that destruction can notify the empty state.
    _beginSourceChange();
}

void PaginationModel::_setCurrentPage(int page)
{
    if (!sourceModel()) return;
    _beginSourceChange();
    _currentPage = page;
    _endSourceChange();
}

int PaginationModel::maxRows() const
{
    return _maxRows;
}

void PaginationModel::setMaxRows(int maxRows)
{
    if (maxRows <= 0 || _maxRows == maxRows) return;
    if (!sourceModel()) {
        _maxRows = maxRows;
        return;
    }

    _beginSourceChange();
    _maxRows = maxRows;
    _endSourceChange();
}

bool PaginationModel::canGoBack() const
{
    return _currentPage > 0;
}

bool PaginationModel::canGoForward() const
{
    return _currentPage < pageCount() - 1;
}

void PaginationModel::firstPage()
{
    setCurrentPage(0);
}

void PaginationModel::previousPage()
{
    setCurrentPage(_currentPage - 1);
}

void PaginationModel::nextPage()
{
    setCurrentPage(_currentPage + 1);
}

void PaginationModel::lastPage()
{
    if (sourceModel())
        setCurrentPage((sourceModel()->rowCount() - 1) / _maxRows);
}

int PaginationModel::pageCount() const
{
    if (sourceModel()) {
        int rows = sourceModel()->rowCount();
        return rows ? 1 + (rows - 1) / _maxRows : 0;
    }
    return 0;
}

int PaginationModel::currentPage() const
{
    return _currentPage;
}

void PaginationModel::setCurrentPage(int page)
{
    if (sourceModel() && page >= 0 && _currentPage != page)
        _setCurrentPage(page);
}

bool PaginationModel::filterAcceptsRow(int source_row, const QModelIndex&) const
{
    return (source_row >= _firstRow) && (source_row < _rangeEnd);
}

void PaginationModel::setSourceModel(QAbstractItemModel* model)
{
    QAbstractItemModel* old = sourceModel();
    if (old == model) return;

    _beginSourceChange();
    if (old) {
        disconnect(old, nullptr, this, nullptr);
    }

    QSortFilterProxyModel::setSourceModel(model);

    if (model) {
        connect(model, &QAbstractItemModel::rowsAboutToBeRemoved, this, &PaginationModel::_beginSourceChange);
        connect(model, &QAbstractItemModel::rowsAboutToBeInserted, this, &PaginationModel::_beginSourceChange);
        connect(model, &QAbstractItemModel::layoutAboutToBeChanged, this, &PaginationModel::_beginSourceChange);
        connect(model, &QAbstractItemModel::modelAboutToBeReset, this, &PaginationModel::_beginSourceChange);

        connect(model, &QAbstractItemModel::rowsRemoved, this, &PaginationModel::_endSourceChange);
        connect(model, &QAbstractItemModel::rowsInserted, this, &PaginationModel::_endSourceChange);
        connect(model, &QAbstractItemModel::layoutChanged, this, &PaginationModel::_endSourceChange);
        connect(model, &QAbstractItemModel::modelReset, this, &PaginationModel::_endSourceChange);
        connect(model, &QObject::destroyed, this, &PaginationModel::_endSourceChange);
    }
    _currentPage = -1;
    _endSourceChange();
}
