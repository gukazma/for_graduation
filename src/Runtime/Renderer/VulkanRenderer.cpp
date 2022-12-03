#include "VulkanRenderer.h"
#include <QVulkanDeviceFunctions>
#include <Renderer/VulkanShader.h>
#include "shader_texture_frag.h"
#include "shader_texture_vert.h"

#include <Renderer/VulkanGraphicsPipeline.h>
#include <Renderer/VulkanBuffer.h>
#include <Renderer/VulkanDescriptor.h>
#include <Renderer/VulkanImage.h>
#include <Renderer/Mesh.h>
#include <Scene/Entity.h>
#include <Renderer/VulkanUniformBuffer.hpp>
namespace GU
{
	VulkanUniformBuffer<CameraUBO>* aaa;

	VulkanRenderer::VulkanRenderer(QVulkanWindow* w)
		:m_window(w)
	{
	}

	void VulkanRenderer::initResources()
	{
		qDebug("initResources");

		m_devFuncs = m_window->vulkanInstance()->deviceFunctions(m_window->device());
		GLOBAL_VULKAN_CONTEXT->physicalDevice = m_window->physicalDevice();
		GLOBAL_VULKAN_CONTEXT->logicalDevice = m_window->device();
		GLOBAL_VULKAN_CONTEXT->commandPool = m_window->graphicsCommandPool();
		GLOBAL_VULKAN_CONTEXT->graphicsQueue = m_window->graphicsQueue();
		GLOBAL_VULKAN_CONTEXT->renderPass = m_window->defaultRenderPass();
		
		VkShaderModule vertexShader = createShader(m_window->device(), shader_texture_vert, sizeof(shader_texture_vert));
		VkShaderModule fragShader = createShader(m_window->device(), shader_texture_frag, sizeof(shader_texture_frag));
		createShaderStageInfo(vertexShader, fragShader, GLOBAL_VULKAN_CONTEXT->shaderStage);
		// descript binding point
		createDescriptorSetLayout(*GLOBAL_VULKAN_CONTEXT, GLOBAL_VULKAN_CONTEXT->descriptorSetLayout);
		// create graphicspipeline according to descriptor and qt default pipelinelayout
		createPipelineLayout(*GLOBAL_VULKAN_CONTEXT, GLOBAL_VULKAN_CONTEXT->descriptorSetLayout, GLOBAL_VULKAN_CONTEXT->pipelineLayout);
		// create graphicspipeline according to layout
		createGraphicsPipeline(*GLOBAL_VULKAN_CONTEXT, GLOBAL_VULKAN_CONTEXT->shaderStage, GLOBAL_VULKAN_CONTEXT->pipelineLayout, GLOBAL_VULKAN_CONTEXT->graphicsPipeline);

		VulkanImage vkImage;
		createTextureImage(*GLOBAL_VULKAN_CONTEXT, "./assets/grid.jpg", vkImage);
		createTextureImageView(*GLOBAL_VULKAN_CONTEXT, vkImage);
		createTextureSampler(*GLOBAL_VULKAN_CONTEXT, vkImage);
		aaa = new VulkanUniformBuffer<CameraUBO>();
		GLOBAL_VULKAN_CONTEXT->uniformBuffers = aaa->uniformBuffers;
		GLOBAL_VULKAN_CONTEXT->uniformBuffersMemory = aaa->uniformBuffersMemory;
		GLOBAL_VULKAN_CONTEXT->uniformBuffersMapped = aaa->uniformBuffersMapped;
		//createUniformBuffers(*GLOBAL_VULKAN_CONTEXT, GLOBAL_VULKAN_CONTEXT->uniformBuffers, GLOBAL_VULKAN_CONTEXT->uniformBuffersMemory, GLOBAL_VULKAN_CONTEXT->uniformBuffersMapped, sizeof(CameraUBO));
		createUniformBuffers(*GLOBAL_VULKAN_CONTEXT, GLOBAL_VULKAN_CONTEXT->meshUniformBuffers, GLOBAL_VULKAN_CONTEXT->meshUniformBuffersMemory, GLOBAL_VULKAN_CONTEXT->meshUniformBuffersMapped, sizeof(ModelUBO));
		createDescriptorPool(*GLOBAL_VULKAN_CONTEXT, GLOBAL_VULKAN_CONTEXT->descriptorPool);
		createDescriptorSets(*GLOBAL_VULKAN_CONTEXT, vkImage, GLOBAL_VULKAN_CONTEXT->descriptorSetLayout, GLOBAL_VULKAN_CONTEXT->descriptorPool, GLOBAL_VULKAN_CONTEXT->descriptorSets);
		createBackgroundPipeline(*GLOBAL_VULKAN_CONTEXT, GLOBAL_VULKAN_CONTEXT->backgroudPipeline);
		createBoundingBoxPipeline(*GLOBAL_VULKAN_CONTEXT, GLOBAL_VULKAN_CONTEXT->boundingboxPipeline);
	}

