// Copyright (C) 2022 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <abstractview.h>

#include <propertyeditorcomponentgenerator.h>

#include <QHash>
#include <QPointer>

QT_BEGIN_NAMESPACE
class QShortcut;
class QStackedWidget;
class QColorDialog;
QT_END_NAMESPACE

namespace QmlDesigner {

class DynamicPropertiesModel;
class ModelNode;
class QmlObjectNode;
class TextureEditorQmlBackend;
class SourcePathCacheInterface;

class TextureEditorView : public AbstractView
{
    Q_OBJECT

public:
    TextureEditorView(class AsynchronousImageCache &imageCache,
                      ExternalDependenciesInterface &externalDependencies);
    ~TextureEditorView() override;

    bool hasWidget() const override;
    WidgetInfo widgetInfo() override;

    void selectedNodesChanged(const QList<ModelNode> &selectedNodeList,
                              const QList<ModelNode> &lastSelectedNodeList) override;

    void propertiesRemoved(const QList<AbstractProperty> &propertyList) override;

    void modelAttached(Model *model) override;
    void modelAboutToBeDetached(Model *model) override;

    void variantPropertiesChanged(const QList<VariantProperty> &propertyList, PropertyChangeFlags propertyChange) override;
    void bindingPropertiesChanged(const QList<BindingProperty> &propertyList, PropertyChangeFlags propertyChange) override;
    void auxiliaryDataChanged(const ModelNode &node,
                              AuxiliaryDataKeyView key,
                              const QVariant &data) override;
    void propertiesAboutToBeRemoved(const QList<AbstractProperty> &propertyList) override;
    void nodeReparented(const ModelNode &node, const NodeAbstractProperty &newPropertyParent,
                        const NodeAbstractProperty &oldPropertyParent,
                        AbstractView::PropertyChangeFlags propertyChange) override;
    void nodeAboutToBeRemoved(const ModelNode &removedNode) override;
    void nodeRemoved(const ModelNode &removedNode,
                     const NodeAbstractProperty &parentProperty,
                     PropertyChangeFlags propertyChange) override;

    void resetView();
    void currentStateChanged(const ModelNode &node) override;
    void instancePropertyChanged(const QList<QPair<ModelNode, PropertyName> > &propertyList) override;

    void importsChanged(const Imports &addedImports, const Imports &removedImports) override;
    void customNotification(const AbstractView *view, const QString &identifier,
                            const QList<ModelNode> &nodeList, const QList<QVariant> &data) override;

    void dragStarted(QMimeData *mimeData) override;
    void dragEnded() override;

    void changeValue(const QString &name);
    void changeExpression(const QString &name);
    void exportPropertyAsAlias(const QString &name);
    void removeAliasExport(const QString &name);

    bool locked() const;

    void currentTimelineChanged(const ModelNode &node) override;

    void refreshMetaInfos(const TypeIds &deletedTypeIds) override;

    DynamicPropertiesModel *dynamicPropertiesModel() const;

    static TextureEditorView *instance();

public slots:
    void handleToolBarAction(int action);

protected:
    void timerEvent(QTimerEvent *event) override;
    void setValue(const QmlObjectNode &fxObjectNode, PropertyNameView name, const QVariant &value);
    bool eventFilter(QObject *obj, QEvent *event) override;

private:
    static QString textureEditorResourcesPath();

    void reloadQml();
    void highlightSupportedProperties(bool highlight = true);

    void applyTextureToSelectedModel(const ModelNode &texture);

    void setupQmlBackend();
    TextureEditorQmlBackend *getQmlBackend(const QUrl &qmlFileUrl);
    QUrl getPaneUrl();
    void setupCurrentQmlBackend(const ModelNode &selectedNode,
                                const QUrl &qmlSpecificsFile,
                                const QString &specificQmlData);
    void setupWidget();

    void commitVariantValueToModel(PropertyNameView propertyName, const QVariant &value);
    void commitAuxValueToModel(PropertyNameView propertyName, const QVariant &value);
    void removePropertyFromModel(PropertyNameView propertyName);
    void duplicateTexture(const ModelNode &texture);

    bool noValidSelection() const;
    void asyncResetView();

    AsynchronousImageCache &m_imageCache;
    ModelNode m_selectedTexture;
    QShortcut *m_updateShortcut = nullptr;
    int m_timerId = 0;
    QStackedWidget *m_stackedWidget = nullptr;
    ModelNode m_selectedModel;
    QHash<QString, TextureEditorQmlBackend *> m_qmlBackendHash;
    TextureEditorQmlBackend *m_qmlBackEnd = nullptr;
    PropertyComponentGenerator m_propertyComponentGenerator;
    PropertyEditorComponentGenerator m_propertyEditorComponentGenerator{m_propertyComponentGenerator};
    bool m_locked = false;
    bool m_setupCompleted = false;
    bool m_hasQuick3DImport = false;
    bool m_hasTextureRoot = false;
    bool m_initializingPreviewData = false;
    ModelNode m_newSelectedTexture;
    bool m_selectedTextureChanged = false;

    QPointer<QColorDialog> m_colorDialog;
    DynamicPropertiesModel *m_dynamicPropertiesModel = nullptr;
};

} // namespace QmlDesigner
