/*
  quickinspectorwidget.cpp

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

#include "quickinspectorwidget.h"
#include "quickinspectorclient.h"
#include "quickclientitemmodel.h"
#include "quickitemtreewatcher.h"
#include "geometryextension/sggeometryextensionclient.h"
#include "geometryextension/sggeometrytab.h"
#include "materialextension/materialextensionclient.h"
#include "materialextension/materialtab.h"
#include "quickpreviewscene.h"
#include "quickitemoverlay.h"
#include "quickitemdelegatewidget.h"
#include "ui_quickinspectorwidget.h"

#include <common/objectbroker.h>
#include <ui/deferredresizemodesetter.h>

#include <QLabel>
#include <QTimer>
#include <qmath.h>
#include <QQuickImageProvider>
#include <QRectF>
#include <QQuickView>
#include <QQuickItem>
#include <QPainter>
#include <QQmlContext>
#include <QtCore/qglobal.h>
#include <QPropertyAnimation>

namespace GammaRay {
class QuickSceneImageProvider : public QQuickImageProvider
{
  public:
    explicit QuickSceneImageProvider() : QQuickImageProvider(QQuickImageProvider::Pixmap) {}
    ~QuickSceneImageProvider() {}

    QPixmap requestPixmap(const QString & id, QSize * size, const QSize & requestedSize)
    {
      Q_UNUSED(requestedSize);
      if (id == "background") {
        QPixmap bgPattern(20, 20);
        bgPattern.fill(Qt::lightGray);
        QPainter bgPainter(&bgPattern);
        bgPainter.fillRect(10, 0, 10, 10, Qt::gray);
        bgPainter.fillRect(0, 10, 10, 10, Qt::gray);
        *size = QSize(20, 20);
        return bgPattern;
      }
      *size = m_pixmap.size();
      return m_pixmap;
    }

    void setPixmap(QPixmap pixmap)
    {
      m_pixmap = pixmap;
    }

  private:
    QPixmap m_pixmap;
};
}

using namespace GammaRay;

static QObject* createQuickInspectorClient(const QString &/*name*/, QObject *parent)
{
  return new QuickInspectorClient(parent);
}

static QObject* createMaterialExtension(const QString &name, QObject *parent)
{
  return new MaterialExtensionClient(name, parent);
}
static QObject* createSGGeometryExtension(const QString &name, QObject *parent)
{
  return new SGGeometryExtensionClient(name, parent);
}

QuickInspectorWidget::QuickInspectorWidget(QWidget* parent) :
  QWidget(parent),
  ui(new Ui::QuickInspectorWidget),
  m_renderTimer(new QTimer(this)),
  m_sceneChangedSinceLastRequest(false),
  m_waitingForImage(false),
  m_imageProvider(new QuickSceneImageProvider),
  m_preview(new QQuickView)
{
  ui->setupUi(this);


  ObjectBroker::registerClientObjectFactoryCallback<QuickInspectorInterface*>(createQuickInspectorClient);
  m_interface = ObjectBroker::object<QuickInspectorInterface*>();
  connect(m_interface, SIGNAL(sceneChanged()), this, SLOT(sceneChanged()));
  connect(m_interface, SIGNAL(sceneRendered(QImage, QVariantMap)), this, SLOT(sceneRendered(QImage, QVariantMap)));

  ui->windowComboBox->setModel(ObjectBroker::model("com.kdab.GammaRay.QuickWindowModel"));
  connect(ui->windowComboBox, SIGNAL(currentIndexChanged(int)), m_interface, SLOT(selectWindow(int)));
  if (ui->windowComboBox->currentIndex() >= 0)
    m_interface->selectWindow(ui->windowComboBox->currentIndex());

  QSortFilterProxyModel *proxy = new QuickClientItemModel(this);
  proxy->setSourceModel(ObjectBroker::model("com.kdab.GammaRay.QuickItemModel"));
  proxy->setDynamicSortFilter(true);
  ui->itemTreeView->setModel(proxy);
  ui->itemTreeSearchLine->setProxy(proxy);
  QItemSelectionModel* selectionModel = ObjectBroker::selectionModel(proxy);
  ui->itemTreeView->setSelectionModel(selectionModel);
  connect(selectionModel, SIGNAL(selectionChanged(QItemSelection,QItemSelection)),
          this, SLOT(itemSelectionChanged(QItemSelection)));
  connect(proxy, SIGNAL(dataChanged(QModelIndex,QModelIndex,QVector<int>)), this, SLOT(itemModelDataChanged(QModelIndex,QModelIndex)));

  QSortFilterProxyModel *sgProxy = new QSortFilterProxyModel(this);
  sgProxy->setSourceModel(ObjectBroker::model("com.kdab.GammaRay.QuickSceneGraphModel"));
  sgProxy->setDynamicSortFilter(true);
  ui->sgTreeView->setModel(sgProxy);
  ui->sgTreeSearchLine->setProxy(sgProxy);
  QItemSelectionModel* sgSelectionModel = ObjectBroker::selectionModel(sgProxy);
  ui->sgTreeView->setSelectionModel(sgSelectionModel);
  connect(sgSelectionModel, SIGNAL(selectionChanged(QItemSelection,QItemSelection)),
          this, SLOT(itemSelectionChanged(QItemSelection)));

  new QuickItemTreeWatcher(ui->itemTreeView, ui->sgTreeView, this);
  new DeferredResizeModeSetter(ui->itemTreeView->header(), 0, QHeaderView::ResizeToContents);

  ui->itemPropertyWidget->setObjectBaseName("com.kdab.GammaRay.QuickItem");
  ui->sgPropertyWidget->setObjectBaseName("com.kdab.GammaRay.QuickSceneGraph");

  qmlRegisterType<QuickItemOverlay>("com.kdab.GammaRay", 1, 0, "QuickItemOverlay");
  qmlRegisterType<QuickItemOverlay>();

  QWidget *previewWidget = QWidget::createWindowContainer(m_preview, ui->previewTreeSplitter);
  m_preview->show();
  m_preview->setResizeMode(QQuickView::SizeRootObjectToView);
  m_preview->engine()->addImageProvider("quicksceneprovider", m_imageProvider);
  m_preview->setSource(QUrl("qrc:/gammaray/plugins/quickinspector/quickpreview.qml"));
  m_rootItem = qobject_cast< QQuickItem* >(m_preview->rootObject());
  m_preview->engine()->rootContext()->setContextProperty("inspectorInterface", m_interface);
  QTimer::singleShot(0, this, SLOT(setSplitterSizes()));

  m_renderTimer->setInterval(100);
  m_renderTimer->setSingleShot(true);
  connect(m_renderTimer, SIGNAL(timeout()), this, SLOT(requestRender()));

  connect(m_interface, SIGNAL(features(GammaRay::QuickInspectorInterface::Features)), this, SLOT(setFeatures(GammaRay::QuickInspectorInterface::Features)));
  m_interface->checkFeatures();
  m_interface->renderScene();
}

