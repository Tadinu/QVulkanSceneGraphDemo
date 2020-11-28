#ifndef VULKAN_SCENE_GRAPH_TEXTURE_H
#define VULKAN_SCENE_GRAPH_TEXTURE_H

#include <QtQuick/QQuickItem>

#define RENDER_IMAGE_TEXTURE (1)

class CustomTextureNode;

//! [1]
class CustomTextureItem : public QQuickItem
{
    Q_OBJECT
    Q_PROPERTY(qreal t READ t WRITE setT NOTIFY tChanged)
    QML_ELEMENT

public:
    CustomTextureItem();

    qreal t() const { return m_t; }
    void setT(qreal t);

signals:
    void tChanged();

protected:
    QSGNode *updatePaintNode(QSGNode *, UpdatePaintNodeData *) override;
    void geometryChanged(const QRectF &newGeometry, const QRectF &oldGeometry) override;

private slots:
    void invalidateSceneGraph();

private:
    void releaseResources() override;

    CustomTextureNode *m_node = nullptr;
    qreal m_t = 0;
};
//! [1]

#endif // VULKAN_SCENE_GRAPH_TEXTURE_H
