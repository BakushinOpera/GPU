#pragma once
#include <d3d12.h>
#include <dxgi1_6.h>
#include <d3dcompiler.h>
#include <vector>
#include <unordered_map>
#include <map>
#include <string>

#pragma comment(lib, "d3d12")
#pragma comment(lib, "dxgi")
#pragma comment(lib, "d3dcompiler.lib")

class Barrier {
public:
	static void add(ID3D12Resource* res, D3D12_RESOURCE_STATES first, D3D12_RESOURCE_STATES second,
		ID3D12GraphicsCommandList4* commandList) {
		D3D12_RESOURCE_BARRIER rb = {};
		rb.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		rb.Transition.pResource = res;
		rb.Transition.StateBefore = first;
		rb.Transition.StateAfter = second;
		rb.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		commandList->ResourceBarrier(1, &rb);
	}
};

class GPUFence {
public:
	ID3D12Fence* fence;
	HANDLE eventHandle;
	UINT64 value = 0;
	void create(ID3D12Device5* device) {
		device->CreateFence(value, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence));
		eventHandle = CreateEvent(NULL, FALSE, FALSE, NULL);
	}
	void signal(ID3D12CommandQueue * queue) {
		queue->Signal(fence, ++value);
	}
	void wait() {
		if (fence->GetCompletedValue() < value) {
			fence->SetEventOnCompletion(value, eventHandle);
			WaitForSingleObject(eventHandle, INFINITE);
		}
	}
	~GPUFence() {
		CloseHandle(eventHandle);
		fence->Release();
	}
};

class Core {
public:
	IDXGIAdapter1* adapter;

	ID3D12Device5* device;
	ID3D12CommandQueue* graphicsQueue;
	ID3D12CommandQueue* copyQueue;
	ID3D12CommandQueue* computeQueue;
	IDXGISwapChain3* swapchain;

	ID3D12CommandAllocator* graphicsCommandAllocator[2];
	ID3D12GraphicsCommandList4* graphicsCommandList[2];

	ID3D12DescriptorHeap* backbufferHeap;
	ID3D12Resource** backbuffers;

	GPUFence graphicsQueueFence[2];

	ID3D12DescriptorHeap* dsvHeap;
	ID3D12Resource* dsv;
	D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle;

	D3D12_VIEWPORT viewport;
	D3D12_RECT scissorRect;

	// root signature
	ID3D12RootSignature* rootSignature;

	void init(HWND hwnd, int _width, int _height);
	void ResetCommandList();
	ID3D12GraphicsCommandList4* getCommandList() {
		unsigned int frameIndex = swapchain->GetCurrentBackBufferIndex();
		return graphicsCommandList[frameIndex];
	}
	void runCommandList();
	void flushGraphicsQueue() {
		graphicsQueueFence[0].signal(graphicsQueue);
		graphicsQueueFence[0].wait();
	}

	void uploadResource(ID3D12Resource* dstResource, const void* data, unsigned int size,
		D3D12_RESOURCE_STATES targetState, D3D12_PLACED_SUBRESOURCE_FOOTPRINT* texFootprint = NULL) {
		// allocate memory in upload heap
		ID3D12Resource* uploadBuffer;
		D3D12_HEAP_PROPERTIES heapProps = {};
		heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;
		D3D12_RESOURCE_DESC bufferDesc = {};
		bufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		bufferDesc.Width = size;
		bufferDesc.Height = 1;
		bufferDesc.DepthOrArraySize = 1;
		bufferDesc.MipLevels = 1;
		bufferDesc.SampleDesc.Count = 1;
		bufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &bufferDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ, NULL, IID_PPV_ARGS(&uploadBuffer));

		// get pointer to allocated memory on upload heap (Map)
		void* mappeddata = NULL;
		uploadBuffer->Map(0, NULL, &mappeddata);
		// memcpy vertex data (or any data)
		memcpy(mappeddata, data, size);
		// tell driver we are done (Unmap)
		uploadBuffer->Unmap(0, NULL);

