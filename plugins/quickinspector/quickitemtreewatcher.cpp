/*
  quickitemtreewatcher.cpp

  This file is part of GammaRay, the Qt application inspection and
  manipulation tool.

  Copyright (C) 2014 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com
  Author: Volker Krause <volker.krause@kdab.com>

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

#include "quickitemtreewatcher.h"
#include "quickitemmodelroles.h"

#include <QAbstractItemModel>
#include <QTreeView>

using namespace GammaRay;

QuickItemTreeWatcher::QuickItemTreeWatcher(QTreeView* itemView, QTreeView* sgView, QObject *parent)
  : QObject(parent),
    m_itemView(itemView),
    m_sgView(sgView)
{
  connect(itemView->model(), SIGNAL(dataChanged(QModelIndex,QModelIndex)),
          this, SLOT(itemModelDataChanged(QModelIndex, QModelIndex)));
  connect(sgView->model(), SIGNAL(dataChanged(QModelIndex,QModelIndex)),
          this, SLOT(sgModelDataChanged(QModelIndex,QModelIndex)));
}

QuickItemTreeWatcher::~QuickItemTreeWatcher()
{
}

void QuickItemTreeWatcher::itemModelDataChanged(const QModelIndex& topLeft, const QModelIndex& bottomRight)
{
  for (int row = topLeft.row(); row <= bottomRight.row(); ++row) {
    const QModelIndex index = topLeft.sibling(row, 0);
    const bool invisible = index.data(QuickItemModelRole::ItemFlags).value<int>() & (QuickItemModelRole::Invisible | QuickItemModelRole::ZeroSize);
    if (!invisible) {
      m_itemView->setExpanded(index, true);
    }
  }
}

void QuickItemTreeWatcher::sgModelDataChanged(const QModelIndex& topLeft, const QModelIndex& bottomRight)
{
  for (int row = topLeft.row(); row <= bottomRight.row(); ++row) {
    const QModelIndex index = topLeft.sibling(row, 0);
    m_sgView->setExpanded(index, true);
  }
  m_sgView->resizeColumnToContents(0);
}
