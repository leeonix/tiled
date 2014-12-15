/*
 * movabletabwidget.h
 * Copyright 2014, Sean Humeniuk <seanhumeniuk@gmail.com>
 * Copyright 2014, Thorbjørn Lindeijer <thorbjorn@lindeijer.nl>
 *
 * This file is part of Tiled.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef MOVABLE_TAB_WIDGET_H
#define MOVABLE_TAB_WIDGET_H

#include <QTabWidget>

namespace Tiled {
namespace Internal {

/**
 * A QTabWidget that has movable tabs by default and forwards its QTabBar's
 * tabMoved signal.
 */
class MovableTabWidget : public QTabWidget
{
    Q_OBJECT

public:
    /**
     * Constructor
     */
    explicit MovableTabWidget(QWidget *parent = 0);

    void moveTab(int from, int to);

signals:
    /**
     * Emitted when a tab is moved from index position \a from to
     * index position \a to.
     */
    void tabMoved(int from, int to);
};

} // namespace Tiled::Internal
} // namespace Tiled

#endif // MOVABLE_TAB_WIDGET_H
