#pragma once
#include <Windows.h>
#include <string>

class Window {
public:
	HWND hwnd;
	HINSTANCE hinstance;
	int width, height;
	WNDCLASSEX wc;
	std::string name;
	bool keys[256];
	int mousex, mousey;
	bool mouseButtons[3];

	void create(int window_width, int window_height, const std::string window_name, float window_x, float window_y);
	void updateMouse(int x, int y) {
		mousex = x;mousey = y;
	}
	void processMessages();
};