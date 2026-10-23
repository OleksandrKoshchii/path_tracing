#include "vulkan_context.hpp"

void VulkanContext::drawFrame(PipelineType type)
{
    auto fenceResult = logicalDevice_.waitForFences(*inFlightFences_[frameIndex], vk::True, UINT64_MAX);
	if (fenceResult != vk::Result::eSuccess)
	{
		throw std::runtime_error("failed to wait for fence!");
	}
	logicalDevice_.resetFences(*inFlightFences_[frameIndex]);

    auto [result, imageIndex] = swapChain_.acquireNextImage(UINT64_MAX, *presentCompleteSemaphores_[frameIndex], nullptr);

    commandBuffers_[frameIndex].reset();

    vk::PipelineStageFlags waitDestinationStageMask;

    switch(type)
    {
        case PipelineType::Graphics:
            recordGraphicsPipelineCommandBuffer(imageIndex);
            waitDestinationStageMask = ( vk::PipelineStageFlagBits::eColorAttachmentOutput );
            break;
        case PipelineType::Compute:
            recordComputePipelineCommandBuffer(imageIndex);
            waitDestinationStageMask = ( vk::PipelineStageFlagBits::eComputeShader );
            break;
    }

    const vk::SubmitInfo   submitInfo
    {
        .waitSemaphoreCount   = 1,
        .pWaitSemaphores      = &*presentCompleteSemaphores_[frameIndex],
        .pWaitDstStageMask    = &waitDestinationStageMask,
        .commandBufferCount   = 1,
        .pCommandBuffers      = &*commandBuffers_[frameIndex],
        .signalSemaphoreCount = 1,
        .pSignalSemaphores    = &*renderFinishedSemaphores_[imageIndex]
    };

    graphicsAndComputeQueue_.submit(submitInfo, *inFlightFences_[frameIndex]);

    const vk::PresentInfoKHR presentInfoKHR{.waitSemaphoreCount = 1, .pWaitSemaphores = &*renderFinishedSemaphores_[imageIndex], .swapchainCount = 1, .pSwapchains = &*swapChain_, .pImageIndices = &imageIndex};
	result = graphicsAndComputeQueue_.presentKHR(presentInfoKHR);
	switch (result)
	{
		case vk::Result::eSuccess:
			break;
		case vk::Result::eSuboptimalKHR:
			std::cout << "vk::Queue::presentKHR returned vk::Result::eSuboptimalKHR !\n";
			break;
		default:
			break;        // an unexpected result is returned!
	}

    frameIndex = (frameIndex + 1) % MAX_FRAMES_IN_FLIGHT;
}

void VulkanContext::recordGraphicsPipelineCommandBuffer(uint32_t imageIndex)
{
    auto &commandBuffer = commandBuffers_[frameIndex];

	commandBuffer.begin({});

	// Before starting rendering, transition the swapchain image to vk::ImageLayout::eColorAttachmentOptimal
	transition_image_layout(
	    imageIndex,
	    vk::ImageLayout::eUndefined,
	    vk::ImageLayout::eColorAttachmentOptimal,
	    {},                                                        // srcAccessMask (no need to wait for previous operations)
	    vk::AccessFlagBits2::eColorAttachmentWrite,                // dstAccessMask
	    vk::PipelineStageFlagBits2::eColorAttachmentOutput,        // srcStage
	    vk::PipelineStageFlagBits2::eColorAttachmentOutput         // dstStage
	);
	vk::ClearValue              clearColor     = vk::ClearColorValue(0.0f, 0.0f, 0.0f, 1.0f);
	vk::RenderingAttachmentInfo attachmentInfo = {
	    .imageView   = swapChainImageViews_[imageIndex],
	    .imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
	    .loadOp      = vk::AttachmentLoadOp::eClear,
	    .storeOp     = vk::AttachmentStoreOp::eStore,
	    .clearValue  = clearColor};
	vk::RenderingInfo renderingInfo = {
	    .renderArea           = {.offset = {0, 0}, .extent = swapChainExtent_},
	    .layerCount           = 1,
	    .colorAttachmentCount = 1,
	    .pColorAttachments    = &attachmentInfo};

	commandBuffer.beginRendering(renderingInfo);
	commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, *graphicsPipeline_);
	commandBuffer.setViewport(0, vk::Viewport(0.0f, 0.0f, static_cast<float>(swapChainExtent_.width), static_cast<float>(swapChainExtent_.height), 0.0f, 1.0f));
	commandBuffer.setScissor(0, vk::Rect2D(vk::Offset2D(0, 0), swapChainExtent_));
	commandBuffer.draw(3, 1, 0, 0);
	commandBuffer.endRendering();

		// After rendering, transition the swapchain image to vk::ImageLayout::ePresentSrcKHR
	transition_image_layout(
	    imageIndex,
	    vk::ImageLayout::eColorAttachmentOptimal,
	    vk::ImageLayout::ePresentSrcKHR,
	    vk::AccessFlagBits2::eColorAttachmentWrite,                // srcAccessMask
	    {},                                                        // dstAccessMask
	    vk::PipelineStageFlagBits2::eColorAttachmentOutput,        // srcStage
	    vk::PipelineStageFlagBits2::eBottomOfPipe                  // dstStage
	);
	commandBuffer.end();
}

