#include "tools/cabana/historylog.h"

#include <QFontDatabase>
#include <QPainter>

// HistoryLogModel

inline const Signal &get_signal(const DBCMsg *m, int index) {
  return std::next(m->sigs.begin(), index)->second;
}

QVariant HistoryLogModel::data(const QModelIndex &index, int role) const {
  bool has_signal = dbc_msg && !dbc_msg->sigs.empty();
  if (role == Qt::DisplayRole) {
    const auto &m = messages[index.row()];
    if (index.column() == 0) {
      return QString::number(m.ts, 'f', 2);
    }
    return has_signal ? QString::number(get_raw_value((uint8_t *)m.dat.begin(), m.dat.size(), get_signal(dbc_msg, index.column() - 1)))
                      : toHex(m.dat);
  } else if (role == Qt::FontRole && index.column() == 1 && !has_signal) {
    return QFontDatabase::systemFont(QFontDatabase::FixedFont);
  }
  return {};
}

void HistoryLogModel::setMessage(const QString &message_id) {
  beginResetModel();
  msg_id = message_id;
  dbc_msg = dbc()->msg(message_id);
  column_count = (dbc_msg && !dbc_msg->sigs.empty() ? dbc_msg->sigs.size() : 1) + 1;
  row_count = 0;
  endResetModel();

  updateState();
}

QVariant HistoryLogModel::headerData(int section, Qt::Orientation orientation, int role) const {
  if (orientation == Qt::Horizontal) {
    bool has_signal = dbc_msg && !dbc_msg->sigs.empty();
    if (role == Qt::DisplayRole || role == Qt::ToolTipRole) {
      if (section == 0) {
        return "Time";
      }
      return has_signal ? QString::fromStdString(get_signal(dbc_msg, section - 1).name).replace('_', ' ') : "Data";
    } else if (role == Qt::BackgroundRole && section > 0 && has_signal) {
      return QBrush(QColor(getColor(section - 1)));
    } else if (role == Qt::ForegroundRole && section > 0 && has_signal) {
      return QBrush(Qt::black);
    }
  }
  return {};
}

void HistoryLogModel::updateState() {
  if (msg_id.isEmpty()) return;

  int prev_row_count = row_count;
  messages = can->messages(msg_id);
  row_count = messages.size();
  int delta = row_count - prev_row_count;
  if (delta > 0) {
    beginInsertRows({}, prev_row_count, row_count - 1);
    endInsertRows();
  } else if (delta < 0) {
    beginRemoveRows({}, row_count, prev_row_count - 1);
    endRemoveRows();
  }
  if (row_count > 0) {
    emit dataChanged(index(0, 0), index(row_count - 1, column_count - 1), {Qt::DisplayRole});
  }
}

// HeaderView

QSize HeaderView::sectionSizeFromContents(int logicalIndex) const {
  const QString text = model()->headerData(logicalIndex, this->orientation(), Qt::DisplayRole).toString();
  const QRect rect = fontMetrics().boundingRect(QRect(0, 0, sectionSize(logicalIndex), 1000), defaultAlignment(), text);
  return rect.size() + QSize{10, 6};
}

void HeaderView::paintSection(QPainter *painter, const QRect &rect, int logicalIndex) const {
  auto bg_role = model()->headerData(logicalIndex, Qt::Horizontal, Qt::BackgroundRole);
  if (bg_role.isValid()) {
    QPen pen(model()->headerData(logicalIndex, Qt::Horizontal, Qt::ForegroundRole).value<QBrush>(), 1);
    painter->setPen(pen);
    painter->fillRect(rect, bg_role.value<QBrush>());
  }
  QString text = model()->headerData(logicalIndex, Qt::Horizontal, Qt::DisplayRole).toString();
  painter->drawText(rect.adjusted(5, 3, 5, 3), defaultAlignment(), text);
}

// HistoryLog

HistoryLog::HistoryLog(QWidget *parent) : QTableView(parent) {
  model = new HistoryLogModel(this);
  setModel(model);
  setHorizontalHeader(new HeaderView(Qt::Horizontal, this));
  horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
  horizontalHeader()->setDefaultAlignment(Qt::AlignLeft | (Qt::Alignment)Qt::TextWordWrap);
  horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
  verticalHeader()->setVisible(false);
  setFrameShape(QFrame::NoFrame);
  setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
}

int HistoryLog::sizeHintForColumn(int column) const {
  // sizeHintForColumn is only called for column 0 (ResizeToContents)
  return itemDelegate()->sizeHint(viewOptions(), model->index(0, 0)).width() + 1; // +1 for grid
}
