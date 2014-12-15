/*
 * documentmanager.cpp
 * Copyright 2010, Stefan Beller <stefanbeller@googlemail.com>
 * Copyright 2010, Thorbjørn Lindeijer <thorbjorn@lindeijer.nl>
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

#include "documentmanager.h"

#include "abstracttool.h"
#include "filesystemwatcher.h"
#include "map.h"
#include "mapdocument.h"
#include "maprenderer.h"
#include "mapscene.h"
#include "mapview.h"
#include "movabletabwidget.h"
#include "pluginmanager.h"
#include "tmxmapreader.h"
#include "zoomable.h"

#include <QUndoGroup>
#include <QFileInfo>

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QDialogButtonBox>
#include <QScrollBar>

using namespace Tiled;
using namespace Tiled::Internal;

namespace Tiled {
namespace Internal {

class FileChangedWarning : public QWidget
{
    Q_OBJECT

public:
    FileChangedWarning(QWidget *parent = 0)
        : QWidget(parent)
        , mLabel(new QLabel(this))
        , mButtons(new QDialogButtonBox(QDialogButtonBox::Yes |
                                        QDialogButtonBox::No,
                                        Qt::Horizontal,
                                        this))
    {
        mLabel->setText(tr("File change detected. Discard changes and reload the map?"));

        QHBoxLayout *layout = new QHBoxLayout;
        layout->addWidget(mLabel);
        layout->addStretch(1);
        layout->addWidget(mButtons);
        setLayout(layout);

        connect(mButtons, SIGNAL(accepted()), SIGNAL(reload()));
        connect(mButtons, SIGNAL(rejected()), SIGNAL(ignore()));
    }

signals:
    void reload();
    void ignore();

private:
    QLabel *mLabel;
    QDialogButtonBox *mButtons;
};

class MapViewContainer : public QWidget
{
    Q_OBJECT

public:
    MapViewContainer(MapView *mapView, QWidget *parent = 0)
        : QWidget(parent)
        , mMapView(mapView)
        , mWarning(new FileChangedWarning)
    {
        mWarning->setVisible(false);

        QVBoxLayout *layout = new QVBoxLayout;
        layout->setMargin(0);
        layout->setSpacing(0);
        layout->addWidget(mapView);
        layout->addWidget(mWarning);
        setLayout(layout);

        connect(mWarning, SIGNAL(reload()), SIGNAL(reload()));
        connect(mWarning, SIGNAL(ignore()), mWarning, SLOT(hide()));
    }

    MapView *mapView() const { return mMapView; }

    void setFileChangedWarningVisible(bool visible)
    { mWarning->setVisible(visible); }

signals:
    void reload();

private:
    MapView *mMapView;
    FileChangedWarning *mWarning;
};

} // namespace Internal
} // namespace Tiled

DocumentManager *DocumentManager::mInstance = 0;

DocumentManager *DocumentManager::instance()
{
    if (!mInstance)
        mInstance = new DocumentManager;
    return mInstance;
}

void DocumentManager::deleteInstance()
{
    delete mInstance;
    mInstance = 0;
}

DocumentManager::DocumentManager(QObject *parent)
    : QObject(parent)
    , mTabWidget(new MovableTabWidget)
    , mUndoGroup(new QUndoGroup(this))
    , mSelectedTool(0)
    , mSceneWithTool(0)
    , mFileSystemWatcher(new FileSystemWatcher(this))
{
    mTabWidget->setDocumentMode(true);
    mTabWidget->setTabsClosable(true);

    connect(mTabWidget, SIGNAL(currentChanged(int)),
            SLOT(currentIndexChanged()));
    connect(mTabWidget, SIGNAL(tabCloseRequested(int)),
            SIGNAL(documentCloseRequested(int)));
    connect(mTabWidget, SIGNAL(tabMoved(int,int)),
            SLOT(documentTabMoved(int,int)));

    connect(mFileSystemWatcher, SIGNAL(fileChanged(QString)),
            SLOT(fileChanged(QString)));
}

DocumentManager::~DocumentManager()
{
    // All documents should be closed gracefully beforehand
    Q_ASSERT(mDocuments.isEmpty());
    delete mTabWidget;
}

QWidget *DocumentManager::widget() const
{
    return mTabWidget;
}

MapDocument *DocumentManager::currentDocument() const
{
    const int index = mTabWidget->currentIndex();
    if (index == -1)
        return 0;

    return mDocuments.at(index);
}

MapView *DocumentManager::currentMapView() const
{
    if (QWidget *widget = mTabWidget->currentWidget())
        return static_cast<MapViewContainer*>(widget)->mapView();

    return 0;
}

MapScene *DocumentManager::currentMapScene() const
{
    if (MapView *mapView = currentMapView())
        return mapView->mapScene();

    return 0;
}

MapView *DocumentManager::viewForDocument(MapDocument *mapDocument) const
{
    const int index = mDocuments.indexOf(mapDocument);
    if (index == -1)
        return 0;

    return static_cast<MapViewContainer*>(mTabWidget->widget(index))->mapView();
}

int DocumentManager::findDocument(const QString &fileName) const
{
    const QString canonicalFilePath = QFileInfo(fileName).canonicalFilePath();
    if (canonicalFilePath.isEmpty()) // file doesn't exist
        return -1;

    for (int i = 0; i < mDocuments.size(); ++i) {
        QFileInfo fileInfo(mDocuments.at(i)->fileName());
        if (fileInfo.canonicalFilePath() == canonicalFilePath)
            return i;
    }

    return -1;
}

void DocumentManager::switchToDocument(int index)
{
    mTabWidget->setCurrentIndex(index);
}

void DocumentManager::switchToDocument(MapDocument *mapDocument)
{
    const int index = mDocuments.indexOf(mapDocument);
    if (index != -1)
        switchToDocument(index);
}

void DocumentManager::switchToLeftDocument()
{
    const int tabCount = mTabWidget->count();
    if (tabCount < 2)
        return;

    const int currentIndex = mTabWidget->currentIndex();
    switchToDocument((currentIndex > 0 ? currentIndex : tabCount) - 1);
}

void DocumentManager::switchToRightDocument()
{
    const int tabCount = mTabWidget->count();
    if (tabCount < 2)
        return;

    const int currentIndex = mTabWidget->currentIndex();
    switchToDocument((currentIndex + 1) % tabCount);
}

void DocumentManager::addDocument(MapDocument *mapDocument)
{
    Q_ASSERT(mapDocument);
    Q_ASSERT(!mDocuments.contains(mapDocument));

    mDocuments.append(mapDocument);
    mUndoGroup->addStack(mapDocument->undoStack());

    if (!mapDocument->fileName().isEmpty())
        mFileSystemWatcher->addPath(mapDocument->fileName());

    MapView *view = new MapView;
    MapScene *scene = new MapScene(view); // scene is owned by the view
    MapViewContainer *container = new MapViewContainer(view, mTabWidget);

    scene->setMapDocument(mapDocument);
    view->setScene(scene);

    const int documentIndex = mDocuments.size() - 1;

    mTabWidget->addTab(container, mapDocument->displayName());
    mTabWidget->setTabToolTip(documentIndex, mapDocument->fileName());
    connect(mapDocument, SIGNAL(fileNameChanged(QString,QString)),
            SLOT(fileNameChanged(QString,QString)));
    connect(mapDocument, SIGNAL(modifiedChanged()), SLOT(updateDocumentTab()));
    connect(mapDocument, SIGNAL(saved()), SLOT(documentSaved()));

    connect(container, SIGNAL(reload()), SLOT(reloadRequested()));

    switchToDocument(documentIndex);
    centerViewOn(0, 0);
}

void DocumentManager::closeCurrentDocument()
{
    const int index = mTabWidget->currentIndex();
    if (index == -1)
        return;

    closeDocumentAt(index);
}

void DocumentManager::closeDocumentAt(int index)
{
    MapDocument *mapDocument = mDocuments.at(index);
    emit documentAboutToClose(mapDocument);

    QWidget *mapViewContainer = mTabWidget->widget(index);
    mDocuments.removeAt(index);
    mTabWidget->removeTab(index);
    delete mapViewContainer;

    if (!mapDocument->fileName().isEmpty())
        mFileSystemWatcher->removePath(mapDocument->fileName());

    delete mapDocument;
}

bool DocumentManager::reloadCurrentDocument()
{
    const int index = mTabWidget->currentIndex();
    if (index == -1)
        return false;

    return reloadDocumentAt(index);
}

bool DocumentManager::reloadDocumentAt(int index)
{
    MapDocument *oldDocument = mDocuments.at(index);

    // Try to find the interface that was used for reading this map
    QString readerPluginName = oldDocument->readerPluginFileName();
    MapReaderInterface *reader = 0;
    if (!readerPluginName.isEmpty()) {
        PluginManager *pm = PluginManager::instance();
        if (const Plugin *plugin = pm->pluginByFileName(readerPluginName))
            reader = qobject_cast<MapReaderInterface*>(plugin->instance);
    }

    QString error;
    MapDocument *newDocument = MapDocument::load(oldDocument->fileName(),
                                                 reader, &error);
    if (!newDocument) {
        emit reloadError(tr("%1:\n\n%2").arg(oldDocument->fileName(), error));
        return false;
    }

    // Remember current view state
    MapView *mapView = viewForDocument(oldDocument);
    const int layerIndex = oldDocument->currentLayerIndex();
    const qreal scale = mapView->zoomable()->scale();
    const int horizontalPosition = mapView->horizontalScrollBar()->sliderPosition();
    const int verticalPosition = mapView->verticalScrollBar()->sliderPosition();

    // Replace old tab
    addDocument(newDocument);
    closeDocumentAt(index);
    mTabWidget->moveTab(mDocuments.size() - 1, index);

    // Restore previous view state
    mapView = currentMapView();
    mapView->zoomable()->setScale(scale);
    mapView->horizontalScrollBar()->setSliderPosition(horizontalPosition);
    mapView->verticalScrollBar()->setSliderPosition(verticalPosition);
    if (layerIndex > 0 && layerIndex < newDocument->map()->layerCount())
        newDocument->setCurrentLayerIndex(layerIndex);

    return true;
}

void DocumentManager::closeAllDocuments()
{
    while (!mDocuments.isEmpty())
        closeCurrentDocument();
}

void DocumentManager::currentIndexChanged()
{
    if (mSceneWithTool) {
        mSceneWithTool->disableSelectedTool();
        mSceneWithTool = 0;
    }

    MapDocument *mapDocument = currentDocument();

    if (mapDocument)
        mUndoGroup->setActiveStack(mapDocument->undoStack());

    emit currentDocumentChanged(mapDocument);

    if (MapScene *mapScene = currentMapScene()) {
        mapScene->setSelectedTool(mSelectedTool);
        mapScene->enableSelectedTool();
        mSceneWithTool = mapScene;
    }
}

void DocumentManager::setSelectedTool(AbstractTool *tool)
{
    if (mSelectedTool == tool)
        return;

    mSelectedTool = tool;

    if (mSceneWithTool) {
        mSceneWithTool->disableSelectedTool();

        if (tool) {
            mSceneWithTool->setSelectedTool(tool);
            mSceneWithTool->enableSelectedTool();
        }
    }
}

void DocumentManager::fileNameChanged(const QString &fileName,
                                      const QString &oldFileName)
{
    if (!fileName.isEmpty())
        mFileSystemWatcher->addPath(fileName);
    if (!oldFileName.isEmpty())
        mFileSystemWatcher->removePath(oldFileName);

    updateDocumentTab();
}

void DocumentManager::updateDocumentTab()
{
    MapDocument *mapDocument = static_cast<MapDocument*>(sender());
    const int index = mDocuments.indexOf(mapDocument);

    QString tabText = mapDocument->displayName();
    if (mapDocument->isModified())
        tabText.prepend(QLatin1Char('*'));

    mTabWidget->setTabText(index, tabText);
    mTabWidget->setTabToolTip(index, mapDocument->fileName());
}

void DocumentManager::documentSaved()
{
    MapDocument *document = static_cast<MapDocument*>(sender());
    const int index = mDocuments.indexOf(document);
    Q_ASSERT(index != -1);

    QWidget *widget = mTabWidget->widget(index);
    MapViewContainer *container = static_cast<MapViewContainer*>(widget);
    container->setFileChangedWarningVisible(false);
}

void DocumentManager::documentTabMoved(int from, int to)
{
    mDocuments.move(from, to);
}

void DocumentManager::fileChanged(const QString &fileName)
{
    const int index = findDocument(fileName);

    // Most likely the file was removed
    if (index == -1)
        return;

    MapDocument *document = mDocuments.at(index);

    // Ignore change event when it seems to be our own save
    if (QFileInfo(fileName).lastModified() == document->lastSaved())
        return;

    // Automatically reload when there are no unsaved changes
    if (!document->isModified()) {
        reloadDocumentAt(index);
        return;
    }

    QWidget *widget = mTabWidget->widget(index);
    MapViewContainer *container = static_cast<MapViewContainer*>(widget);
    container->setFileChangedWarningVisible(true);
}

void DocumentManager::reloadRequested()
{
    int index = mTabWidget->indexOf(static_cast<MapViewContainer*>(sender()));
    Q_ASSERT(index != -1);
    reloadDocumentAt(index);
}

void DocumentManager::centerViewOn(qreal x, qreal y)
{
    MapView *view = currentMapView();
    if (!view)
        return;

    view->centerOn(currentDocument()->renderer()->pixelToScreenCoords(x, y));
}

#include "documentmanager.moc"
