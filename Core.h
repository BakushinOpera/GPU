#pragma once
#include <d3d12.h>
#include <dxgi1_6.h>
#include <d3dcompiler.h>
#include <vector>
#include <unordered_map>
#include <map>
#include <fstream>
#include <sstream>
#include <string>

#pragma comment(lib, "d3d12")
#pragma comment(lib, "dxgi")
#pragma comment(lib, "d3dcompiler.lib")

struct ConstantBufferVariable {
	unsigned int offset;
	unsigned int size;
};

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

	void uploadResource(ID3D12Resource* dstResource, const void* data, unsigned int size, D3D12_RESOURCE_STATES targetState, D3D12_PLACED_SUBRESOURCE_FOOTPRINT* texFootprint = NULL);

	void beginFrame();
	void finishFrame();

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
	void createPSO(Core* core, std::string name, ID3DBlob* vs, ID3DBlob* ps, D3D12_INPUT_LAYOUT_DESC layout);
	void bind(Core* core, std::string name) {
		core->getCommandList()->SetPipelineState(psos[name]);
	}
};

class ConstantBuffer {
public:
	ID3D12Resource* constantBuffer;
	unsigned char* buffer;
	unsigned int cbSizeInBytes;
	unsigned int maxDrawCalls; // NumInstances
	unsigned int offsetIndex;

	std::string cbuffname;
	std::map<std::string, ConstantBufferVariable> constantBufferData;

	void init(Core* core, unsigned int sizeInBytes, unsigned int _maxDrawCalls = 1024) {
		cbSizeInBytes = (sizeInBytes + 255) & ~255;
		maxDrawCalls = _maxDrawCalls;
		unsigned int cbSizeInBytesAligned = cbSizeInBytes * maxDrawCalls;
		offsetIndex = 0;
		D3D12_HEAP_PROPERTIES heapprops = {};
		heapprops.Type = D3D12_HEAP_TYPE_UPLOAD;
		heapprops.CreationNodeMask = 1;
		heapprops.VisibleNodeMask = 1;
		D3D12_RESOURCE_DESC cbDesc = {};
		cbDesc.Width = cbSizeInBytesAligned;
		cbDesc.Height = 1;
		cbDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		cbDesc.DepthOrArraySize = 1;
		cbDesc.MipLevels = 1;
		cbDesc.SampleDesc.Count = 1;
		cbDesc.SampleDesc.Quality = 0;
		cbDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

		core->device->CreateCommittedResource(&heapprops, D3D12_HEAP_FLAG_NONE, &cbDesc, D3D12_RESOURCE_STATE_GENERIC_READ, NULL,
			IID_PPV_ARGS(&constantBuffer));
		constantBuffer->Map(0, NULL, (void**)&buffer);

		HRESULT hr;
		hr = core->device->CreateCommittedResource(&heapprops, D3D12_HEAP_FLAG_NONE, &cbDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ, NULL, __uuidof(ID3D12Resource), (void**)&constantBuffer);
		hr = constantBuffer->Map(0, NULL, (void**)&buffer);
	}
	// name = variable name
	void update(std::string name, void* data) {
		ConstantBufferVariable cbVariable = constantBufferData[name];
		unsigned int offset = offsetIndex * cbSizeInBytes;
		memcpy(&buffer[offset + cbVariable.offset], data, cbVariable.size);
	}
	void next() {
		offsetIndex++;
		if (offsetIndex >= maxDrawCalls) offsetIndex = 0;
	}
	// get address of constant buffer in use
	D3D12_GPU_VIRTUAL_ADDRESS getGPUAddress() {
		return (constantBuffer->GetGPUVirtualAddress() + (offsetIndex * cbSizeInBytes));
	}
};
// always have a pair of VS and PS
class Shaders {
public:
	ID3DBlob* vertexShader; // translation to clip space
	ID3DBlob* pixelShader;  // calcute colour for every covered pixel
	std::string vertexShaderStr;
	std::string pixelShaderStr;

	ID3DBlob* status;
	HRESULT hr;
	std::vector<ConstantBuffer> vsConstantBuffers, psConstantBuffers;

	void init(Core* core, std::string vs, std::string ps) {
		vertexShaderStr = Read(vs + ".txt");
		VS_compiler();
		reflect(core, vertexShader, vsConstantBuffers);
		pixelShaderStr = Read(ps + ".txt");
		PS_compiler();
		reflect(core, pixelShader, psConstantBuffers);
	}
	std::string Read(std::string filename) {
		std::ifstream file(filename);
		std::stringstream buffer;
		buffer << file.rdbuf();
		return buffer.str();
	}
	void VS_compiler() {
		hr = D3DCompile(vertexShaderStr.c_str(), strlen(vertexShaderStr.c_str()), NULL,
			NULL, NULL, "VS", "vs_5_0", 0, 0, &vertexShader, &status);
		if (FAILED(hr)) {
			MessageBoxA(NULL, (char*)status->GetBufferPointer(), "Vertex shader compilation failed", 0);
			exit(0);
		}
	}
	void PS_compiler() {
		hr = D3DCompile(pixelShaderStr.c_str(), strlen(pixelShaderStr.c_str()), NULL, NULL,
			NULL, "PS", "ps_5_0", 0, 0, &pixelShader, &status);
		if (FAILED(hr)) {
			MessageBoxA(NULL, (char*)status->GetBufferPointer(), "Shader compilation failed", 0);
			exit(0);
		}
	}
	void reflect(Core* core, ID3DBlob* shader, std::vector<ConstantBuffer>& targetBuffers);
	void apply(Core* core) {
		for (int i = 0; i < vsConstantBuffers.size(); i++) {
			core->getCommandList()->SetGraphicsRootConstantBufferView(0, vsConstantBuffers[i].getGPUAddress());
			vsConstantBuffers[i].next();
		}
		for (int i = 0; i < psConstantBuffers.size(); i++) {
			core->getCommandList()->SetGraphicsRootConstantBufferView(1, psConstantBuffers[i].getGPUAddress());
			psConstantBuffers[i].next();
		}
	}
	void updateConstantBuffers(std::string variablename, void* data) {
		for (int i = 0; i < vsConstantBuffers.size(); i++) {
			if (vsConstantBuffers[i].constantBufferData.find(variablename) != vsConstantBuffers[i].constantBufferData.end())
				vsConstantBuffers[i].update(variablename, data);
		}
		for (int i = 0; i < psConstantBuffers.size(); i++) {
			if (psConstantBuffers[i].constantBufferData.find(variablename) != psConstantBuffers[i].constantBufferData.end())
				psConstantBuffers[i].update(variablename, data);
		}
	}
};
class ShadersManager {
public:
	std::map<std::string, Shaders> shaders;

	// name = shadername
	void load(Core* core, std::string vs, std::string ps, std::string name) {
		if (shaders.find(name) != shaders.end())
			return;
		Shaders s;
		s.init(core, vs, ps);
		shaders.insert({ name, s });
	}
	Shaders& get(std::string name) {
		if (shaders.find(name) == shaders.end()) exit(0);
		return shaders[name];
	}
};