	void VulkanRenderer::initSwapChainResources()
	{
		qDebug("initSwapChainResources");
	}

	void VulkanRenderer::releaseSwapChainResources()
	{
		qDebug("releaseSwapChainResources");
	}

	void VulkanRenderer::releaseResources()
	{
		qDebug("releaseResources");
	}

	void VulkanRenderer::startNextFrame()
	{
		GLOBAL_TIME_TICK();
		g_CoreContext.g_winWidth = m_window->swapChainImageSize().width();
		g_CoreContext.g_winHeight = m_window->swapChainImageSize().height();
		m_Camera.updateView();
		m_Camera.updateProjection();
		GLOBAL_VULKAN_CONTEXT->swapChainExtent = { (unsigned int)m_window->swapChainImageSize().width(),(unsigned int)m_window->swapChainImageSize().height() };
		aaa->update({ m_Camera.getViewMatrix(), m_Camera.getProjectionMatrix() }, m_window->currentSwapChainImageIndex());
		const QSize sz = m_window->swapChainImageSize();
		VkClearColorValue clearColor = { { 0.0f, 0.0f, 0.0f, 1.0f } };
		VkClearDepthStencilValue clearDS = { 1.0f, 0.0f };
		VkClearValue clearValues[2];
		memset(clearValues, 0, sizeof(clearValues));
		clearValues[0].color = clearColor;
		clearValues[1].depthStencil = clearDS;

		VkRenderPassBeginInfo rpBeginInfo;
		memset(&rpBeginInfo, 0, sizeof(rpBeginInfo));
		rpBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		rpBeginInfo.renderPass = m_window->defaultRenderPass();
		rpBeginInfo.framebuffer = m_window->currentFramebuffer();
		rpBeginInfo.renderArea.extent.width = sz.width();
		rpBeginInfo.renderArea.extent.height = sz.height();
		rpBeginInfo.clearValueCount = 2;
		rpBeginInfo.pClearValues = clearValues;

		// dynamic state
		VkViewport viewport{};
		viewport.x = 0.0f;
		viewport.y = 0.0f;
		viewport.width = (float)sz.width();
		viewport.height = (float)sz.height();
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;
		VkRect2D scissor{};
		scissor.offset = { 0, 0 };
		scissor.extent.width = viewport.width;
		scissor.extent.height = viewport.height;

		VkCommandBuffer cmdBuf = m_window->currentCommandBuffer();
		// prepare cammondbuffer
		m_devFuncs->vkCmdBeginRenderPass(cmdBuf, &rpBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
		m_devFuncs->vkCmdSetViewport(cmdBuf, 0, 1, &viewport);
		m_devFuncs->vkCmdSetScissor(cmdBuf, 0, 1, &scissor);

		// BackgroundColor
		m_devFuncs->vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, GLOBAL_VULKAN_CONTEXT->backgroudPipeline);
		m_devFuncs->vkCmdDraw(cmdBuf, 6, 1, 0, 0);

		// Wireframe
		m_devFuncs->vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, GLOBAL_VULKAN_CONTEXT->boundingboxPipeline);
		m_devFuncs->vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, GLOBAL_VULKAN_CONTEXT->pipelineLayout, 0, 1, &GLOBAL_VULKAN_CONTEXT->descriptorSets[m_window->currentSwapChainImageIndex()], 0, nullptr);
		m_devFuncs->vkCmdDraw(cmdBuf, 24, 1, 0, 0);

		// 3D model
		GLOBAL_SCENE->renderTick(*GLOBAL_VULKAN_CONTEXT, cmdBuf, m_window->currentSwapChainImageIndex(), GLOBAL_DELTATIME);

		// submit queue
		m_devFuncs->vkCmdEndRenderPass(cmdBuf);
		m_window->frameReady();
		m_window->requestUpdate();
	}
}