QuickInspectorWidget::~QuickInspectorWidget()
{
}

void QuickInspectorWidget::setSplitterSizes()
{
  ui->previewTreeSplitter->setSizes(QList<int>()
                                    << (ui->previewTreeSplitter->height() - ui->previewTreeSplitter->handleWidth()) /2
                                    << (ui->previewTreeSplitter->height() - ui->previewTreeSplitter->handleWidth()) /2);
}

void QuickInspectorWidget::sceneChanged()
{
  if (!m_renderTimer->isActive())
    m_renderTimer->start();
}

void QuickInspectorWidget::sceneRendered(const QImage& img, const QVariantMap &geometryData)
{
  m_waitingForImage = false;

  m_imageProvider->setPixmap(QPixmap::fromImage(img));

  if (m_rootItem) {
    QMetaObject::invokeMethod(m_rootItem, "updatePreview");
    m_rootItem->setProperty("geometryData", geometryData);
  }

  if (m_sceneChangedSinceLastRequest) {
    m_sceneChangedSinceLastRequest = false;
    sceneChanged();
  }
}

void QuickInspectorWidget::requestRender()
{
  if (!m_waitingForImage) {
    m_waitingForImage = true;
    m_interface->renderScene();
  } else {
    m_sceneChangedSinceLastRequest = true;
  }
}

void QuickInspectorWidget::setFeatures(QuickInspectorInterface::Features features)
{
  if (!(features & QuickInspectorInterface::SGInspector)) {
    ui->tabWidget->removeTab(1);
  }

  m_rootItem->setProperty("canVisualizeOverdraw", (bool)(features & QuickInspectorInterface::VisualizeOverdraw));
}

void QuickInspectorWidget::itemSelectionChanged(const QItemSelection& selection)
{
  if (selection.isEmpty())
    return;
  const QModelIndex &index = selection.first().topLeft();
  ui->itemTreeView->scrollTo(index);
}

void QuickInspectorWidget::itemModelDataChanged(const QModelIndex& topLeft, const QModelIndex& bottomRight)
{
  for (int i = topLeft.row(); i <= bottomRight.row(); i++) {
    QModelIndex index = ui->itemTreeView->model()->index(i, 0, topLeft.parent());
    QuickItemDelegateWidget *widget = qobject_cast<QuickItemDelegateWidget*>(ui->itemTreeView->indexWidget(index));

    if (!widget) {
      widget = new QuickItemDelegateWidget(index, ui->itemTreeView);
      widget->setAutoFillBackground(true);
      ui->itemTreeView->setIndexWidget(index, widget);
    }

    QPropertyAnimation *colorAnimation = new QPropertyAnimation(widget);
    colorAnimation->setTargetObject(widget);
    colorAnimation->setPropertyName("textColor");
    colorAnimation->setStartValue(QColor(255, 0, 0));
    QVariant endValue = index.data(Qt::ForegroundRole);
    if (!endValue.isValid())
      endValue = widget->palette().text().color();
    colorAnimation->setEndValue(endValue);
    colorAnimation->setDuration(2000);
    colorAnimation->start();
  }
}

void QuickInspectorUiFactory::initUi()
{
    PropertyWidget::registerTab<MaterialTab>("material", QObject::tr("Shaders"));
    ObjectBroker::registerClientObjectFactoryCallback<MaterialExtensionInterface*>(createMaterialExtension);
    PropertyWidget::registerTab<SGGeometryTab>("sgGeometry", QObject::tr("Geometry"));
    ObjectBroker::registerClientObjectFactoryCallback<SGGeometryExtensionInterface*>(createSGGeometryExtension);
}

#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
Q_EXPORT_PLUGIN(QuickInspectorUiFactory)
#endif
