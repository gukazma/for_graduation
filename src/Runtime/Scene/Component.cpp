#include "Component.h"
#include <Renderer/VulkanDescriptor.h>
#include <Global/CoreContext.h>
#include <Scene/Asset.h>
#include <Renderer/Texture.h>
namespace GU
{
	MaterialComponent::MaterialComponent(uint64_t modelid, uint64_t textureid)
	{
		material.meshUUID = modelid;
		material.textureUUID = textureid;
		createDescritorSets();
	}
	void MaterialComponent::createDescritorSets()
	{
		material.modelUBO = std::make_shared<::GU::VulkanUniformBuffer<::GU::ModelUBO> >();
		::GU::createDescriptorSets(*GLOBAL_VULKAN_CONTEXT, \
			* GLOBAL_ASSET->getTextureWithUUID(material.textureUUID)->image, \
			material.modelUBO->uniformBuffers, \
			GLOBAL_VULKAN_CONTEXT->descriptorSetLayout, \
			GLOBAL_VULKAN_CONTEXT->descriptorPool, \
			material.descriptorSets);
	}
	void MaterialComponent::destoryUBO()
	{
		material.modelUBO.reset();
	}
	NavMeshComponent::NavMeshComponent(rcPolyMesh* pmesh)
	{

	}
	SkeletalMeshComponent::SkeletalMeshComponent(uint64_t modelid, uint64_t textureid)
	{
		material.skeletalMeshUUID = modelid;
		material.textureUUID = textureid;
		createDescritorSets();
	}
	void SkeletalMeshComponent::createDescritorSets()
	{
		material.modelUBO = std::make_shared<::GU::VulkanUniformBuffer<::GU::SkeletalModelUBO> >();
		::GU::createSkeletalDescriptorSets(*GLOBAL_VULKAN_CONTEXT, \
			* GLOBAL_ASSET->getTextureWithUUID(material.textureUUID)->image, \
			material.modelUBO->uniformBuffers, \
			GLOBAL_VULKAN_CONTEXT->descriptorSetLayout, \
			GLOBAL_VULKAN_CONTEXT->descriptorPool, \
			material.descriptorSets);
	}
	void SkeletalMeshComponent::destoryUBO()
	{
		material.modelUBO.reset();
	}
}

