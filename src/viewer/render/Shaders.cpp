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

#include "render/Shaders.h"

#include <cstdio>

/*
 */
namespace viewer::render {

const char *kPreviewVertexShader = R"(
#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aUV;
layout(location = 3) in vec3 aVertexColor;

uniform mat4 uMVP;
uniform mat4 uModel;
uniform mat3 uNormalMat;

out vec3 vNormal;
out vec3 vWorldPos;
out vec2 vUV;
out vec3 vVertexColor;

/*
 */
void main() {
    vec4 wp = uModel * vec4(aPos, 1.0);
    vWorldPos = wp.xyz;
    vNormal = normalize(uNormalMat * aNormal);
    vUV = aUV;
    vVertexColor = aVertexColor;
    gl_Position = uMVP * vec4(aPos, 1.0);
}
)";

const char *kPreviewFragmentShader = R"(
#version 330 core
in vec3 vNormal;
in vec3 vWorldPos;
in vec2 vUV;
in vec3 vVertexColor;

uniform vec3 uLightDir;
uniform vec3 uViewPos;
uniform vec3 uColor;
uniform int  uWireframe;
uniform int  uHasTexture;
uniform int  uBakedLighting;   // 1 = use vcol as baked light (vert_type==2)
uniform sampler2D uDiffuseTex;

out vec4 FragColor;

/*
 */
void main() {
    if(uWireframe != 0) {
        FragColor = vec4(uColor, 1.0);
        return;
    }

    vec3 baseColor = uColor;
    if(uHasTexture != 0) {
        baseColor = texture(uDiffuseTex, vUV).rgb;
    }

    // baked vertex lighting path (NIL vert_type==2):
    // vcol encodes overbright pre-baked lighting (128=1.0, 255~=2.0).
    if(uBakedLighting != 0) {
        vec3 vc = gl_FrontFacing ? vVertexColor : max(vVertexColor, vec3(0.25));
        vec3 color = baseColor * vc * 0.45;
        FragColor = vec4(color, 1.0);
        return;
    }

    vec3 N = normalize(vNormal);
    if(!gl_FrontFacing) N = -N;

    vec3 L = normalize(-uLightDir);
    vec3 V = normalize(uViewPos - vWorldPos);
    vec3 H = normalize(L + V);

    float diff = max(dot(N, L), 0.0) * 0.55;
    float spec = pow(max(dot(N, H), 0.0), 32.0) * 0.12;
    float ambient = 0.18;
    float rim = pow(1.0 - max(dot(N, V), 0.0), 3.0) * 0.08;

    vec3 color = baseColor * (ambient + diff)
               + vec3(1.0) * spec
               + vec3(0.5, 0.6, 0.8) * rim;
    FragColor = vec4(color, 1.0);
}
)";

/*
 */
GLuint compileShader(GLenum type, const char *src) noexcept {
	GLuint s = glCreateShader(type);
	glShaderSource(s, 1, &src, nullptr);
	glCompileShader(s);

	GLint ok = GL_FALSE;
	glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
	if(!ok) {
		char log[512];
		glGetShaderInfoLog(s, sizeof(log), nullptr, log);
		std::printf("[Shaders] compile error:\n%s\n", log);
		glDeleteShader(s);
		return 0;
	}
	return s;
}

/*
 */
GLuint linkProgram(GLuint vs, GLuint fs) noexcept {
	GLuint p = glCreateProgram();
	glAttachShader(p, vs);
	glAttachShader(p, fs);
	glLinkProgram(p);

	GLint ok = GL_FALSE;
	glGetProgramiv(p, GL_LINK_STATUS, &ok);
	if(!ok) {
		char log[512];
		glGetProgramInfoLog(p, sizeof(log), nullptr, log);
		std::printf("[Shaders] link error:\n%s\n", log);
		glDeleteProgram(p);
		return 0;
	}
	return p;
}

/*
 */
GLuint buildProgram(const char *vsSrc, const char *fsSrc) noexcept {
	GLuint vs = compileShader(GL_VERTEX_SHADER, vsSrc);
	GLuint fs = compileShader(GL_FRAGMENT_SHADER, fsSrc);
	if(!vs || !fs) {
		if(vs) glDeleteShader(vs);
		if(fs) glDeleteShader(fs);
		return 0;
	}
	GLuint prog = linkProgram(vs, fs);
	glDeleteShader(vs);
	glDeleteShader(fs);
	return prog;
}

} // namespace viewer::render
