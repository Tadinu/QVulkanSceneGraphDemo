/****************************************************************************
**
** Copyright (C) 2019 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the demonstration applications of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:BSD$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** BSD License Usage
** Alternatively, you may use this file under the terms of the BSD license
** as follows:
**
** "Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are
** met:
**   * Redistributions of source code must retain the above copyright
**     notice, this list of conditions and the following disclaimer.
**   * Redistributions in binary form must reproduce the above copyright
**     notice, this list of conditions and the following disclaimer in
**     the documentation and/or other materials provided with the
**     distribution.
**   * Neither the name of The Qt Company Ltd nor the names of its
**     contributors may be used to endorse or promote products derived
**     from this software without specific prior written permission.
**
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
** LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
** A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
** OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
** LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
** OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "VulkanSceneGraphTexture.h"

#include <QtGui/QScreen>
#include <QtQuick/QQuickWindow>
#include <QtQuick/QSGTextureProvider>
#include <QtQuick/QSGSimpleTextureNode>

#include <QVulkanInstance>
#include <QVulkanFunctions>

class CustomTextureNode : public QSGTextureProvider, public QSGSimpleTextureNode
{
    Q_OBJECT

public:
    CustomTextureNode(QQuickItem *item);
    ~CustomTextureNode() override;
    static bool CBALWAYS_STAGE;

    QSGTexture *texture() const override;

    void initResources();
    void sync();

private slots:
    void renderPlainTexture();
    void renderImageTexture();

private:
    QVulkanInstance *m_vulkanInstance = nullptr;
    bool initialize();

    static QImage LoadImage(const QString& inImagePath, const QSize& inImageSize);

    enum Stage {
        VertexStage,
        FragmentStage
    };
    void prepareShader(Stage stage);
    VkShaderModule createShader(const QString &inShaderFilePath);

    quint32 determinePhysicalMemoryIndex(const VkMemoryRequirements& memReq);
    VkImage createPlainTexture(const QSize &inTextureSize);
    bool buildTexture(const QImage &inImage, const QSize& inTextureSize);
    VkImage createTextureImage(const QSize &inTextureSize, VkDeviceMemory *mem,
                               VkImageTiling inTiling, VkImageUsageFlags inUsage);
    bool writeLinearImage(const QImage &img, VkImage image, VkDeviceMemory memory);

    void ensureTexture();
    void freeTexture();
    VkRenderPass createRenderPass();

    QQuickItem *m_item;
    QQuickWindow *m_window;
    QSize m_size;
    qreal m_dpr;

    QByteArray m_vert;
    QByteArray m_frag;

    VkImage m_texImage = VK_NULL_HANDLE;
    VkDeviceMemory m_texMemory = VK_NULL_HANDLE;

    VkImage m_texStaging = VK_NULL_HANDLE;
    VkDeviceMemory m_texStagingMemory = VK_NULL_HANDLE;

    VkFramebuffer m_texFramebuffer = VK_NULL_HANDLE;
    VkImageView m_texView = VK_NULL_HANDLE;

    QSize m_texSize;
    VkFormat m_texFormat;

    bool m_initialized = false;

    float m_t;

    VkPhysicalDevice m_physDev = VK_NULL_HANDLE;
    VkDevice m_dev = VK_NULL_HANDLE;
    QVulkanDeviceFunctions *m_devFuncs = nullptr;
    QVulkanFunctions *m_funcs = nullptr;
    VkCommandBuffer m_commandBuffer = VK_NULL_HANDLE;

    VkBuffer m_buf = VK_NULL_HANDLE;
    VkDeviceMemory m_bufMem = VK_NULL_HANDLE;
    std::vector<VkDescriptorBufferInfo> m_uniformBufInfo;

    VkBuffer m_vbuf = VK_NULL_HANDLE;
    VkDeviceMemory m_vbufMem = VK_NULL_HANDLE;
    VkBuffer m_ubuf = VK_NULL_HANDLE;
    VkDeviceMemory m_ubufMem = VK_NULL_HANDLE;
    VkDeviceSize m_allocPerUbuf = 0;

    VkPipelineCache m_pipelineCache = VK_NULL_HANDLE;
    VkPipelineLayout m_pipelineLayout = VK_NULL_HANDLE;
    VkPipeline m_pipeline = VK_NULL_HANDLE;

    VkDescriptorPool m_descriptorPool = VK_NULL_HANDLE;
    VkDescriptorSetLayout m_descSetLayout = VK_NULL_HANDLE;
    VkDescriptorSet m_ubufDescriptor = VK_NULL_HANDLE;
    std::vector<VkDescriptorSet> m_descSet;

    VkRenderPass m_renderPass = VK_NULL_HANDLE;

    VkSampler m_sampler = VK_NULL_HANDLE;
    bool m_texLayoutPending = false;
    bool m_texStagingPending = false;
};

CustomTextureItem::CustomTextureItem()
{
    setFlag(ItemHasContents, true);
}

void CustomTextureItem::invalidateSceneGraph() // called on the render thread when the scenegraph is invalidated
{
    m_node = nullptr;
}

void CustomTextureItem::releaseResources() // called on the gui thread if the item is removed from scene
{
    m_node = nullptr;
}

QSGNode *CustomTextureItem::updatePaintNode(QSGNode *node, UpdatePaintNodeData *)
{
    CustomTextureNode *n = static_cast<CustomTextureNode *>(node);

    if (!n && (width() <= 0 || height() <= 0))
        return nullptr;

    if (!n) {
        m_node = new CustomTextureNode(this);
        n = m_node;
    }

    m_node->sync();

    n->setTextureCoordinatesTransform(QSGSimpleTextureNode::NoTransform);
    n->setFiltering(QSGTexture::Linear);
    n->setRect(0, 0, width(), height());

    window()->update(); // ensure getting to beforeRendering() at some point

    return n;
}

void CustomTextureItem::geometryChanged(const QRectF &newGeometry, const QRectF &oldGeometry)
{
    QQuickItem::geometryChanged(newGeometry, oldGeometry);

    if (newGeometry.size() != oldGeometry.size())
        update();
}

void CustomTextureItem::setT(qreal t)
{
    if (t == m_t)
        return;

    m_t = t;
    emit tChanged();

    update();
}

bool CustomTextureNode::CBALWAYS_STAGE = false;
CustomTextureNode::CustomTextureNode(QQuickItem *item)
    : m_item(item)
{
    CBALWAYS_STAGE = qEnvironmentVariableIntValue("QT_VK_FORCE_STAGE_TEX");
    qDebug("CBALWAYS_STAGE: %d", CBALWAYS_STAGE);

    m_window = m_item->window();
#if 1 //RENDER_IMAGE_TEXTURE
    connect(m_window, &QQuickWindow::beforeRendering, this, &CustomTextureNode::renderImageTexture);
#else
    connect(m_window, &QQuickWindow::beforeRendering, this, &CustomTextureNode::renderPlainTexture);
#endif
    connect(m_window, &QQuickWindow::screenChanged, this, [this]() {
        if (m_window->effectiveDevicePixelRatio() != m_dpr) {
            m_item->update();
        }
    });
}

CustomTextureNode::~CustomTextureNode()
{
    m_devFuncs->vkDestroyBuffer(m_dev, m_vbuf, nullptr);
    m_devFuncs->vkDestroyBuffer(m_dev, m_ubuf, nullptr);
    m_devFuncs->vkFreeMemory(m_dev, m_vbufMem, nullptr);
    m_devFuncs->vkFreeMemory(m_dev, m_ubufMem, nullptr);

    m_devFuncs->vkDestroyPipelineCache(m_dev, m_pipelineCache, nullptr);
    m_devFuncs->vkDestroyPipelineLayout(m_dev, m_pipelineLayout, nullptr);
    m_devFuncs->vkDestroyPipeline(m_dev, m_pipeline, nullptr);

    m_devFuncs->vkDestroyRenderPass(m_dev, m_renderPass, nullptr);

    m_devFuncs->vkDestroyDescriptorSetLayout(m_dev, m_descSetLayout, nullptr);
    m_devFuncs->vkDestroyDescriptorPool(m_dev, m_descriptorPool, nullptr);

    freeTexture();
}

QSGTexture *CustomTextureNode::texture() const
{
    return QSGSimpleTextureNode::texture();
}

QImage CustomTextureNode::LoadImage(const QString& inImagePath, const QSize& inImageSize)
{
    QImage image(inImagePath);

    if (image.isNull()) {
        qFatal("Failed to load image %s", qPrintable(inImagePath));
    }

    // Resize img to the input texture size
    image = image.scaled(inImageSize);

    // Convert to byte ordered RGBA8. Use premultiplied alpha, see pColorBlendState in the pipeline.
    image = image.convertToFormat(QImage::Format_RGBA8888_Premultiplied);

    return image;
}

// Use a triangle strip to get a quad.
//
// Note that the vertex data and the projection matrix assume OpenGL. With
// Vulkan Y is negated in clip space and the near/far plane is at 0/1 instead
// of -1/1. These will be corrected for by an extra transformation when
// calculating the modelview-projection matrix.
static float vertexData[] = { // Y up, front = CW
    // x, y, z, u, v
    -1, -1, 0, 0, 1,
    -1,  1, 0, 0, 0,
     1, -1, 0, 1, 1,
     1,  1, 0, 1, 0
};

static const int UNIFORM_DATA_SIZE = 16 * sizeof(float);

static const float vertices[] = {
    -1, -1,
    1, -1,
    -1, 1,
    1, 1
};

const int UBUF_SIZE = 4;

quint32 CustomTextureNode::determinePhysicalMemoryIndex(const VkMemoryRequirements& memReq)
{
    Q_ASSERT(m_funcs);
    Q_ASSERT(m_physDev);

    quint32 memIndex = 0;
    VkPhysicalDeviceMemoryProperties physDevMemProps;
    m_funcs->vkGetPhysicalDeviceMemoryProperties(m_physDev, &physDevMemProps);
    for (uint32_t i = 0; i < physDevMemProps.memoryTypeCount; ++i) {
        if (!(memReq.memoryTypeBits & (1 << i))) {
            continue;
        }
        memIndex = i;
    }
    return memIndex;
}

VkImage CustomTextureNode::createPlainTexture(const QSize &inTextureSize)
{
    VkImageCreateInfo imageInfo;
    memset(&imageInfo, 0, sizeof(imageInfo));
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.flags = 0;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
    imageInfo.extent.width = static_cast<uint32_t>(inTextureSize.width());
    imageInfo.extent.height = static_cast<uint32_t>(inTextureSize.height());
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_PREINITIALIZED;

    imageInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    imageInfo.usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    VkImage texImage = VK_NULL_HANDLE;
    if (m_devFuncs->vkCreateImage(m_dev, &imageInfo, nullptr, &texImage) != VK_SUCCESS) {
        qCritical("VulkanWrapper: failed to create image!");
        return  nullptr;
    }

    VkMemoryRequirements memReq;
    m_devFuncs->vkGetImageMemoryRequirements(m_dev, texImage, &memReq);

    quint32 memIndex = determinePhysicalMemoryIndex(memReq);

    VkMemoryAllocateInfo allocInfo = {
        VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        nullptr,
        memReq.size,
        memIndex
    };

    VkResult err = m_devFuncs->vkAllocateMemory(m_dev, &allocInfo, nullptr, &m_texMemory);
    if (err != VK_SUCCESS) {
        qWarning("Failed to allocate memory for linear image: %d", err);
        return nullptr;
    }

    err = m_devFuncs->vkBindImageMemory(m_dev, texImage, m_texMemory, 0);
    if (err != VK_SUCCESS) {
        qWarning("Failed to bind linear image memory: %d", err);
        return nullptr;
    }

    VkImageViewCreateInfo viewInfo;
    memset(&viewInfo, 0, sizeof(viewInfo));
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = texImage;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = imageInfo.format;
    viewInfo.components.r = VK_COMPONENT_SWIZZLE_R;
    viewInfo.components.g = VK_COMPONENT_SWIZZLE_G;
    viewInfo.components.b = VK_COMPONENT_SWIZZLE_B;
    viewInfo.components.a = VK_COMPONENT_SWIZZLE_A;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;

    err = m_devFuncs->vkCreateImageView(m_dev, &viewInfo, nullptr, &m_texView);
    if (err != VK_SUCCESS) {
        qWarning("Failed to create render target image view: %d", err);
        return nullptr;
    }

    VkFramebufferCreateInfo fbInfo;
    memset(&fbInfo, 0, sizeof(fbInfo));
    fbInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    fbInfo.renderPass = m_renderPass;
    fbInfo.attachmentCount = 1;
    fbInfo.pAttachments = &m_texView;
    fbInfo.width = uint32_t(inTextureSize.width());
    fbInfo.height = uint32_t(inTextureSize.height());
    fbInfo.layers = 1;

    err = m_devFuncs->vkCreateFramebuffer(m_dev, &fbInfo, nullptr, &m_texFramebuffer);
    if (err != VK_SUCCESS) {
        qWarning("Failed to create framebuffer: %d", err);
        return nullptr;
    }
    return texImage;
}

bool CustomTextureNode::buildTexture(const QImage &inImage, const QSize& inTextureSize)
{
    const bool srgb = QCoreApplication::arguments().contains(QStringLiteral("--srgb"));
    if (srgb) {
        qDebug("sRGB swapchain was requested, making texture sRGB too");
    }

    m_texFormat = srgb ? VK_FORMAT_R8G8B8A8_SRGB : VK_FORMAT_R8G8B8A8_UNORM;

    // Now we can either map and copy the image data directly, or have to go
    // through a staging buffer to copy and convert into the internal optimal
    // tiling format.
    Q_ASSERT(m_funcs);
    VkFormatProperties props;
    m_funcs->vkGetPhysicalDeviceFormatProperties(m_physDev, m_texFormat, &props);
    const bool canSampleLinear = (props.linearTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT);
    const bool canSampleOptimal = (props.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT);
    if (!canSampleLinear && !canSampleOptimal) {
        qWarning("Neither linear nor optimal image sampling is supported for RGBA8");
        return false;
    }

    if (canSampleLinear && !CBALWAYS_STAGE) {
        m_texImage = createTextureImage(inTextureSize, &m_texMemory,
                                      VK_IMAGE_TILING_LINEAR, VK_IMAGE_USAGE_SAMPLED_BIT);
        if (!m_texImage) {
            return false;
        }

        if (!writeLinearImage(inImage, m_texImage, m_texMemory)) {
            return false;
        }

        m_texLayoutPending = true;
    } else {
        m_texStaging = createTextureImage(inTextureSize, &m_texStagingMemory,
                                      VK_IMAGE_TILING_LINEAR, VK_IMAGE_USAGE_TRANSFER_SRC_BIT);

        if (!m_texStaging) {
            return false;
        }

        m_texImage = createTextureImage(inTextureSize, &m_texMemory,
                                      VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);
        if (!m_texImage) {
            return false;
        }

        if (!writeLinearImage(inImage, m_texStaging, m_texStagingMemory)) {
            return false;
        }

        m_texStagingPending = true;
    }

    VkImageViewCreateInfo viewInfo;
    memset(&viewInfo, 0, sizeof(viewInfo));
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = m_texImage;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = m_texFormat;
    viewInfo.components.r = VK_COMPONENT_SWIZZLE_R;
    viewInfo.components.g = VK_COMPONENT_SWIZZLE_G;
    viewInfo.components.b = VK_COMPONENT_SWIZZLE_B;
    viewInfo.components.a = VK_COMPONENT_SWIZZLE_A;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.levelCount = viewInfo.subresourceRange.layerCount = 1;

    VkResult err = m_devFuncs->vkCreateImageView(m_dev, &viewInfo, nullptr, &m_texView);
    if (err != VK_SUCCESS) {
        qWarning("Failed to create image view for texture: %d", err);
        return false;
    }

    VkFramebufferCreateInfo fbInfo;
    memset(&fbInfo, 0, sizeof(fbInfo));
    fbInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    fbInfo.renderPass = m_renderPass;
    fbInfo.attachmentCount = 1;
    fbInfo.pAttachments = &m_texView;
    fbInfo.width = static_cast<uint32_t>(inTextureSize.width());
    fbInfo.height = static_cast<uint32_t>(inTextureSize.height());
    fbInfo.layers = 1;

    err = m_devFuncs->vkCreateFramebuffer(m_dev, &fbInfo, nullptr, &m_texFramebuffer);
    if (err != VK_SUCCESS) {
        qWarning("Failed to create framebuffer: %d", err);
        return false;
    }

    return true;
}

VkImage CustomTextureNode::createTextureImage(const QSize &size, VkDeviceMemory *mem,
                                              VkImageTiling inTiling, VkImageUsageFlags inUsage)
{
    VkImageCreateInfo imageInfo;
    memset(&imageInfo, 0, sizeof(imageInfo));
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.format = m_texFormat;
    imageInfo.extent.width = static_cast<uint32_t>(size.width());
    imageInfo.extent.height = static_cast<uint32_t>(size.height());
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.tiling = inTiling;
    imageInfo.usage = inUsage;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_PREINITIALIZED;

    VkImage texImage = VK_NULL_HANDLE;
    VkResult err = m_devFuncs->vkCreateImage(m_dev, &imageInfo, nullptr, &texImage);
    if (err != VK_SUCCESS) {
        qWarning("Failed to create linear image for texture: %d", err);
        return nullptr;
    }

    VkMemoryRequirements memReq;
    m_devFuncs->vkGetImageMemoryRequirements(m_dev, texImage, &memReq);

    quint32 memIndex = determinePhysicalMemoryIndex(memReq);

    VkMemoryAllocateInfo allocInfo = {
        VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        nullptr,
        memReq.size,
        memIndex
    };
    qDebug("allocating %u bytes for texture image", uint32_t(memReq.size));

    err = m_devFuncs->vkAllocateMemory(m_dev, &allocInfo, nullptr, mem);
    if (err != VK_SUCCESS) {
        qWarning("Failed to allocate memory for linear image: %d", err);
        return nullptr;
    }

    err = m_devFuncs->vkBindImageMemory(m_dev, texImage, *mem, 0);
    if (err != VK_SUCCESS) {
        qWarning("Failed to bind linear image memory: %d", err);
        return nullptr;
    }

    return texImage;
}

bool CustomTextureNode::writeLinearImage(const QImage &img, VkImage image, VkDeviceMemory memory)
{
    VkImageSubresource subres = {
        VK_IMAGE_ASPECT_COLOR_BIT,
        0, // mip level
        0
    };
    VkSubresourceLayout layout;
    m_devFuncs->vkGetImageSubresourceLayout(m_dev, image, &subres, &layout);

    uchar *p;
    VkResult err = m_devFuncs->vkMapMemory(m_dev, memory, layout.offset, layout.size, 0, reinterpret_cast<void **>(&p));
    if (err != VK_SUCCESS) {
        qWarning("Failed to map memory for linear image: %d", err);
        return false;
    }

    for (int y = 0; y < img.height(); ++y) {
        const uchar *line = img.constScanLine(y);
        memcpy(p, line, size_t(img.width()) * 4);
        p += layout.rowPitch;
    }

    m_devFuncs->vkUnmapMemory(m_dev, memory);
    return true;
}

void CustomTextureNode::ensureTexture()
{
    if (!m_texLayoutPending && !m_texStagingPending) {
        return;
    }
    Q_ASSERT(m_texLayoutPending != m_texStagingPending);

    VkImageMemoryBarrier barrier;
    memset(&barrier, 0, sizeof(barrier));
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.levelCount = barrier.subresourceRange.layerCount = 1;

    if (m_texLayoutPending) {
        m_texLayoutPending = false;

        barrier.oldLayout = VK_IMAGE_LAYOUT_PREINITIALIZED;
        barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        barrier.image = m_texImage;

        m_devFuncs->vkCmdPipelineBarrier(m_commandBuffer,
                                VK_PIPELINE_STAGE_HOST_BIT,
                                VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                                0, 0, nullptr, 0, nullptr,
                                1, &barrier);
    } else {
        m_texStagingPending = false;

        barrier.oldLayout = VK_IMAGE_LAYOUT_PREINITIALIZED;
        barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        barrier.image = m_texStaging;
        m_devFuncs->vkCmdPipelineBarrier(m_commandBuffer,
                                VK_PIPELINE_STAGE_HOST_BIT,
                                VK_PIPELINE_STAGE_TRANSFER_BIT,
                                0, 0, nullptr, 0, nullptr,
                                1, &barrier);

        barrier.oldLayout = VK_IMAGE_LAYOUT_PREINITIALIZED;
        barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.image = m_texImage;
        m_devFuncs->vkCmdPipelineBarrier(m_commandBuffer,
                                VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                VK_PIPELINE_STAGE_TRANSFER_BIT,
                                0, 0, nullptr, 0, nullptr,
                                1, &barrier);

        VkImageCopy copyInfo;
        memset(&copyInfo, 0, sizeof(copyInfo));
        copyInfo.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        copyInfo.srcSubresource.layerCount = 1;
        copyInfo.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        copyInfo.dstSubresource.layerCount = 1;
        copyInfo.extent.width = m_texSize.width();
        copyInfo.extent.height = m_texSize.height();
        copyInfo.extent.depth = 1;
        m_devFuncs->vkCmdCopyImage(m_commandBuffer, m_texStaging, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                          m_texImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyInfo);

        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        barrier.image = m_texImage;
        m_devFuncs->vkCmdPipelineBarrier(m_commandBuffer,
                                VK_PIPELINE_STAGE_TRANSFER_BIT,
                                VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                                0, 0, nullptr, 0, nullptr,
                                1, &barrier);
    }
}

void CustomTextureNode::freeTexture()
{
    delete texture();
    if (m_texImage) {
        m_devFuncs->vkDestroyFramebuffer(m_dev, m_texFramebuffer, nullptr);
        m_texFramebuffer = VK_NULL_HANDLE;

        m_devFuncs->vkDestroyImage(m_dev, m_texImage, nullptr);
        m_texImage = VK_NULL_HANDLE;

        m_devFuncs->vkFreeMemory(m_dev, m_texMemory, nullptr);
        m_texMemory = VK_NULL_HANDLE;

        m_devFuncs->vkDestroyImage(m_dev, m_texStaging, nullptr);
        m_texStaging = VK_NULL_HANDLE;

        m_devFuncs->vkFreeMemory(m_dev, m_texStagingMemory, nullptr);
        m_texStagingMemory = VK_NULL_HANDLE;

        m_devFuncs->vkDestroyImageView(m_dev, m_texView, nullptr);
        m_texView = VK_NULL_HANDLE;

        m_devFuncs->vkDestroyPipeline(m_dev, m_pipeline, nullptr);
        m_pipeline = VK_NULL_HANDLE;

        m_devFuncs->vkDestroyPipelineLayout(m_dev, m_pipelineLayout, nullptr);
        m_pipelineLayout = VK_NULL_HANDLE;

        m_devFuncs->vkDestroyPipelineCache(m_dev, m_pipelineCache, nullptr);
        m_pipelineCache = VK_NULL_HANDLE;

        m_devFuncs->vkDestroyDescriptorSetLayout(m_dev, m_descSetLayout, nullptr);
        m_descSetLayout = VK_NULL_HANDLE;

        m_devFuncs->vkDestroyDescriptorPool(m_dev, m_descriptorPool, nullptr);
        m_descriptorPool = VK_NULL_HANDLE;

        m_devFuncs->vkDestroyBuffer(m_dev, m_buf, nullptr);
        m_buf = VK_NULL_HANDLE;

        m_devFuncs->vkFreeMemory(m_dev, m_bufMem, nullptr);
        m_bufMem = VK_NULL_HANDLE;
    }
}

static inline VkDeviceSize aligned(VkDeviceSize v, VkDeviceSize byteAlign)
{
    return (v + byteAlign - 1) & ~(byteAlign - 1);
}

VkRenderPass CustomTextureNode::createRenderPass()
{
    const VkFormat vkformat = VK_FORMAT_R8G8B8A8_UNORM;
    const VkSampleCountFlagBits samples =  VK_SAMPLE_COUNT_1_BIT;
    VkAttachmentDescription colorAttDesc;
    memset(&colorAttDesc, 0, sizeof(colorAttDesc));
    colorAttDesc.format = vkformat;
    colorAttDesc.samples = samples;
    colorAttDesc.loadOp =  VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttDesc.storeOp =  VK_ATTACHMENT_STORE_OP_STORE;
    colorAttDesc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttDesc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttDesc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttDesc.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    const VkAttachmentReference colorRef = { 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };

    VkSubpassDescription subpassDesc;
    memset(&subpassDesc, 0, sizeof(subpassDesc));
    subpassDesc.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpassDesc.colorAttachmentCount = 1;
    subpassDesc.pColorAttachments = &colorRef;
    subpassDesc.pDepthStencilAttachment = nullptr;
    subpassDesc.pResolveAttachments = nullptr;

    VkRenderPassCreateInfo rpInfo;
    memset(&rpInfo, 0, sizeof(rpInfo));
    rpInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    rpInfo.attachmentCount = 1;
    rpInfo.pAttachments = &colorAttDesc;
    rpInfo.subpassCount = 1;
    rpInfo.pSubpasses = &subpassDesc;

    VkRenderPass renderPass = nullptr;
    VkResult err = m_devFuncs->vkCreateRenderPass(m_dev, &rpInfo, nullptr, &renderPass);
    if (err != VK_SUCCESS) {
        qWarning("Failed to create renderpass: %d", err);
        return nullptr;
    }

    return renderPass;
}

bool CustomTextureNode::initialize()
{
    const int framesInFlight = m_window->graphicsStateInfo().framesInFlight;
    m_initialized = true;

    QSGRendererInterface *rif = m_window->rendererInterface();
    m_vulkanInstance = reinterpret_cast<QVulkanInstance *>(
                rif->getResource(m_window, QSGRendererInterface::VulkanInstanceResource));
    Q_ASSERT(m_vulkanInstance && m_vulkanInstance->isValid());
    Q_ASSERT(m_vulkanInstance == m_window->vulkanInstance());

    m_physDev = *static_cast<VkPhysicalDevice *>(rif->getResource(m_window, QSGRendererInterface::PhysicalDeviceResource));
    m_dev = *static_cast<VkDevice *>(rif->getResource(m_window, QSGRendererInterface::DeviceResource));
    Q_ASSERT(m_physDev && m_dev);

    m_devFuncs = m_vulkanInstance->deviceFunctions(m_dev);
    m_funcs = m_vulkanInstance->functions();
    Q_ASSERT(m_devFuncs && m_funcs);

    m_renderPass = createRenderPass();
    Q_ASSERT(m_renderPass);

    VkPhysicalDeviceProperties physDevProps;
    m_funcs->vkGetPhysicalDeviceProperties(m_physDev, &physDevProps);

    VkPhysicalDeviceMemoryProperties physDevMemProps;
    m_funcs->vkGetPhysicalDeviceMemoryProperties(m_physDev, &physDevMemProps);

    VkBufferCreateInfo bufferInfo;
    memset(&bufferInfo, 0, sizeof(bufferInfo));
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = sizeof(vertices);
    bufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    VkResult err = m_devFuncs->vkCreateBuffer(m_dev, &bufferInfo, nullptr, &m_vbuf);
    if (err != VK_SUCCESS) {
        qFatal("Failed to create vertex buffer: %d", err);
    }

    VkMemoryRequirements memReq;
    m_devFuncs->vkGetBufferMemoryRequirements(m_dev, m_vbuf, &memReq);
    VkMemoryAllocateInfo memAllocInfo = {
        VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        nullptr,
        memReq.size,
        0
    };

    uint32_t memTypeIndex = uint32_t(-1);
    const VkMemoryType *memType = physDevMemProps.memoryTypes;
    for (uint32_t i = 0; i < physDevMemProps.memoryTypeCount; ++i) {
        if (memReq.memoryTypeBits & (1 << i)) {
            if ((memType[i].propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)
                    && (memType[i].propertyFlags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT))
            {
                memTypeIndex = i;
                break;
            }
        }
    }
    if (memTypeIndex == uint32_t(-1)) {
        qFatal("Failed to find host visible and coherent memory type");
    }

    memAllocInfo.memoryTypeIndex = memTypeIndex;
    err = m_devFuncs->vkAllocateMemory(m_dev, &memAllocInfo, nullptr, &m_vbufMem);
    if (err != VK_SUCCESS) {
        qFatal("Failed to allocate vertex buffer memory of size %u: %d", uint(memAllocInfo.allocationSize), err);
    }

    void *p = nullptr;
    err = m_devFuncs->vkMapMemory(m_dev, m_vbufMem, 0, memAllocInfo.allocationSize, 0, &p);
    if (err != VK_SUCCESS || !p) {
        qFatal("Failed to map vertex buffer memory: %d", err);
    }
    memcpy(p, vertices, sizeof(vertices));
    m_devFuncs->vkUnmapMemory(m_dev, m_vbufMem);
    err = m_devFuncs->vkBindBufferMemory(m_dev, m_vbuf, m_vbufMem, 0);
    if (err != VK_SUCCESS) {
        qFatal("Failed to bind vertex buffer memory: %d", err);
    }

    m_allocPerUbuf = aligned(UBUF_SIZE, physDevProps.limits.minUniformBufferOffsetAlignment);

    bufferInfo.size = framesInFlight * m_allocPerUbuf;
    bufferInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    err = m_devFuncs->vkCreateBuffer(m_dev, &bufferInfo, nullptr, &m_ubuf);
    if (err != VK_SUCCESS) {
        qFatal("Failed to create uniform buffer: %d", err);
    }

    m_devFuncs->vkGetBufferMemoryRequirements(m_dev, m_ubuf, &memReq);
    memTypeIndex = uint32_t(-1);
    for (uint32_t i = 0; i < physDevMemProps.memoryTypeCount; ++i) {
        if (memReq.memoryTypeBits & (1 << i)) {
            if ((memType[i].propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)
                    && (memType[i].propertyFlags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT))
            {
                memTypeIndex = i;
                break;
            }
        }
    }
    if (memTypeIndex == uint32_t(-1)) {
        qFatal("Failed to find host visible and coherent memory type");
    }

    memAllocInfo.allocationSize = qMax(memReq.size, framesInFlight * m_allocPerUbuf);
    memAllocInfo.memoryTypeIndex = memTypeIndex;
    err = m_devFuncs->vkAllocateMemory(m_dev, &memAllocInfo, nullptr, &m_ubufMem);
    if (err != VK_SUCCESS) {
        qFatal("Failed to allocate uniform buffer memory of size %u: %d", uint(memAllocInfo.allocationSize), err);
    }

    err = m_devFuncs->vkBindBufferMemory(m_dev, m_ubuf, m_ubufMem, 0);
    if (err != VK_SUCCESS) {
        qFatal("Failed to bind uniform buffer memory: %d", err);
    }

    // Now onto the pipeline.

    VkPipelineCacheCreateInfo pipelineCacheInfo;
    memset(&pipelineCacheInfo, 0, sizeof(pipelineCacheInfo));
    pipelineCacheInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
    err = m_devFuncs->vkCreatePipelineCache(m_dev, &pipelineCacheInfo, nullptr, &m_pipelineCache);
    if (err != VK_SUCCESS) {
        qFatal("Failed to create pipeline cache: %d", err);
    }

    VkDescriptorSetLayoutBinding descLayoutBinding;
    memset(&descLayoutBinding, 0, sizeof(descLayoutBinding));
    descLayoutBinding.binding = 0;
    descLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
    descLayoutBinding.descriptorCount = 1;
    descLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    VkDescriptorSetLayoutCreateInfo layoutInfo;
    memset(&layoutInfo, 0, sizeof(layoutInfo));
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = 1;
    layoutInfo.pBindings = &descLayoutBinding;
    err = m_devFuncs->vkCreateDescriptorSetLayout(m_dev, &layoutInfo, nullptr, &m_descSetLayout);
    if (err != VK_SUCCESS) {
        qFatal("Failed to create descriptor set layout: %d", err);
    }

    VkPipelineLayoutCreateInfo pipelineLayoutInfo;
    memset(&pipelineLayoutInfo, 0, sizeof(pipelineLayoutInfo));
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &m_descSetLayout;
    err = m_devFuncs->vkCreatePipelineLayout(m_dev, &pipelineLayoutInfo, nullptr, &m_pipelineLayout);
    if (err != VK_SUCCESS) {
        qWarning("Failed to create pipeline layout: %d", err);
    }

    VkGraphicsPipelineCreateInfo pipelineInfo;
    memset(&pipelineInfo, 0, sizeof(pipelineInfo));
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;

    VkShaderModuleCreateInfo shaderInfo;
    memset(&shaderInfo, 0, sizeof(shaderInfo));
    shaderInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    shaderInfo.codeSize = m_vert.size();
    shaderInfo.pCode = reinterpret_cast<const quint32 *>(m_vert.constData());
    VkShaderModule vertShaderModule;
    err = m_devFuncs->vkCreateShaderModule(m_dev, &shaderInfo, nullptr, &vertShaderModule);
    if (err != VK_SUCCESS) {
        qFatal("Failed to create vertex shader module: %d", err);
    }

    shaderInfo.codeSize = m_frag.size();
    shaderInfo.pCode = reinterpret_cast<const quint32 *>(m_frag.constData());
    VkShaderModule fragShaderModule;
    err = m_devFuncs->vkCreateShaderModule(m_dev, &shaderInfo, nullptr, &fragShaderModule);
    if (err != VK_SUCCESS) {
        qFatal("Failed to create fragment shader module: %d", err);
    }

    VkPipelineShaderStageCreateInfo stageInfo[2];
    memset(&stageInfo, 0, sizeof(stageInfo));
    stageInfo[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stageInfo[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    stageInfo[0].module = vertShaderModule;
    stageInfo[0].pName = "main";
    stageInfo[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stageInfo[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    stageInfo[1].module = fragShaderModule;
    stageInfo[1].pName = "main";
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = stageInfo;

    VkVertexInputBindingDescription vertexBinding = {
        0, // binding
        2 * sizeof(float), // stride
        VK_VERTEX_INPUT_RATE_VERTEX
    };
    VkVertexInputAttributeDescription vertexAttr = {
        0, // location
        0, // binding
        VK_FORMAT_R32G32_SFLOAT, // 'vertices' only has 2 floats per vertex
        0 // offset
    };
    VkPipelineVertexInputStateCreateInfo vertexInputInfo;
    memset(&vertexInputInfo, 0, sizeof(vertexInputInfo));
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexBindingDescriptionCount = 1;
    vertexInputInfo.pVertexBindingDescriptions = &vertexBinding;
    vertexInputInfo.vertexAttributeDescriptionCount = 1;
    vertexInputInfo.pVertexAttributeDescriptions = &vertexAttr;
    pipelineInfo.pVertexInputState = &vertexInputInfo;

    VkDynamicState dynStates[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
    VkPipelineDynamicStateCreateInfo dynamicInfo;
    memset(&dynamicInfo, 0, sizeof(dynamicInfo));
    dynamicInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicInfo.dynamicStateCount = 2;
    dynamicInfo.pDynamicStates = dynStates;
    pipelineInfo.pDynamicState = &dynamicInfo;

    VkPipelineViewportStateCreateInfo viewportInfo;
    memset(&viewportInfo, 0, sizeof(viewportInfo));
    viewportInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportInfo.viewportCount = viewportInfo.scissorCount = 1;
    pipelineInfo.pViewportState = &viewportInfo;

    VkPipelineInputAssemblyStateCreateInfo iaInfo;
    memset(&iaInfo, 0, sizeof(iaInfo));
    iaInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    iaInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
    pipelineInfo.pInputAssemblyState = &iaInfo;

    VkPipelineRasterizationStateCreateInfo rsInfo;
    memset(&rsInfo, 0, sizeof(rsInfo));
    rsInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rsInfo.lineWidth = 1.0f;
    pipelineInfo.pRasterizationState = &rsInfo;

    VkPipelineMultisampleStateCreateInfo msInfo;
    memset(&msInfo, 0, sizeof(msInfo));
    msInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    msInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    pipelineInfo.pMultisampleState = &msInfo;

    VkPipelineDepthStencilStateCreateInfo dsInfo;
    memset(&dsInfo, 0, sizeof(dsInfo));
    dsInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    pipelineInfo.pDepthStencilState = &dsInfo;

    // SrcAlpha, One
    VkPipelineColorBlendStateCreateInfo blendInfo;
    memset(&blendInfo, 0, sizeof(blendInfo));
    blendInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    VkPipelineColorBlendAttachmentState blend;
    memset(&blend, 0, sizeof(blend));
    blend.blendEnable = true;
    blend.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    blend.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
    blend.colorBlendOp = VK_BLEND_OP_ADD;
    blend.srcAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    blend.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    blend.alphaBlendOp = VK_BLEND_OP_ADD;
    blend.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT
            | VK_COLOR_COMPONENT_A_BIT;
    blendInfo.attachmentCount = 1;
    blendInfo.pAttachments = &blend;
    pipelineInfo.pColorBlendState = &blendInfo;

    pipelineInfo.layout = m_pipelineLayout;

    pipelineInfo.renderPass = m_renderPass;

    err = m_devFuncs->vkCreateGraphicsPipelines(m_dev, m_pipelineCache, 1, &pipelineInfo, nullptr, &m_pipeline);

    m_devFuncs->vkDestroyShaderModule(m_dev, vertShaderModule, nullptr);
    m_devFuncs->vkDestroyShaderModule(m_dev, fragShaderModule, nullptr);

    if (err != VK_SUCCESS) {
        qFatal("Failed to create graphics pipeline: %d", err);
    }

    // Now just need some descriptors.
    VkDescriptorPoolSize descPoolSizes[] = {
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1 }
    };
    VkDescriptorPoolCreateInfo descPoolInfo;
    memset(&descPoolInfo, 0, sizeof(descPoolInfo));
    descPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    descPoolInfo.flags = 0; // won't use vkFreeDescriptorSets
    descPoolInfo.maxSets = 1;
    descPoolInfo.poolSizeCount = sizeof(descPoolSizes) / sizeof(descPoolSizes[0]);
    descPoolInfo.pPoolSizes = descPoolSizes;
    err = m_devFuncs->vkCreateDescriptorPool(m_dev, &descPoolInfo, nullptr, &m_descriptorPool);
    if (err != VK_SUCCESS) {
        qFatal("Failed to create descriptor pool: %d", err);
    }

    VkDescriptorSetAllocateInfo descAllocInfo;
    memset(&descAllocInfo, 0, sizeof(descAllocInfo));
    descAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    descAllocInfo.descriptorPool = m_descriptorPool;
    descAllocInfo.descriptorSetCount = 1;
    descAllocInfo.pSetLayouts = &m_descSetLayout;
    err = m_devFuncs->vkAllocateDescriptorSets(m_dev, &descAllocInfo, &m_ubufDescriptor);
    if (err != VK_SUCCESS) {
        qFatal("Failed to allocate descriptor set");
    }

    VkWriteDescriptorSet writeInfo;
    memset(&writeInfo, 0, sizeof(writeInfo));
    writeInfo.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writeInfo.dstSet = m_ubufDescriptor;
    writeInfo.dstBinding = 0;
    writeInfo.descriptorCount = 1;
    writeInfo.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
    VkDescriptorBufferInfo bufInfo;
    bufInfo.buffer = m_ubuf;
    bufInfo.offset = 0; // dynamic offset is used so this is ignored
    bufInfo.range = UBUF_SIZE;
    writeInfo.pBufferInfo = &bufInfo;

    m_devFuncs->vkUpdateDescriptorSets(m_dev, 1, &writeInfo, 0, nullptr);
    return true;
}

void CustomTextureNode::initResources()
{
    m_initialized = true;

    // [VK INSTANCE] --
    QSGRendererInterface *rif = m_window->rendererInterface();
    m_vulkanInstance = reinterpret_cast<QVulkanInstance *>(
                rif->getResource(m_window, QSGRendererInterface::VulkanInstanceResource));
    Q_ASSERT(m_vulkanInstance && m_vulkanInstance->isValid());
    Q_ASSERT(m_vulkanInstance == m_window->vulkanInstance());
    m_vulkanInstance->setLayers(QByteArrayList() << "VK_LAYER_LUNARG_standard_validation");

    // [VK DEVICE] --
    m_dev = *static_cast<VkDevice *>(rif->getResource(m_window, QSGRendererInterface::DeviceResource));
    Q_ASSERT(m_dev);
    m_devFuncs = m_vulkanInstance->deviceFunctions(m_dev);
    m_funcs = m_vulkanInstance->functions();
    Q_ASSERT(m_devFuncs && m_funcs);

    // [VK PHYSICAL DEVICE] --
    m_physDev = *static_cast<VkPhysicalDevice *>(rif->getResource(m_window, QSGRendererInterface::PhysicalDeviceResource));
    Q_ASSERT(m_physDev);

    // [VK COMMAND BUFFER] --
    m_commandBuffer = *reinterpret_cast<VkCommandBuffer *>(
                        rif->getResource(m_window, QSGRendererInterface::CommandListResource));
    Q_ASSERT(m_commandBuffer);

    // [VK RENDER PASS] --
    m_renderPass = createRenderPass();
    Q_ASSERT(m_renderPass);

    // The setup is similar to hellovulkantriangle. The difference is the
    // presence of a second vertex attribute (texcoord), a sampler, and that we
    // need blending.

    const uint32_t concurrentFrameCount = static_cast<uint32_t>(m_window->graphicsStateInfo().framesInFlight);
    qDebug("concurrentFrameCount: %u", concurrentFrameCount);
    m_descSet.assign(concurrentFrameCount, VkDescriptorSet());
    m_uniformBufInfo.assign(concurrentFrameCount, VkDescriptorBufferInfo());

    // [VK PHYSICAL DEVICE] --
    VkPhysicalDeviceProperties physDevProps;
    m_funcs->vkGetPhysicalDeviceProperties(m_physDev, &physDevProps);

    VkPhysicalDeviceMemoryProperties physDevMemProps;
    m_funcs->vkGetPhysicalDeviceMemoryProperties(m_physDev, &physDevMemProps);

    VkPhysicalDeviceLimits pdevLimits = physDevProps.limits;
    const VkDeviceSize uniAlign = pdevLimits.minUniformBufferOffsetAlignment;
    qDebug("uniform buffer offset alignment is %u", (uint) uniAlign);

    // [VK BUFFER CREATION INFO] --
    VkBufferCreateInfo bufInfo;
    memset(&bufInfo, 0, sizeof(bufInfo));
    bufInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    // Our internal layout is vertex, uniform, uniform, ... with each uniform buffer start offset aligned to uniAlign.
    const VkDeviceSize vertexAllocSize = aligned(sizeof(vertexData), uniAlign);
    const VkDeviceSize uniformAllocSize = aligned(UNIFORM_DATA_SIZE, uniAlign);
    m_allocPerUbuf = uniformAllocSize;
    bufInfo.size = vertexAllocSize + concurrentFrameCount * uniformAllocSize;
    bufInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;

    VkResult err = m_devFuncs->vkCreateBuffer(m_dev, &bufInfo, nullptr, &m_buf);
    if (err != VK_SUCCESS) {
        qFatal("Failed to create buffer: %d", err);
    }

    // [VK MEMORY ALLOCATION] --
    VkMemoryRequirements memReq;
    m_devFuncs->vkGetBufferMemoryRequirements(m_dev, m_buf, &memReq);
    VkMemoryAllocateInfo memAllocInfo = {
        VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        nullptr,
        memReq.size,
        0
    };

    uint32_t memTypeIndex = uint32_t(-1);
    const VkMemoryType *memType = physDevMemProps.memoryTypes;
    for (uint32_t i = 0; i < physDevMemProps.memoryTypeCount; ++i) {
        if (memReq.memoryTypeBits & (1 << i)) {
            if ((memType[i].propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)
                    && (memType[i].propertyFlags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT))
            {
                memTypeIndex = i;
                break;
            }
        }
    }
    if (memTypeIndex == uint32_t(-1)) {
        qFatal("Failed to find host visible and coherent memory type");
    }
    memAllocInfo.memoryTypeIndex = memTypeIndex;

    err = m_devFuncs->vkAllocateMemory(m_dev, &memAllocInfo, nullptr, &m_bufMem);
    if (err != VK_SUCCESS) {
        qFatal("Failed to allocate memory: %d", err);
    }

    // [VK BUFFER MEMORY BINDING] --
    err = m_devFuncs->vkBindBufferMemory(m_dev, m_buf, m_bufMem, 0);
    if (err != VK_SUCCESS) {
        qFatal("Failed to bind buffer memory: %d", err);
    }

    // [VK BUFFER MEMORY MAPPING/UNMAPPING] --
    void *p = nullptr;
    err = m_devFuncs->vkMapMemory(m_dev, m_bufMem, 0, memAllocInfo.allocationSize, 0, &p);
    if (err != VK_SUCCESS) {
        qFatal("Failed to map memory: %d", err);
    }
    memcpy(p, vertexData, sizeof(vertexData));
    QMatrix4x4 ident;

    quint8* pi = reinterpret_cast<quint8*>(p);
    for (uint32_t i = 0; i < concurrentFrameCount; ++i) {
        const VkDeviceSize offset = vertexAllocSize + i * uniformAllocSize;
        memcpy(pi + offset, ident.constData(), 16 * sizeof(float));
        m_uniformBufInfo[i].buffer = m_buf;
        m_uniformBufInfo[i].offset = offset;
        m_uniformBufInfo[i].range = uniformAllocSize;
    }
    m_devFuncs->vkUnmapMemory(m_dev, m_bufMem);

    // [VK GRAPHICS PIPELINE] --
    VkVertexInputBindingDescription vertexBindingDesc = {
        0, // binding
        5 * sizeof(float),
        VK_VERTEX_INPUT_RATE_VERTEX
    };
    VkVertexInputAttributeDescription vertexAttrDesc[] = {
        { // position
            0, // location
            0, // binding
            VK_FORMAT_R32G32B32_SFLOAT,
            0
        },
        { // texcoord
            1,
            0,
            VK_FORMAT_R32G32_SFLOAT,
            3 * sizeof(float)
        }
    };

    VkPipelineVertexInputStateCreateInfo vertexInputInfo;
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.pNext = nullptr;
    vertexInputInfo.flags = 0;
    vertexInputInfo.vertexBindingDescriptionCount = 1;
    vertexInputInfo.pVertexBindingDescriptions = &vertexBindingDesc;
    vertexInputInfo.vertexAttributeDescriptionCount = 2;
    vertexInputInfo.pVertexAttributeDescriptions = vertexAttrDesc;

    // Sampler.
    VkSamplerCreateInfo samplerInfo;
    memset(&samplerInfo, 0, sizeof(samplerInfo));
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_NEAREST;
    samplerInfo.minFilter = VK_FILTER_NEAREST;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.maxAnisotropy = 1.0f;
    err = m_devFuncs->vkCreateSampler(m_dev, &samplerInfo, nullptr, &m_sampler);
    if (err != VK_SUCCESS) {
        qFatal("Failed to create sampler: %d", err);
    }

    // Set up descriptor set and its layout.
    VkDescriptorPoolSize descPoolSizes[2] = {
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, uint32_t(concurrentFrameCount) },
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, uint32_t(concurrentFrameCount) }
    };
    VkDescriptorPoolCreateInfo descPoolInfo;
    memset(&descPoolInfo, 0, sizeof(descPoolInfo));
    descPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    descPoolInfo.maxSets = concurrentFrameCount;
    descPoolInfo.poolSizeCount = 2;
    descPoolInfo.pPoolSizes = descPoolSizes;
    err = m_devFuncs->vkCreateDescriptorPool(m_dev, &descPoolInfo, nullptr, &m_descriptorPool);
    if (err != VK_SUCCESS) {
        qFatal("Failed to create descriptor pool: %d", err);
    }

    VkDescriptorSetLayoutBinding layoutBinding[2] =
    {
        {
            0, // binding
            VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            1, // descriptorCount
            VK_SHADER_STAGE_VERTEX_BIT,
            nullptr
        },
        {
            1, // binding
            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            1, // descriptorCount
            VK_SHADER_STAGE_FRAGMENT_BIT,
            nullptr
        }
    };
    VkDescriptorSetLayoutCreateInfo descLayoutInfo = {
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        nullptr,
        0,
        2, // bindingCount
        layoutBinding
    };
    err = m_devFuncs->vkCreateDescriptorSetLayout(m_dev, &descLayoutInfo, nullptr, &m_descSetLayout);
    if (err != VK_SUCCESS) {
        qFatal("Failed to create descriptor set layout: %d", err);
    }

    for (uint32_t i = 0; i < concurrentFrameCount; ++i) {
        VkDescriptorSetAllocateInfo descSetAllocInfo = {
            VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
            nullptr,
            m_descriptorPool,
            1,
            &m_descSetLayout
        };
        err = m_devFuncs->vkAllocateDescriptorSets(m_dev, &descSetAllocInfo, &m_descSet[i]);
        if (err != VK_SUCCESS) {
            qFatal("Failed to allocate descriptor set: %d", err);
        }

        VkWriteDescriptorSet descWrite[2];
        memset(descWrite, 0, sizeof(descWrite));
        descWrite[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descWrite[0].dstSet = m_descSet[i];
        descWrite[0].dstBinding = 0;
        descWrite[0].descriptorCount = 1;
        descWrite[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descWrite[0].pBufferInfo = &m_uniformBufInfo[i];

        VkDescriptorImageInfo descImageInfo = {
            m_sampler,
            m_texView,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
        };

        descWrite[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descWrite[1].dstSet = m_descSet[i];
        descWrite[1].dstBinding = 1;
        descWrite[1].descriptorCount = 1;
        descWrite[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descWrite[1].pImageInfo = &descImageInfo;

        m_devFuncs->vkUpdateDescriptorSets(m_dev, 2, descWrite, 0, nullptr);
    }

    // Pipeline cache
    VkPipelineCacheCreateInfo pipelineCacheInfo;
    memset(&pipelineCacheInfo, 0, sizeof(pipelineCacheInfo));
    pipelineCacheInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
    err = m_devFuncs->vkCreatePipelineCache(m_dev, &pipelineCacheInfo, nullptr, &m_pipelineCache);
    if (err != VK_SUCCESS) {
        qFatal("Failed to create pipeline cache: %d", err);
    }

    // Pipeline layout
    VkPipelineLayoutCreateInfo pipelineLayoutInfo;
    memset(&pipelineLayoutInfo, 0, sizeof(pipelineLayoutInfo));
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &m_descSetLayout;
    err = m_devFuncs->vkCreatePipelineLayout(m_dev, &pipelineLayoutInfo, nullptr, &m_pipelineLayout);
    if (err != VK_SUCCESS) {
        qFatal("Failed to create pipeline layout: %d", err);
    }

    // Shaders
    VkShaderModule vertShaderModule = createShader(QStringLiteral(":/texture_vert.spv"));
    VkShaderModule fragShaderModule = createShader(QStringLiteral(":/texture_frag.spv"));

    // Graphics pipeline
    VkGraphicsPipelineCreateInfo pipelineInfo;
    memset(&pipelineInfo, 0, sizeof(pipelineInfo));
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;

    VkPipelineShaderStageCreateInfo shaderStages[2] = {
        {
            VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            nullptr,
            0,
            VK_SHADER_STAGE_VERTEX_BIT,
            vertShaderModule,
            "main",
            nullptr
        },
        {
            VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            nullptr,
            0,
            VK_SHADER_STAGE_FRAGMENT_BIT,
            fragShaderModule,
            "main",
            nullptr
        }
    };
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = shaderStages;

    pipelineInfo.pVertexInputState = &vertexInputInfo;

    VkPipelineInputAssemblyStateCreateInfo ia;
    memset(&ia, 0, sizeof(ia));
    ia.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
    pipelineInfo.pInputAssemblyState = &ia;

    // The viewport and scissor will be set dynamically via vkCmdSetViewport/Scissor.
    // This way the pipeline does not need to be touched when resizing the window.
    VkPipelineViewportStateCreateInfo vp;
    memset(&vp, 0, sizeof(vp));
    vp.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    vp.viewportCount = 1;
    vp.scissorCount = 1;
    pipelineInfo.pViewportState = &vp;

    VkPipelineRasterizationStateCreateInfo rs;
    memset(&rs, 0, sizeof(rs));
    rs.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rs.polygonMode = VK_POLYGON_MODE_FILL;
    rs.cullMode = VK_CULL_MODE_BACK_BIT;
    rs.frontFace = VK_FRONT_FACE_CLOCKWISE;
    rs.lineWidth = 1.0f;
    pipelineInfo.pRasterizationState = &rs;

    VkPipelineMultisampleStateCreateInfo ms;
    memset(&ms, 0, sizeof(ms));
    ms.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    pipelineInfo.pMultisampleState = &ms;

    VkPipelineDepthStencilStateCreateInfo ds;
    memset(&ds, 0, sizeof(ds));
    ds.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    ds.depthTestEnable = VK_TRUE;
    ds.depthWriteEnable = VK_TRUE;
    ds.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
    pipelineInfo.pDepthStencilState = &ds;

    VkPipelineColorBlendStateCreateInfo cb;
    memset(&cb, 0, sizeof(cb));
    cb.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    // assume pre-multiplied alpha, blend, write out all of rgba
    VkPipelineColorBlendAttachmentState att;
    memset(&att, 0, sizeof(att));
    att.colorWriteMask = 0xF;
    att.blendEnable = VK_TRUE;
    att.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
    att.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    att.colorBlendOp = VK_BLEND_OP_ADD;
    att.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    att.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    att.alphaBlendOp = VK_BLEND_OP_ADD;
    cb.attachmentCount = 1;
    cb.pAttachments = &att;
    pipelineInfo.pColorBlendState = &cb;

    VkDynamicState dynEnable[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
    VkPipelineDynamicStateCreateInfo dyn;
    memset(&dyn, 0, sizeof(dyn));
    dyn.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dyn.dynamicStateCount = sizeof(dynEnable) / sizeof(VkDynamicState);
    dyn.pDynamicStates = dynEnable;
    pipelineInfo.pDynamicState = &dyn;

    pipelineInfo.layout = m_pipelineLayout;
    pipelineInfo.renderPass = m_renderPass;

    err = m_devFuncs->vkCreateGraphicsPipelines(m_dev, m_pipelineCache, 1, &pipelineInfo, nullptr, &m_pipeline);
    if (err != VK_SUCCESS) {
        qFatal("Failed to create graphics pipeline: %d", err);
    }

    if (vertShaderModule) {
        m_devFuncs->vkDestroyShaderModule(m_dev, vertShaderModule, nullptr);
    }
    if (fragShaderModule) {
        m_devFuncs->vkDestroyShaderModule(m_dev, fragShaderModule, nullptr);
    }
}

void CustomTextureNode::sync()
{
    m_dpr = m_window->effectiveDevicePixelRatio();
    const QSize newSize = m_window->size() * m_dpr;

    if (!m_initialized) {
#if RENDER_IMAGE_TEXTURE
        initResources();
#else // PLAIN TEXTURE
        prepareShader(VertexStage);
        prepareShader(FragmentStage);
        initialize();
#endif
        m_initialized = true;
    }

    bool needsNew = (!texture()) ||
                    (newSize != m_size);

    if (needsNew) {
        freeTexture();

        m_size = newSize;
        QImage image = LoadImage(QStringLiteral(":/Floor_M.png"), newSize);
#if RENDER_IMAGE_TEXTURE
        buildTexture(image, newSize);
        QSGTexture *sgTexture = m_window->createTextureFromImage(image);
#else // PLAIN TEXTURE
        m_texImage = createPlainTexture(m_size);

        QSGTexture *sgTexture = m_window->createTextureFromNativeObject(QQuickWindow::NativeObjectTexture,
                                                                      &m_texImage,
                                                                      VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                                                      m_size);
#endif
        setTexture(sgTexture);
    }

    m_t = float(static_cast<CustomTextureItem *>(m_item)->t());
}

void CustomTextureNode::renderPlainTexture()
{
    if (!m_initialized)
        return;

    VkResult err = VK_SUCCESS;

    uint currentFrameSlot = m_window->graphicsStateInfo().currentFrameSlot;
    void *p = nullptr;
    err = m_devFuncs->vkMapMemory(m_dev, m_bufMem, m_uniformBufInfo[currentFrameSlot].offset,
            UNIFORM_DATA_SIZE, 0, reinterpret_cast<void **>(&p));
    if (err != VK_SUCCESS || !p) {
        qFatal("Failed to map uniform buffer memory: %d", err);
    }
    float t = m_t;
    memcpy(p, &t, UNIFORM_DATA_SIZE);
    m_devFuncs->vkUnmapMemory(m_dev, m_bufMem);

    VkClearValue clearColor = {{ {0, 0, 0, 1} }};

    VkRenderPassBeginInfo rpBeginInfo = {};
    rpBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rpBeginInfo.renderPass = m_renderPass;
    rpBeginInfo.framebuffer = m_texFramebuffer;
    rpBeginInfo.renderArea.extent.width = static_cast<uint32_t>(m_size.width());
    rpBeginInfo.renderArea.extent.height = static_cast<uint32_t>(m_size.height());
    rpBeginInfo.clearValueCount = 1;
    rpBeginInfo.pClearValues = &clearColor;

    m_devFuncs->vkCmdBeginRenderPass(m_commandBuffer, &rpBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

    m_devFuncs->vkCmdBindPipeline(m_commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline);

    VkDeviceSize vbufOffset = 0;
    m_devFuncs->vkCmdBindVertexBuffers(m_commandBuffer, 0, 1, &m_vbuf, &vbufOffset);

    uint32_t dynamicOffset = m_allocPerUbuf * currentFrameSlot;
    m_devFuncs->vkCmdBindDescriptorSets(m_commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout, 0, 1,
                                        &m_descSet[currentFrameSlot], 1, &dynamicOffset);

    VkViewport vp = { 0, 0, float(m_size.width()), float(m_size.height()), 0.0f, 1.0f };
    m_devFuncs->vkCmdSetViewport(m_commandBuffer, 0, 1, &vp);
    VkRect2D scissor = { { 0, 0 }, { uint32_t(m_size.width()), uint32_t(m_size.height()) } };
    m_devFuncs->vkCmdSetScissor(m_commandBuffer, 0, 1, &scissor);

    m_devFuncs->vkCmdDraw(m_commandBuffer, 4, 1, 0, 0);
    m_devFuncs->vkCmdEndRenderPass(m_commandBuffer);

    // Memory barrier before the texture can be used as a source.
    // Since we are not using a sub-pass, we have to do this explicitly.

    VkImageMemoryBarrier imageTransitionBarrier = {};
    imageTransitionBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    imageTransitionBarrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    imageTransitionBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    imageTransitionBarrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    imageTransitionBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    imageTransitionBarrier.image = m_texImage;
    imageTransitionBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    imageTransitionBarrier.subresourceRange.levelCount = imageTransitionBarrier.subresourceRange.layerCount = 1;

    m_devFuncs->vkCmdPipelineBarrier(m_commandBuffer,
                                     VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                                     VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                                     0, 0, nullptr, 0, nullptr,
                                     1, &imageTransitionBarrier);
}

void CustomTextureNode::renderImageTexture()
{
    // Add the necessary barriers and do the host-linear -> device-optimal copy, if not yet done.
    ensureTexture();

    VkClearColorValue clearColor = {{ 0, 0, 0, 1 }};
    VkClearDepthStencilValue clearDS = { 1, 0 };
    VkClearValue clearValues[2];
    memset(clearValues, 0, sizeof(clearValues));
    clearValues[0].color = clearColor;
    clearValues[1].depthStencil = clearDS;

    VkRenderPassBeginInfo rpBeginInfo;
    memset(&rpBeginInfo, 0, sizeof(rpBeginInfo));
    rpBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rpBeginInfo.renderPass = m_renderPass;
    rpBeginInfo.framebuffer = m_texFramebuffer;
    rpBeginInfo.renderArea.extent.width = static_cast<uint32_t>(m_size.width());
    rpBeginInfo.renderArea.extent.height = static_cast<uint32_t>(m_size.height());
    rpBeginInfo.clearValueCount = 2;
    rpBeginInfo.pClearValues = clearValues;

    m_devFuncs->vkCmdBeginRenderPass(m_commandBuffer, &rpBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

#if 0 // TO BE CONSIDERED..
    quint8 *p;
    VkResult err = m_devFuncs->vkMapMemory(m_dev, m_bufMem, m_uniformBufInfo[m_window->graphicsStateInfo().currentFrameSlot].offset,
            UNIFORM_DATA_SIZE, 0, reinterpret_cast<void **>(&p));
    if (err != VK_SUCCESS) {
        qFatal("Failed to map memory: %d", err);
    }

    QMatrix4x4 m = m_proj;
    m.rotate(m_rotation, 0, 0, 1);
    memcpy(p, m.constData(), 16 * sizeof(float));
    m_devFuncs->vkUnmapMemory(m_dev, m_bufMem);

    // Not exactly a real animation system, just advance on every frame for now.
    m_rotation += 1.0f;
#endif

#if 0 // TO BE TROUBLESHOOTED
    m_devFuncs->vkCmdBindPipeline(m_commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline);

    uint currentFrameSlot = m_window->graphicsStateInfo().currentFrameSlot;
    uint32_t dynamicOffset = m_allocPerUbuf * currentFrameSlot;
    m_devFuncs->vkCmdBindDescriptorSets(m_commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout, 0, 1,
                               &m_descSet[currentFrameSlot], 1, &dynamicOffset);

    VkDeviceSize vbOffset = 0;
    m_devFuncs->vkCmdBindVertexBuffers(m_commandBuffer, 0, 1, &m_buf, &vbOffset);

    VkViewport viewport;
    viewport.x = viewport.y = 0;
    viewport.width = static_cast<float>(m_size.width());
    viewport.height = static_cast<float>(m_size.height());
    viewport.minDepth = 0;
    viewport.maxDepth = 1;
    m_devFuncs->vkCmdSetViewport(m_commandBuffer, 0, 1, &viewport);

    VkRect2D scissor;
    scissor.offset.x = scissor.offset.y = 0;
    scissor.extent.width = static_cast<uint32_t>(viewport.width);
    scissor.extent.height = static_cast<uint32_t>(viewport.height);
    m_devFuncs->vkCmdSetScissor(m_commandBuffer, 0, 1, &scissor);

    m_devFuncs->vkCmdDraw(m_commandBuffer, 4, 1, 0, 0);
#endif
    m_devFuncs->vkCmdEndRenderPass(m_commandBuffer);
}

void CustomTextureNode::prepareShader(Stage stage)
{
    QString filename;
    if (stage == VertexStage) {
        filename = QLatin1String(":/squircle.vert.spv");
    } else {
        Q_ASSERT(stage == FragmentStage);
        filename = QLatin1String(":/squircle.frag.spv");
    }
    QFile f(filename);
    if (!f.open(QIODevice::ReadOnly))
        qFatal("Failed to read shader %s", qPrintable(filename));

    const QByteArray contents = f.readAll();

    if (stage == VertexStage) {
        m_vert = contents;
        Q_ASSERT(!m_vert.isEmpty());
    } else {
        m_frag = contents;
        Q_ASSERT(!m_frag.isEmpty());
    }
}

VkShaderModule CustomTextureNode::createShader(const QString& inShaderFilePath)
{
    QFile file(inShaderFilePath);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning("Failed to read shader %s", qPrintable(inShaderFilePath));
        return VK_NULL_HANDLE;
    }
    QByteArray blob = file.readAll();
    file.close();

    VkShaderModuleCreateInfo shaderInfo;
    memset(&shaderInfo, 0, sizeof(shaderInfo));
    shaderInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    shaderInfo.codeSize = static_cast<size_t>(blob.size());
    shaderInfo.pCode = reinterpret_cast<const uint32_t *>(blob.constData());
    VkShaderModule shaderModule;
    VkResult err = m_devFuncs->vkCreateShaderModule(m_dev, &shaderInfo, nullptr, &shaderModule);
    if (err != VK_SUCCESS) {
        qWarning("Failed to create shader module: %d", err);
        return VK_NULL_HANDLE;
    }

    return shaderModule;
}

#include "VulkanSceneGraphTexture.moc"
