/*
  processfiltermodel.cpp

  This file is part of Endoscope, the Qt application inspection and
  manipulation tool.

  Copyright (C) 2010-2011 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com
  Author: Milian Wolff <milian.wolff@kdab.com>

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "processfiltermodel.h"
#include "processmodel.h"

#include <QCoreApplication>

#ifdef Q_WS_WIN
QString getUser()
{
  ///FIXME: implement this properly
  return QString();
}
#else
#include <stdio.h>

QString getUser()
{
  return QString::fromLocal8Bit(cuserid(0));
}
#endif

using namespace Endoscope;

ProcessFilterModel::ProcessFilterModel(QObject *parent)
  : QSortFilterProxyModel(parent)
{
  setFilterCaseSensitivity(Qt::CaseInsensitive);
  setFilterKeyColumn(1);

  m_currentProcId = QString::number(qApp->applicationPid());
  m_currentUser = getUser();
#ifndef Q_WS_WIN
  if (m_currentUser == QLatin1String("root")) {
    // empty current user == no filter. as root we want to show all
    m_currentUser.clear();
  }
#endif
}

bool ProcessFilterModel::lessThan(const QModelIndex &left, const QModelIndex &right) const
{
  const QString l = sourceModel()->data(left).toString();
  const QString r = sourceModel()->data(right).toString();
  if (left.column() == ProcessModel::PIDColumn)
      return l.toInt() < r.toInt();

  return l.compare(r, Qt::CaseInsensitive) <= 0;
}

bool ProcessFilterModel::filterAcceptsRow(int source_row, const QModelIndex &source_parent) const
{
  if (!m_currentUser.isEmpty()) {
    if (ProcessModel* source = dynamic_cast<ProcessModel*>(sourceModel())) {
      return source->dataForRow(source_row).user == m_currentUser;
    }
  }
  return QSortFilterProxyModel::filterAcceptsRow(source_row, source_parent);
}

bool ProcessFilterModel::filterAcceptsColumn(int source_column,
                                             const QModelIndex &/*source_parent*/) const
{
  // hide user row if current user was found
  return m_currentUser.isEmpty() || source_column != ProcessModel::UserColumn;
}

Qt::ItemFlags ProcessFilterModel::flags(const QModelIndex &index) const
{
  Qt::ItemFlags flags = QSortFilterProxyModel::flags(index);

  if (index.data(ProcessModel::PIDRole).toString() == m_currentProcId) {
    return (flags & (~Qt::ItemIsEnabled));
  }

  return flags;
}