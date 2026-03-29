#pragma once

#define PROFILE_MARKER(title)
#define PROFILE_LABEL(id, name, label)
#define PROFILE_TIMING(time)

namespace GAPI {

    using namespace Core;

    typedef ::Vertex Vertex;

    struct Stats {
        int frames;
        int dips;
        int meshes;
        int ranges;
        int triangles;
        int verticesTouched;

        void resetFrame() {
            dips = 0;
            meshes = 0;
            ranges = 0;
            triangles = 0;
            verticesTouched = 0;
        }

        void resetAll() {
            frames = 0;
            resetFrame();
        }
    };

    static Stats stats;

    int cullMode, blendMode;
    short4 clipRect;
    mat4   mViewProjCache;

    struct Shader {
        void init(Pass pass, int type, int *def, int defCount) {
            (void)pass; (void)type; (void)def; (void)defCount;
        }
        void deinit() {}
        void bind() {}
        void setParam(UniformType uType, const vec4 &value, int count = 1) {
            (void)uType; (void)value; (void)count;
        }
        void setParam(UniformType uType, const mat4 &value, int count = 1) {
            (void)uType; (void)value; (void)count;
        }
    };

    struct Texture {
        uint8     *memory;
        int       width, height, origWidth, origHeight;
        TexFormat fmt;
        uint32    opt;

        Texture(int width, int height, int depth, uint32 opt)
            : memory(0)
            , width(width)
            , height(height)
            , origWidth(width)
            , origHeight(height)
            , fmt(FMT_RGBA)
            , opt(opt) {
            (void)depth;
        }

        void init(void *data) {
            ASSERT((opt & OPT_PROXY) == 0);

            opt &= ~(OPT_CUBEMAP | OPT_MIPMAPS);

            memory = new uint8[width * height * 4];
            if (data) {
                update(data);
            }
        }

        void deinit() {
            if (memory) {
                delete[] memory;
                memory = NULL;
            }
        }

        void generateMipMap() {}

        void update(void *data) {
            memcpy(memory, data, width * height * 4);
        }

        void bind(int sampler) {
            Core::active.textures[sampler] = this;
            if (!this || (opt & OPT_PROXY)) return;
            ASSERT(memory);
        }

        void bindTileIndices(Tile8 *tile) {
            (void)tile;
        }

        void unbind(int sampler) {
            (void)sampler;
        }

        void setFilterQuality(int value) {
            if (value > Settings::LOW)
                opt &= ~OPT_NEAREST;
            else
                opt |= OPT_NEAREST;
        }
    };

    struct Mesh {
        Index  *iBuffer;
        Vertex *vBuffer;

        int  iCount;
        int  vCount;
        bool dynamic;

        Mesh(bool dynamic)
            : iBuffer(NULL)
            , vBuffer(NULL)
            , iCount(0)
            , vCount(0)
            , dynamic(dynamic) {
        }

        void init(Index *indices, int iCount, ::Vertex *vertices, int vCount, int aCount) {
            (void)aCount;

            this->iCount = iCount;
            this->vCount = vCount;

            iBuffer = new Index[iCount];
            vBuffer = new Vertex[vCount];

            update(indices, iCount, vertices, vCount);
        }

        void deinit() {
            delete[] iBuffer;
            delete[] vBuffer;
            iBuffer = NULL;
            vBuffer = NULL;
            iCount = 0;
            vCount = 0;
        }

        void update(Index *indices, int iCount, ::Vertex *vertices, int vCount) {
            if (indices) {
                memcpy(iBuffer, indices, iCount * sizeof(indices[0]));
            }

            if (vertices) {
                memcpy(vBuffer, vertices, vCount * sizeof(vertices[0]));
            }
        }

        void bind(const MeshRange &range) const {
            (void)range;
        }

        void initNextRange(MeshRange &range, int &aIndex) const {
            (void)aIndex;
            range.aIndex = -1;
        }
    };

    void init() {
        LOG("Renderer : %s\n", "PicoCalc");
        LOG("Version  : %s\n", "0.1");

        Core::support.texNPOT = true;

        stats.resetAll();
    }

    void deinit() {
    }

    void resize() {
    }

    inline mat4::ProjRange getProjRange() {
        return mat4::PROJ_ZERO_POS;
    }

    mat4 ortho(float l, float r, float b, float t, float znear, float zfar) {
        mat4 m;
        m.ortho(getProjRange(), l, r, b, t, znear, zfar);
        return m;
    }

    mat4 perspective(float fov, float aspect, float znear, float zfar, float eye) {
        mat4 m;
        m.perspective(getProjRange(), fov, aspect, znear, zfar, eye);
        return m;
    }

    bool beginFrame() {
        stats.frames++;
        stats.resetFrame();
        return true;
    }

    void endFrame() {
        if ((stats.frames % 60) == 0) {
            LOG("PicoCalc GAPI: frame=%d dips=%d tris=%d verts=%d\n",
                stats.frames,
                stats.dips,
                stats.triangles,
                stats.verticesTouched);
        }
    }

    void resetState() {
    }

    void bindTarget(Texture *texture, int face) {
        (void)texture;
        (void)face;
    }

    void discardTarget(bool color, bool depth) {
        (void)color;
        (void)depth;
    }

    void copyTarget(Texture *dst, int xOffset, int yOffset, int x, int y, int width, int height) {
        (void)dst;
        (void)xOffset;
        (void)yOffset;
        (void)x;
        (void)y;
        (void)width;
        (void)height;
    }

    void setVSync(bool enable) {
        (void)enable;
    }

    void waitVBlank() {
    }

    void clear(bool color, bool depth) {
        (void)color;
        (void)depth;
    }

    void setClearColor(const vec4 &color) {
        (void)color;
    }

    void setViewport(const short4 &v) {
        (void)v;
    }

    void setScissor(const short4 &s) {
        clipRect = s;
    }

    void setDepthTest(bool enable) {
        (void)enable;
    }

    void setDepthWrite(bool enable) {
        (void)enable;
    }

    void setColorWrite(bool r, bool g, bool b, bool a) {
        (void)r; (void)g; (void)b; (void)a;
    }

    void setAlphaTest(bool enable) {
        (void)enable;
    }

    void setCullMode(int rsMask) {
        cullMode = rsMask;
    }

    void setBlendMode(int rsMask) {
        blendMode = rsMask;
    }

    void setViewProj(const mat4 &mView, const mat4 &mProj) {
        mViewProjCache = mProj * mView;
    }

    void updateLights(vec4 *lightPos, vec4 *lightColor, int count) {
        (void)lightPos;
        (void)lightColor;
        (void)count;
    }

    void setFog(const vec4 &params) {
        (void)params;
    }

    void DIP(Mesh *mesh, const MeshRange &range) {
        stats.dips++;
        stats.ranges++;

        if (!mesh || !mesh->vBuffer || !mesh->iBuffer)
            return;

        stats.meshes++;
        stats.verticesTouched += mesh->vCount;
        stats.triangles += range.iCount / 3;

        // later:
        // - transform from mesh->iBuffer / mesh->vBuffer using range.iStart/iCount/vStart
        // - bin by slab
        // - feed Core1 rasterizer
    }

    void initPalette(Color24 *palette, uint8 *lightmap) {
        (void)palette;
        (void)lightmap;
    }

    vec4 copyPixel(int x, int y) {
        (void)x;
        (void)y;
        return vec4(0.0f);
    }
}