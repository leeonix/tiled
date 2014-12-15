/*
 * tilelayeritem.h
 * Copyright 2008-2009, Thorbjørn Lindeijer <thorbjorn@lindeijer.nl>
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

#ifndef TILELAYERITEM_H
#define TILELAYERITEM_H

#include <QGraphicsItem>

namespace Tiled {

class TileLayer;

namespace Internal {

class MapDocument;

/**
 * A graphics item displaying a tile layer in a QGraphicsView.
 */
class TileLayerItem : public QGraphicsItem
{
public:
    /**
     * Constructor.
     *
     * @param layer       the tile layer to be displayed
     * @param mapDocument the map document owning the map of this layer
     */
    TileLayerItem(TileLayer *layer, MapDocument *mapDocument);

    /**
     * Updates the size and position of this item. Should be called when the
     * size of either the tile layer or its associated map have changed.
     *
     * Calling this function when the size of the map changes is necessary
     * because in certain map orientations this affects the layer position
     * (when using the IsometricRenderer for example).
     */
    void syncWithTileLayer();

    // QGraphicsItem
    QRectF boundingRect() const;
    void paint(QPainter *painter,
               const QStyleOptionGraphicsItem *option,
               QWidget *widget = 0);

private:
    TileLayer *mLayer;
    MapDocument *mMapDocument;
    QRectF mBoundingRect;
};

} // namespace Internal
} // namespace Tiled

#endif // TILELAYERITEM_H
