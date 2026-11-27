#define _USE_MATH_DEFINES
#include <Windows.h>
#include "Window.h"
#include "Core.h"
#include "Math.h"
#include <time.h>
#include <cmath>

struct PRIM_VERTEX {
	Vec3 position;
	Colour colour;
};

class ScreenSpaceTriangle {
public:
	PRIM_VERTEX vertices[3];
	ID3D12Resource* vertexBuffer;
	D3D12_VERTEX_BUFFER_VIEW vbView;
	// define layout
	D3D12_INPUT_ELEMENT_DESC inputLayout[2];
	D3D12_INPUT_LAYOUT_DESC inputLayoutDesc;

	void init(Core* core, void* vertices, int vertexSizeInBytes, int numVertices) {
		//specify vertex buffer will be in GPU memory heap
		D3D12_HEAP_PROPERTIES heapprops = {};
		heapprops.Type = D3D12_HEAP_TYPE_DEFAULT;
		heapprops.CreationNodeMask = 1;
		heapprops.VisibleNodeMask = 1;
		D3D12_RESOURCE_DESC vbDesc = {};

		// create vertex buffer on heap
		vbDesc.Width = numVertices * vertexSizeInBytes;
		vbDesc.Height = 1;
		vbDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		vbDesc.DepthOrArraySize = 1;
		vbDesc.MipLevels = 1;
		vbDesc.SampleDesc.Count = 1;
		vbDesc.SampleDesc.Quality = 0;
		vbDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		// allocate memory
		core->device->CreateCommittedResource(&heapprops, D3D12_HEAP_FLAG_NONE, &vbDesc,
			D3D12_RESOURCE_STATE_COMMON, NULL, IID_PPV_ARGS(&vertexBuffer));
		// copy vertices
		core->uploadResource(vertexBuffer, vertices, numVertices * vertexSizeInBytes,
			D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
		// fill in view
		vbView.BufferLocation = vertexBuffer->GetGPUVirtualAddress();
		vbView.StrideInBytes = vertexSizeInBytes;
		vbView.SizeInBytes = numVertices * vertexSizeInBytes;
	}
	void init(Core* core) {
		vertices[0].position = Vec3(0, 1.0f, 0);
		vertices[0].colour = Colour(0.0f, 1.0f, 0.0f, 1.0f);
		vertices[1].position = Vec3(-1.0f, -1.0f, 0);
		vertices[1].colour = Colour(1.0f, 0.0f, 0.0f, 1.0f);
		vertices[2].position = Vec3(1.0f, -1.0f, 0);
		vertices[2].colour = Colour(0.0f, 0.0f, 1.0f, 1.0f);
		init(core, &vertices[0], sizeof(PRIM_VERTEX), 3);
		// fill in layout (has to match PRIM_VERTEX order)
		inputLayout[0] = { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT,
			D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 };
		inputLayout[1] = { "COLOUR", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT,
			D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 };
		inputLayoutDesc.NumElements = 2;
		inputLayoutDesc.pInputElementDescs = inputLayout;
	}
	void draw(Core* core) {
		// specify type of geometry
		core->getCommandList()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		// where the geometry is (the view)
		core->getCommandList()->IASetVertexBuffers(0, 1, &vbView);
		// issue command to draw
		core->getCommandList()->DrawInstanced(3, 1, 0, 0);
	}
};
class ScreenPrimitiveToDraw {
public:
	ScreenSpaceTriangle prim;
	Shaders* shaders;
	std::string name;

	void init(Core* core, PSOManager& psos, Shaders* shader, std::string _name) {
		shaders = shader;
		name = _name;
		prim.init(core);
		psos.createPSO(core, name, shaders->vertexShader, shaders->pixelShader, prim.inputLayoutDesc);
	}

	void draw(Core* core, PSOManager& psos, void* data, void* time) {
		core->beginRenderPass();
		shaders->updateConstantBuffers("lights", data);
		shaders->updateConstantBuffers("time", time);
		shaders->apply(core);
		psos.bind(core, name);
		prim.draw(core);
	}
};

class Timer{
private:
	LARGE_INTEGER freq;   // Frequency of the performance counter
	LARGE_INTEGER start;  // Starting time

public:
	// Constructor that initializes the frequency
	Timer()
	{
		QueryPerformanceFrequency(&freq);
		reset();
	}

	// Resets the timer
	void reset()
	{
		QueryPerformanceCounter(&start);
	}

	// Returns the elapsed time since the last reset in seconds. Note this should only be called once per frame as it resets the timer.
	float dt()
	{
		LARGE_INTEGER cur;
		QueryPerformanceCounter(&cur);
		float value = static_cast<float>(cur.QuadPart - start.QuadPart) / freq.QuadPart;
		reset();
		return value;
	}
};
int WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PSTR lpCmdline, int nCmdShow) {
	Window win;
	win.create(1024, 768, "Window", 0, 0);
	Core core;
	core.init(win.hwnd, win.width, win.height);

	PSOManager psos;
	ShadersManager shadersManager;
	shadersManager.load(&core, "VertexShader", "PixelShader", "Triangle");
	Shaders& s = shadersManager.get("Triangle");

	ScreenPrimitiveToDraw prim;
	prim.init(&core, psos, &s, "Triangle");

	Timer timer;
	float time = 0;
	Vec4 lights[4];
	//std::vector<std::pair<std::string, void*>> list;

	while (1) {
		core.beginFrame();
		win.processMessages();

		 // lights spinning
		time += timer.dt();

		for (int i = 0; i < 4; i++) {
			float angle = time + (i * M_PI / 2.0f);
			lights[i] = Vec4(1024 / 2.0f + (cosf(angle) * (768 * 0.3f)),
							 1024 / 2.0f + (sinf(angle) * (768 * 0.3f)),
							 0, 0);
		}
		prim.draw(&core, psos, lights, &time);
		if (win.keys[VK_ESCAPE] == 1) break;
		core.finishFrame();
	}
	core.flushGraphicsQueue();
	return 0;
}