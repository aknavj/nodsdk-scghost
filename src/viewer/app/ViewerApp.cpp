/*
===========================================================================
Copyright (C) 2002 - 2005, Nihilistic Software, Inc.
Copyright (C) 2002 - 2014, Blizzard Entertainment, Inc.
Copyright (C) 2024 - 2026, OpenNod SDK & StaRev contributors

StaRev - Starcraft Ghost reversed
This file is part of the StaRev source code.

StaRev is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License version 3
as published by the Free Software Foundation.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, see <http://www.gnu.org/licenses/>.
===========================================================================
*/

#include "app/ViewerApp.h"
#include "app/BrowserView.h"
#include "tools/common/tool_help.h"

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include <cstdio>

/*
 */
namespace viewer::app {

/*
 */
namespace {

    /*
    */
    void glfwErrorCallback(int error, const char *description) {
        std::fprintf(stderr, "[GLFW Error %d] %s\n", error, description);
    }

} // namespace

/*
 */
int ViewerApp::run(int argc, char **argv) {
	if(argc < 2) {
		std::fprintf(stderr, "Usage: %s <asset_directory>\n", argv[0]);
		return 1;
	}
	const char *assetDir = argv[1];

	glfwSetErrorCallback(&glfwErrorCallback);
	if(!glfwInit()) {
		std::fprintf(stderr, "Failed to initialize GLFW\n");
		return 1;
	}

	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE,        GLFW_OPENGL_CORE_PROFILE);
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);

	GLFWwindow *window = glfwCreateWindow(1600, 900, NOD_TOOLS_WINDOW_TITLE("NODAssetViewer"), nullptr, nullptr);
	if(!window) {
		std::fprintf(stderr, "Failed to create GLFW window\n");
		glfwTerminate();
		return 1;
	}
	glfwMakeContextCurrent(window);
	glfwSwapInterval(1);

	glewExperimental = GL_TRUE;
	const GLenum glewErr = glewInit();
	if(glewErr != GLEW_OK) {
		// GLEW_ERROR_NO_GLX_DISPLAY (4) is benign on Wayland/EGL —
		// the GL context is valid, GLEW just can't find a GLX display.
		std::fprintf(stderr, "GLEW warning: %s (code %u) — continuing anyway\n", glewGetErrorString(glewErr), glewErr);
	}
	glGetError(); // flush spurious error

	std::printf("OpenGL: %s\n",   glGetString(GL_VERSION));
	std::printf("Renderer: %s\n", glGetString(GL_RENDERER));

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO &io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

	ImGui::StyleColorsDark();
	ImGuiStyle &style = ImGui::GetStyle();
	style.WindowRounding = 0.0f;
	style.FrameRounding = 2.0f;
	style.ScrollbarRounding = 3.0f;
	style.GrabRounding = 2.0f;

	ImGui_ImplGlfw_InitForOpenGL(window, true);
	ImGui_ImplOpenGL3_Init("#version 330 core");

	{
		BrowserView browser;
		if(!browser.mount(assetDir)) {
			std::fprintf(stderr, "Warning: no files found in '%s'\n", assetDir);
		}

		while(!glfwWindowShouldClose(window)) {
			glfwPollEvents();

			if(io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Q)) {
				glfwSetWindowShouldClose(window, GLFW_TRUE);
			}

			ImGui_ImplOpenGL3_NewFrame();
			ImGui_ImplGlfw_NewFrame();
			ImGui::NewFrame();

			browser.draw();

			ImGui::Render();

			int displayW, displayH;
			glfwGetFramebufferSize(window, &displayW, &displayH);
			glViewport(0, 0, displayW, displayH);
			glClearColor(0.12f, 0.12f, 0.14f, 1.0f);
			glClear(GL_COLOR_BUFFER_BIT);

			ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
			glfwSwapBuffers(window);
		}
	} // BrowserView destroyed before GL/ImGui shutdown

	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();

	glfwDestroyWindow(window);
	glfwTerminate();
	return 0;
}

} // namespace viewer::app
