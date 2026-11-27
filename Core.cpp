#include "Core.h"
extern "C"{
	_declspec(dllexport) DWORD NvOptimusEnablement = 0x00000001;
}


// Core class
void Core::init(HWND hwnd, int _width, int _height) {
	// debug output
	ID3D12Debug1* debug;
	D3D12GetDebugInterface(IID_PPV_ARGS(&debug));
	debug->EnableDebugLayer();
	debug->Release();

	IDXGIAdapter1* adapterf;
	std::vector<IDXGIAdapter1*> adapters;
	IDXGIFactory6* factory = NULL;
	CreateDXGIFactory(__uuidof(IDXGIFactory6), (void**)&factory);
	int i = 0;
	while (factory->EnumAdapters1(i, &adapterf) != DXGI_ERROR_NOT_FOUND) {
		adapters.push_back(adapterf);
		i++;
	}
	long long maxVideoMemory = 0;
	int useAdapterIndex = 0;
	for (int i = 0; i < adapters.size(); i++) {
		DXGI_ADAPTER_DESC desc;
		adapters[i]->GetDesc(&desc);
		if (desc.DedicatedVideoMemory > maxVideoMemory) {
			maxVideoMemory = desc.DedicatedVideoMemory;
			useAdapterIndex = i;
		}
	}
	adapter = adapters[useAdapterIndex];

	// create the device
	D3D12CreateDevice(adapter, D3D_FEATURE_LEVEL_12_1, IID_PPV_ARGS(&device));

	// create command queues
	D3D12_COMMAND_QUEUE_DESC graphicsQueueDesc = {};
	graphicsQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	device->CreateCommandQueue(&graphicsQueueDesc, IID_PPV_ARGS(&graphicsQueue));
	D3D12_COMMAND_QUEUE_DESC copyQueueDesc = {};
	copyQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_COPY;
	device->CreateCommandQueue(&copyQueueDesc, IID_PPV_ARGS(&copyQueue));
	D3D12_COMMAND_QUEUE_DESC computeQueueDesc = {};
	computeQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_COMPUTE;
	device->CreateCommandQueue(&computeQueueDesc, IID_PPV_ARGS(&computeQueue));

	// SwapChain
	DXGI_SWAP_CHAIN_DESC1 scDesc = {};
	scDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	scDesc.Width = _width;
	scDesc.Height = _height;
	scDesc.SampleDesc.Count = 1; // MSAA here
	scDesc.SampleDesc.Quality = 0;
	scDesc.BufferCount = 2;
	scDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	IDXGISwapChain1* swapChain1;
	factory->CreateSwapChainForHwnd(graphicsQueue, hwnd, &scDesc, NULL, NULL, &swapChain1);
	swapChain1->QueryInterface(&swapchain);
	swapChain1->Release();
	factory->Release();

	// create Command Allocators and Conmand Lists
	device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
		IID_PPV_ARGS(&graphicsCommandAllocator[0]));
	device->CreateCommandList1(0, D3D12_COMMAND_LIST_TYPE_DIRECT, D3D12_COMMAND_LIST_FLAG_NONE,
		IID_PPV_ARGS(&graphicsCommandList[0]));

	device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
		IID_PPV_ARGS(&graphicsCommandAllocator[1]));
	device->CreateCommandList1(0, D3D12_COMMAND_LIST_TYPE_DIRECT, D3D12_COMMAND_LIST_FLAG_NONE,
		IID_PPV_ARGS(&graphicsCommandList[1]));

	// create heap
	D3D12_DESCRIPTOR_HEAP_DESC renderTargetViewHeapDesc = {};
	renderTargetViewHeapDesc.NumDescriptors = scDesc.BufferCount;
	renderTargetViewHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	device->CreateDescriptorHeap(&renderTargetViewHeapDesc, IID_PPV_ARGS(&backbufferHeap));

	backbuffers = new ID3D12Resource * [scDesc.BufferCount];
	D3D12_CPU_DESCRIPTOR_HANDLE renderTargetViewHandle = backbufferHeap->GetCPUDescriptorHandleForHeapStart();
	unsigned int renderTargetViewDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	for (unsigned int i = 0; i < 2; i++) {
		swapchain->GetBuffer(i, IID_PPV_ARGS(&backbuffers[i]));
		D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
		rtvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
		rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
		device->CreateRenderTargetView(backbuffers[i], &rtvDesc, renderTargetViewHandle);
		renderTargetViewHandle.ptr += renderTargetViewDescriptorSize;
	}

	// create fences
	graphicsQueueFence[0].create(device);
	graphicsQueueFence[1].create(device);

	// create Descriptor Heap
	D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc;
	memset(&dsvHeapDesc, 0, sizeof(D3D12_DESCRIPTOR_HEAP_DESC));
	dsvHeapDesc.NumDescriptors = 1;
	dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
	dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	device->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(&dsvHeap));
	dsvHandle = dsvHeap->GetCPUDescriptorHandleForHeapStart();

	// create Depth Buffer
	D3D12_DEPTH_STENCIL_VIEW_DESC depthStencilDesc = {};
	depthStencilDesc.Format = DXGI_FORMAT_D32_FLOAT;
	depthStencilDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
	depthStencilDesc.Flags = D3D12_DSV_FLAG_NONE;
	D3D12_CLEAR_VALUE depthClearValue = {};
	depthClearValue.Format = DXGI_FORMAT_D32_FLOAT;
	depthClearValue.DepthStencil.Depth = 1.0f;
	depthClearValue.DepthStencil.Stencil = 0;

	D3D12_HEAP_PROPERTIES heapprops = {};
	heapprops.Type = D3D12_HEAP_TYPE_DEFAULT;
	heapprops.CreationNodeMask = 1;
	heapprops.VisibleNodeMask = 1;

	D3D12_RESOURCE_DESC dsvDesc = {};
	dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
	dsvDesc.Width = _width;
	dsvDesc.Height = _height;
	dsvDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	dsvDesc.DepthOrArraySize = 1;
	dsvDesc.MipLevels = 1;
	dsvDesc.SampleDesc.Count = 1;
	dsvDesc.SampleDesc.Quality = 0;
	dsvDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
	dsvDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;

	device->CreateCommittedResource(&heapprops, D3D12_HEAP_FLAG_NONE, &dsvDesc,
		D3D12_RESOURCE_STATE_DEPTH_WRITE, &depthClearValue, IID_PPV_ARGS(&dsv));
	device->CreateDepthStencilView(dsv, &depthStencilDesc, dsvHandle);

	viewport.TopLeftX = 0.0f;
	viewport.TopLeftY = 0.0f;
	viewport.Width = (float)_width;
	viewport.Height = (float)_height;
	viewport.MinDepth = 0.0f;
	viewport.MaxDepth = 1.0f;
	scissorRect.left = 0;
	scissorRect.top = 0;
	scissorRect.right = _width;
	scissorRect.bottom = _height;

	// create root signature -- allows drawing
	// update root signature
	std::vector<D3D12_ROOT_PARAMETER> parameters;
	D3D12_ROOT_PARAMETER rootParameterCBVS;
	rootParameterCBVS.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameterCBVS.Descriptor.ShaderRegister = 0; // Register(b0)
	rootParameterCBVS.Descriptor.RegisterSpace = 0;
	rootParameterCBVS.ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
	parameters.push_back(rootParameterCBVS);

	D3D12_ROOT_PARAMETER rootParameterCBPS;
	rootParameterCBPS.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameterCBPS.Descriptor.ShaderRegister = 0; // Register(b0)
	rootParameterCBPS.Descriptor.RegisterSpace = 0;
	rootParameterCBPS.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	parameters.push_back(rootParameterCBPS);

	D3D12_ROOT_SIGNATURE_DESC desc = {};
	desc.NumParameters = parameters.size();
	desc.pParameters = &parameters[0];
	desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

	ID3DBlob* serialized;
	ID3DBlob* error;
	D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &serialized, &error);

	device->CreateRootSignature(0, serialized->GetBufferPointer(), serialized->GetBufferSize(), IID_PPV_ARGS(&rootSignature));
	serialized->Release();

}

