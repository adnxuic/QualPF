#ifndef TEXTFILTERPROXYMODEL_H
#define TEXTFILTERPROXYMODEL_H

#include <QSortFilterProxyModel>

class TextFilterProxyModel : public QSortFilterProxyModel
{
    Q_OBJECT
public:
    explicit TextFilterProxyModel(QObject* parent = nullptr);
    void setMinimumScore(double minimum) { minimum_ = minimum; invalidateFilter(); }

protected:
    bool filterAcceptsRow(int source_row, const QModelIndex& source_parent) const override;
    double minimum_ = -1;
};

#endif // TEXTFILTERPROXYMODEL_H
