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

#pragma once

#include <GL/glew.h>
#include <glm/glm.hpp>
#include <cstdint>
#include <vector>

#include "render/Camera.h"

#include "parsers/NODParser.h"
#include "parsers/NADParser.h"
#include "parsers/NILParser.h"

/*
 */
class DDSParser;
class TGAParser;

/*
 */
namespace viewer::render {

    /*
     */
    class PreviewRenderer {

        public:
            PreviewRenderer() = default;
            ~PreviewRenderer() { shutdown(); }

            PreviewRenderer(const PreviewRenderer &) = delete;
            PreviewRenderer &operator=(const PreviewRenderer &) = delete;

            bool init();
            void shutdown();

            // 2D texture preview helpers (delegate to render::uploadDDS / uploadTGA).
            [[nodiscard]] static GLuint uploadDDS(const DDSParser *dds);
            [[nodiscard]] static GLuint uploadTGA(const TGAParser *tga);
            void drawTexturePreview(GLuint texId, int texW, int texH);

            /*
             */
            struct MeshBuildInfo {
                const uint8_t *vertex_data = nullptr;
                std::size_t    vertex_data_size = 0;
                uint32_t       vertex_set_count = 0;
                const NODParser::VertexSet *vertex_sets = nullptr;
                const uint16_t *indices = nullptr;
                uint32_t        index_count = 0;
                const NODParser::MeshGroup *mesh_groups = nullptr;
                uint32_t        mesh_group_count = 0;
                glm::vec3       bounds_min = glm::vec3(0.0f);
                glm::vec3       bounds_max = glm::vec3(0.0f);
                const GLuint   *group_textures = nullptr;
                uint32_t        group_texture_count = 0;
                const NODParser::Bone *bones = nullptr;
                uint32_t        bone_count = 0;
            };

            /*
             */
            struct SimpleMeshBuildInfo {
                const void  *vertex_data;
                uint32_t     vertex_count;
                uint32_t     vertex_stride;
                uint32_t     pos_offset;
                uint32_t     normal_offset;
                const uint16_t *indices;
                uint32_t     index_count;
                bool         is_strip;
                glm::vec3    bounds_min;
                glm::vec3    bounds_max;
            };

            /*
             */
            struct DrawGroup {
                GLsizei index_offset; // bytes
                GLsizei index_count;
                GLuint  diffuse_tex;
                bool    use_vcol;
            };

            bool buildMesh(const MeshBuildInfo &info);
            bool buildMeshSimple(const SimpleMeshBuildInfo &info);
            bool buildMeshNIL(const NILParser *nil, const GLuint *groupTextures = nullptr, uint32_t groupTextureCount = 0);
            bool buildEntityGeometry(const float *vbuf, uint32_t vcount, const uint32_t *ibuf, uint32_t icount, const DrawGroup *groups, uint32_t groupCount);

            void clearMesh();
            void clearEntityMesh();
            void clearGroupTextures();
            void resetCamera() { m_camera.reset(); }

            // CPU skinning: re-uploads the main VBO with vertices skinned by
            // bone_skin_mats[b] = M_anim_world[b] * M_invbind[b]  (D3D space).
            void updateSkinning(const std::vector<glm::mat4> &boneSkinMats);

            // Renders to FBO and shows in ImGui.  Returns true if drawn.
            bool drawMeshPreview();

            bool wireframe = false;

            // Direct camera access (used by entity instancer for bounds queries).
            [[nodiscard]] Camera       &camera()       noexcept { return m_camera; }
            [[nodiscard]] const Camera &camera() const noexcept { return m_camera; }

        private:
            struct SkinVert {
                glm::vec3 bind_pos;
                glm::vec3 bind_norm;
                glm::vec2 uv;
                uint8_t   bone_idx[4];
                float     bone_w[4];
            };

            void ensureFbo(int w, int h);
            void buildGrid();
            void renderScene(int w, int h);

            // FBO
            int    m_fboW = 0, m_fboH = 0;
            GLuint m_fbo = 0, m_fboColor = 0, m_fboDepth = 0;

            // Shader & uniforms
            GLuint m_prog = 0;
            GLint m_locMVP = -1, m_locModel = -1, m_locNormalMat = -1;
            GLint m_locLightDir = -1, m_locViewPos = -1;
            GLint m_locColor = -1, m_locWireframe = -1;
            GLint m_locHasTexture = -1, m_locDiffuseTex = -1, m_locBakedLighting = -1;

            // Grid
            GLuint m_gridVao = 0, m_gridVbo = 0;
            int    m_gridVertCount = 0;

            // Main mesh
            GLuint  m_vao = 0, m_vbo = 0, m_ebo = 0;
            GLsizei m_idxCount = 0;
            bool    m_hasMesh = false;
            std::vector<DrawGroup> m_drawGroups;
            std::vector<GLuint>    m_ownedTextures;

            // Skinning
            std::vector<SkinVert> m_skinVerts;
            bool                  m_hasSkinning = false;

            // Entity mesh (NIL props baked to world space)
            GLuint m_entityVao = 0, m_entityVbo = 0, m_entityEbo = 0;
            std::vector<DrawGroup> m_entityDrawGroups;
            bool   m_hasEntityMesh = false;

            Camera m_camera;
    };

} // namespace viewer::render
