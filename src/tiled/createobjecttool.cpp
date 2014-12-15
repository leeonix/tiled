/*
 * createobjecttool.cpp
 * Copyright 2010-2011, Thorbjørn Lindeijer <thorbjorn@lindeijer.nl>
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

#include "createobjecttool.h"

#include "addremovemapobject.h"
#include "map.h"
#include "mapdocument.h"
#include "mapobject.h"
#include "mapobjectitem.h"
#include "maprenderer.h"
#include "mapscene.h"
#include "objectgroup.h"
#include "objectgroupitem.h"
#include "preferences.h"
#include "tile.h"
#include "utils.h"

#include <QApplication>
#include <QKeyEvent>
#include <QPalette>

using namespace Tiled;
using namespace Tiled::Internal;

CreateObjectTool::CreateObjectTool(CreationMode mode, QObject *parent)
    : AbstractObjectTool(QString(),
                         QIcon(QLatin1String(":images/24x24/insert-rectangle.png")),
                         QKeySequence(tr("O")),
                         parent)
    , mNewMapObjectItem(0)
    , mOverlayObjectGroup(0)
    , mOverlayPolygonObject(0)
    , mOverlayPolygonItem(0)
    , mTile(0)
    , mMode(mode)
{
}

CreateObjectTool::~CreateObjectTool()
{
    delete mOverlayObjectGroup;
}

void CreateObjectTool::deactivate(MapScene *scene)
{
    if (mNewMapObjectItem)
        cancelNewMapObject();

    AbstractObjectTool::deactivate(scene);
}

void CreateObjectTool::keyPressed(QKeyEvent *event)
{
    switch (event->key()) {
    case Qt::Key_Enter:
    case Qt::Key_Return:
        if (mNewMapObjectItem) {
                finishNewMapObject();
            return;
        }
        break;
    case Qt::Key_Escape:
        if (mNewMapObjectItem) {
            cancelNewMapObject();
            return;
        }
        break;
    }

    AbstractObjectTool::keyPressed(event);
}

void CreateObjectTool::mouseEntered()
{
}

void CreateObjectTool::mouseMoved(const QPointF &pos,
                                  Qt::KeyboardModifiers modifiers)
{
    AbstractObjectTool::mouseMoved(pos, modifiers);

    if (mNewMapObjectItem){
        bool snapToGrid = Preferences::instance()->snapToGrid();
        bool snapToFineGrid = Preferences::instance()->snapToFineGrid();
        if (modifiers & Qt::ControlModifier) {
            snapToGrid = !snapToGrid;
            snapToFineGrid = false;
        }

        mouseMovedWhileCreatingObject(pos, modifiers, snapToGrid, snapToFineGrid);
    }
}

void CreateObjectTool::mousePressed(QGraphicsSceneMouseEvent *event)
{
    bool snapToGrid = Preferences::instance()->snapToGrid();
    bool snapToFineGrid = Preferences::instance()->snapToFineGrid();
    if (event->modifiers() & Qt::ControlModifier) {
        snapToGrid = !snapToGrid;
        snapToFineGrid = false;
    }

    if(mNewMapObjectItem){
        mousePressedWhileCreatingObject(event, snapToGrid, snapToFineGrid);
        return;
    }

    if (event->button() != Qt::LeftButton) {
        AbstractObjectTool::mousePressed(event);
        return;
    }

    ObjectGroup *objectGroup = currentObjectGroup();
    if (!objectGroup || !objectGroup->isVisible())
        return;

    const MapRenderer *renderer = mapDocument()->renderer();
    QPointF tileCoords;

    /*TODO: calculate the tile offset with a polymorphic behaviour object
     * that is instantiated by the correspondend ObjectTool
    */
    if (mMode == CreateTile) {
        if (!mTile)
            return;

        const QPointF diff(-mTile->width() / 2, mTile->height() / 2);
        tileCoords = renderer->screenToTileCoords(event->scenePos() + diff);
    } else {
        tileCoords = renderer->screenToTileCoords(event->scenePos());
    }

        if (snapToFineGrid) {
        int gridFine = Preferences::instance()->gridFine();
        tileCoords = (tileCoords * gridFine).toPoint();
        tileCoords /= gridFine;
    } else if (snapToGrid)
        tileCoords = tileCoords.toPoint();
    
    const QPointF pixelCoords = renderer->tileToPixelCoords(tileCoords);

    startNewMapObject(pixelCoords, objectGroup);
}

void CreateObjectTool::mouseReleased(QGraphicsSceneMouseEvent *event)
{
    bool snapToGrid = Preferences::instance()->snapToGrid();
    bool snapToFineGrid = Preferences::instance()->snapToFineGrid();
    if (event->modifiers() & Qt::ControlModifier) {
        snapToGrid = !snapToGrid;
        snapToFineGrid = false;
    }
    if (mNewMapObjectItem){
        mouseReleasedWhileCreatingObject(event, snapToGrid, snapToFineGrid);
    }
}

void CreateObjectTool::startNewMapObject(const QPointF &pos,
                                         ObjectGroup *objectGroup)
{
    Q_ASSERT(!mNewMapObjectItem);

    MapObject *newMapObject = createNewMapObject();
    if (newMapObject == 0)
        return;
    newMapObject->setPosition(pos);

    objectGroup->addObject(newMapObject);

    mNewMapObjectItem = new MapObjectItem(newMapObject, mapDocument());
    mNewMapObjectItem->setZValue(10000); // same as the BrushItem
    mapScene()->addItem(mNewMapObjectItem);
}

MapObject *CreateObjectTool::clearNewMapObjectItem()
{
    Q_ASSERT(mNewMapObjectItem);

    MapObject *newMapObject = mNewMapObjectItem->mapObject();

    ObjectGroup *objectGroup = newMapObject->objectGroup();
    objectGroup->removeObject(newMapObject);

    delete mNewMapObjectItem;
    mNewMapObjectItem = 0;

    delete mOverlayPolygonItem;
    mOverlayPolygonItem = 0;

    return newMapObject;
}

void CreateObjectTool::cancelNewMapObject()
{
    MapObject *newMapObject = clearNewMapObjectItem();
    delete newMapObject;
}

void CreateObjectTool::finishNewMapObject()
{
    Q_ASSERT(mNewMapObjectItem);
    MapObject *newMapObject = mNewMapObjectItem->mapObject();
    ObjectGroup *objectGroup = newMapObject->objectGroup();
    clearNewMapObjectItem();

    mapDocument()->undoStack()->push(new AddMapObject(mapDocument(),
                                                      objectGroup,
                                                      newMapObject));
}

void CreateObjectTool::mouseMovedWhileCreatingObject(const QPointF &, Qt::KeyboardModifiers, bool, bool)
{
   //optional override
}

void CreateObjectTool::mousePressedWhileCreatingObject(QGraphicsSceneMouseEvent *, bool, bool)
{
    //optional override
}

void CreateObjectTool::mouseReleasedWhileCreatingObject(QGraphicsSceneMouseEvent *, bool, bool)
{
    //optional override
}