void VulkanContext::recordComputePipelineCommandBuffer(uint32_t imageIndex)
{
    auto &commandBuffer = commandBuffers_[frameIndex];

    commandBuffer.begin({});

    transition_image_layout(
        imageIndex,
        vk::ImageLayout::eUndefined,                               // we don't care about previous result
        vk::ImageLayout::eGeneral,                                 // to RWTexture2D
        {},                                                        // wait nothing
        vk::AccessFlagBits2::eShaderStorageWrite,                  // want to write in a compute shader
        vk::PipelineStageFlagBits2::eComputeShader,                // srcStage
        vk::PipelineStageFlagBits2::eComputeShader                 // dstStage
    );

    commandBuffer.bindPipeline(vk::PipelineBindPoint::eCompute, *computePipeline_);

    commandBuffer.bindDescriptorSets(
        vk::PipelineBindPoint::eCompute, 
        *computePipelineLayout_,
        0,                               
        *descriptorSets_[imageIndex],
        nullptr
    );

    commandBuffer.dispatch(
        static_cast<uint32_t>(ceil(800 / 8)),
        static_cast<uint32_t>(ceil(600 / 8)),\
        1); //TODO : RAPLACE BY A CONSTANT VAL

    // to ePresentSrcKHR
    transition_image_layout(
        imageIndex,
        vk::ImageLayout::eGeneral,                                 // from RWTexture2D
        vk::ImageLayout::ePresentSrcKHR,                           // to present mode
        vk::AccessFlagBits2::eShaderStorageWrite,                  // cache flushes to VRAM
        {},                                                        
        vk::PipelineStageFlagBits2::eComputeShader,                // gpu wait until Compute shader finish it's work
        vk::PipelineStageFlagBits2::eBottomOfPipe
    );

    commandBuffer.end();
}

void VulkanContext::transition_image_layout(
    uint32_t                imageIndex,
    vk::ImageLayout         old_layout,
    vk::ImageLayout         new_layout,
    vk::AccessFlags2        src_access_mask,
    vk::AccessFlags2        dst_access_mask,
    vk::PipelineStageFlags2 src_stage_mask,
    vk::PipelineStageFlags2 dst_stage_mask)
{
	vk::ImageMemoryBarrier2 barrier = 
    {
	    .srcStageMask        = src_stage_mask,  //what stage did work with our resource last time
	    .srcAccessMask       = src_access_mask, //what kind of work was that
	    .dstStageMask        = dst_stage_mask,  //what stage requires our resource
	    .dstAccessMask       = dst_access_mask, //what kind of work does it require to do
	    .oldLayout           = old_layout,      //transforms the strcture of an image(from) (eUndefined if we do not care)
	    .newLayout           = new_layout,      //(to)
	    .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
	    .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
	    .image               = swapChainImages_[imageIndex],
	    .subresourceRange    = 
        {
	        .aspectMask     = vk::ImageAspectFlagBits::eColor,
	        .baseMipLevel   = 0,
	        .levelCount     = 1,
	        .baseArrayLayer = 0,
	        .layerCount     = 1}
    };
	vk::DependencyInfo dependency_info = 
    {
	    .dependencyFlags         = {},
	    .imageMemoryBarrierCount = 1,
	    .pImageMemoryBarriers    = &barrier
    };

    auto &commandBuffer = commandBuffers_[frameIndex];

    commandBuffer.pipelineBarrier2(dependency_info);
}