void Core::ResetCommandList() {
	unsigned int frameIndex = swapchain->GetCurrentBackBufferIndex();
	graphicsCommandAllocator[frameIndex]->Reset();
	graphicsCommandList[frameIndex]->Reset(graphicsCommandAllocator[frameIndex], NULL);
}
void Core::runCommandList() {
	getCommandList()->Close();
	ID3D12CommandList* lists[] = { getCommandList() };
	graphicsQueue->ExecuteCommandLists(1, lists);
}

void Core::uploadResource(ID3D12Resource* dstResource, const void* data, unsigned int size,
	D3D12_RESOURCE_STATES targetState, D3D12_PLACED_SUBRESOURCE_FOOTPRINT* texFootprint) {
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
	}
	else {
		getCommandList()->CopyBufferRegion(dstResource, 0, uploadBuffer, 0, size);
	}
	Barrier::add(dstResource, D3D12_RESOURCE_STATE_COPY_DEST, targetState, getCommandList());
	runCommandList();
	// cpu wait until GPU finishes work
	flushGraphicsQueue();
	uploadBuffer->Release();
}
void Core::beginFrame() {
	graphicsQueueFence[frameIndex()].wait();
	D3D12_CPU_DESCRIPTOR_HANDLE renderTargetViewHandle = backbufferHeap->GetCPUDescriptorHandleForHeapStart();

	unsigned int renderTargetViewDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	renderTargetViewHandle.ptr += frameIndex() * renderTargetViewDescriptorSize;

	ResetCommandList();
	Barrier::add(backbuffers[frameIndex()], D3D12_RESOURCE_STATE_PRESENT,
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
void Core::finishFrame() {
	unsigned int frameIndex = swapchain->GetCurrentBackBufferIndex();
	Barrier::add(backbuffers[frameIndex], D3D12_RESOURCE_STATE_RENDER_TARGET,
		D3D12_RESOURCE_STATE_PRESENT, getCommandList());
	runCommandList();
	graphicsQueueFence[frameIndex].signal(graphicsQueue);
	swapchain->Present(1, 0);
}

// PSOManager
void PSOManager::createPSO(Core* core, std::string name, ID3DBlob* vs, ID3DBlob* ps, D3D12_INPUT_LAYOUT_DESC layout) {
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

// Shaders class
void Shaders::reflect(Core* core, ID3DBlob* shader, std::vector<ConstantBuffer>& targetBuffers) {
	ID3D12ShaderReflection* reflection;
	D3DReflect(shader->GetBufferPointer(), shader->GetBufferSize(), IID_PPV_ARGS(&reflection));
	D3D12_SHADER_DESC desc;
	reflection->GetDesc(&desc);

	for (int i = 0;i < desc.ConstantBuffers;i++) {
		ConstantBuffer buffer;
		ID3D12ShaderReflectionConstantBuffer* constantBuffer = reflection->GetConstantBufferByIndex(i);
		D3D12_SHADER_BUFFER_DESC cbDesc;
		constantBuffer->GetDesc(&cbDesc);
		buffer.cbuffname = cbDesc.Name;
		for (int j = 0;j < cbDesc.Variables;j++) {
			ID3D12ShaderReflectionVariable* var = constantBuffer->GetVariableByIndex(j);
			D3D12_SHADER_VARIABLE_DESC vDesc;
			var->GetDesc(&vDesc);
			ConstantBufferVariable bufferVariable;
			bufferVariable.offset = vDesc.StartOffset;
			bufferVariable.size = vDesc.Size;
			buffer.constantBufferData.insert({ vDesc.Name, bufferVariable });
		}
		buffer.init(core, cbDesc.Size);
		targetBuffers.push_back(buffer);
	}
	reflection->Release();
}