		ResetCommandList();

		// issue copy command
		if (texFootprint != NULL) {
			D3D12_TEXTURE_COPY_LOCATION src = {};
			src.pResource = uploadBuffer;
			src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
			src.PlacedFootprint = *texFootprint;
			D3D12_TEXTURE_COPY_LOCATION dst = {};
			dst.pResource = dstResource;
			dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
			dst.SubresourceIndex = 0;
			getCommandList()->CopyTextureRegion(&dst, 0, 0, 0, &src, NULL);
		} else {
			getCommandList()->CopyBufferRegion(dstResource, 0, uploadBuffer, 0, size);
		}
		Barrier::add(dstResource, D3D12_RESOURCE_STATE_COPY_DEST, targetState, getCommandList());
		runCommandList();
		flushGraphicsQueue();
		uploadBuffer->Release();
	}

	void beginFrame() {
		unsigned int frameIndex = swapchain->GetCurrentBackBufferIndex();
		graphicsQueueFence[frameIndex].wait();
		D3D12_CPU_DESCRIPTOR_HANDLE renderTargetViewHandle = backbufferHeap-> GetCPUDescriptorHandleForHeapStart();

		unsigned int renderTargetViewDescriptorSize = device-> GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
		renderTargetViewHandle.ptr += frameIndex * renderTargetViewDescriptorSize;
		
		ResetCommandList();
		Barrier::add(backbuffers[frameIndex], D3D12_RESOURCE_STATE_PRESENT,
			D3D12_RESOURCE_STATE_RENDER_TARGET, getCommandList());
		getCommandList()->OMSetRenderTargets(1, &renderTargetViewHandle, FALSE, &dsvHandle);
		float color[4];
		color[0] = 0;
		color[1] = 0;
		color[2] = 1.0;
		color[3] = 1.0;
		getCommandList()->ClearRenderTargetView(renderTargetViewHandle, color, 0, NULL);
		getCommandList()->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, NULL);
	}
	void finishFrame() {
		unsigned int frameIndex = swapchain->GetCurrentBackBufferIndex();
		Barrier::add(backbuffers[frameIndex], D3D12_RESOURCE_STATE_RENDER_TARGET,
			D3D12_RESOURCE_STATE_PRESENT, getCommandList());
		runCommandList();
		graphicsQueueFence[frameIndex].signal(graphicsQueue);
		swapchain->Present(1, 0);
	}

	// to set common draw functionality
	// can be put in beginFrame() but sometimes may need it to do sth else
	void beginRenderPass() {
		getCommandList()->RSSetViewports(1, &viewport);
		getCommandList()->RSSetScissorRects(1, &scissorRect);
		getCommandList()->SetGraphicsRootSignature(rootSignature);
	}
	int frameIndex() {
		return swapchain->GetCurrentBackBufferIndex();
	}
};

class PSOManager {
public:
	std::unordered_map<std::string, ID3D12PipelineState*> psos;

	// call after creating shaders and layout
	void createPSO(Core* core, std::string name, ID3DBlob* vs, ID3DBlob* ps, D3D12_INPUT_LAYOUT_DESC layout) {
		// check if PSO in map, avoiding creating extra state
		if (psos.find(name) != psos.end()) return;

		// configure GPU pipeline with shaders, layout and Root Signature
		D3D12_GRAPHICS_PIPELINE_STATE_DESC desc = {};
		desc.InputLayout = layout;
		desc.pRootSignature = core->rootSignature;
		desc.VS = { vs->GetBufferPointer(), vs->GetBufferSize() };
		desc.PS = { ps->GetBufferPointer(), ps->GetBufferSize() };

		// responsible for configuring the rasterizer
		D3D12_RASTERIZER_DESC rasterDesc = {};
		rasterDesc.FillMode = D3D12_FILL_MODE_SOLID;
		rasterDesc.CullMode = D3D12_CULL_MODE_NONE;
		rasterDesc.FrontCounterClockwise = FALSE;
		rasterDesc.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
		rasterDesc.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
		rasterDesc.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
		rasterDesc.DepthClipEnable = TRUE;
		rasterDesc.MultisampleEnable = FALSE;
		rasterDesc.AntialiasedLineEnable = FALSE;
		rasterDesc.ForcedSampleCount = 0;
		rasterDesc.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;
		desc.RasterizerState = rasterDesc;

		// responsible for configuring the depth buffer
		D3D12_DEPTH_STENCIL_DESC depthStencilDesc = {};
		depthStencilDesc.DepthEnable = TRUE;
		depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
		depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
		depthStencilDesc.StencilEnable = FALSE;
		desc.DepthStencilState = depthStencilDesc;

		D3D12_BLEND_DESC blendDesc = {};
		blendDesc.AlphaToCoverageEnable = FALSE;
		blendDesc.IndependentBlendEnable = FALSE;
		const D3D12_RENDER_TARGET_BLEND_DESC defaultRenderTargetBlend = {
			FALSE, FALSE,
			D3D12_BLEND_ONE, D3D12_BLEND_ZERO, D3D12_BLEND_OP_ADD,
			D3D12_BLEND_ONE, D3D12_BLEND_ZERO, D3D12_BLEND_OP_ADD,
			D3D12_LOGIC_OP_NOOP,
			D3D12_COLOR_WRITE_ENABLE_ALL 
		};
		for (int i = 0; i < D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT; i++) {
			blendDesc.RenderTarget[i] = defaultRenderTargetBlend;
		}
		desc.BlendState = blendDesc;

		// render target state + topology
		desc.SampleMask = UINT_MAX;
		desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		desc.NumRenderTargets = 1;
		desc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
		desc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
		desc.SampleDesc.Count = 1;

		ID3D12PipelineState* pso;
		HRESULT hr = core->device->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&pso));
		psos.insert({ name, pso });

	}
	void bind(Core* core, std::string name) {
		core->getCommandList()->SetPipelineState(psos[name]);
	}
};

//struct ConstantBufferVariable {
//	unsigned int offset;
//	unsigned int size;
//};
class ConstantBuffer {
public:
	ID3D12Resource* constantBuffer;
	unsigned char* buffer;
	unsigned int cbSizeInBytes;

	//std::string name;
	//std::map<std::string, ConstantBufferVariable> constantBufferData;

	void init(Core* core, unsigned int sizeInBytes, int frames) {
		cbSizeInBytes = (sizeInBytes + 255) & ~255;
		HRESULT hr;
		D3D12_HEAP_PROPERTIES heapprops = {};
		heapprops.Type = D3D12_HEAP_TYPE_UPLOAD;
		heapprops.CreationNodeMask = 1;
		heapprops.VisibleNodeMask = 1;
		D3D12_RESOURCE_DESC cbDesc = {};
		cbDesc.Width = cbSizeInBytes * frames;
		cbDesc.Height = 1;
		cbDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		cbDesc.DepthOrArraySize = 1;
		cbDesc.MipLevels = 1;
		cbDesc.SampleDesc.Count = 1;
		cbDesc.SampleDesc.Quality = 0;
		cbDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		hr = core->device->CreateCommittedResource(&heapprops, D3D12_HEAP_FLAG_NONE, &cbDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ, NULL, __uuidof(ID3D12Resource), (void**)&constantBuffer);
		hr = constantBuffer->Map(0, NULL, (void**)&buffer);
	}
	void update(void* data, unsigned int sizeInBytes, int frame) {
		memcpy(buffer + (frame * cbSizeInBytes), data, sizeInBytes);
	}
	D3D12_GPU_VIRTUAL_ADDRESS getGPUAddress(int frame) {
		return (constantBuffer->GetGPUVirtualAddress() + (frame * cbSizeInBytes));
	}
};