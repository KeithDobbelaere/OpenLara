#ifndef H_GAPI_PICOCALC
#define H_GAPI_PICOCALC

#include "core.h"

#define PROFILE_MARKER(title)
#define PROFILE_LABEL(id, name, label)
#define PROFILE_TIMING(time)

//#define DITHER_FILTER

#define COLOR_FMT_888
#define CONV_COLOR(r,g,b) (0xFF000000u | ((uint32(r) & 0xFF) << 16) | ((uint32(g) & 0xFF) << 8) | (uint32(b) & 0xFF))

#define SW_MAX_DIST  (20.0f * 1024.0f)
#define SW_FOG_START (12.0f * 1024.0f)

namespace GAPI {

    using namespace Core;

    typedef ::Vertex Vertex;

    typedef uint32 ColorSW;
    typedef uint16 DepthSW;

    uint8* swLightmap;
    uint8   swLightmapNone[32 * 256];
    uint8   swLightmapShade[32 * 256];
    ColorSW* swPalette;
    ColorSW swPaletteColor[256];
    ColorSW swPaletteWater[256];
    ColorSW swPaletteGray[256];
    uint8   swGradient[256];
    Tile8* curTile;

    uint8 ambient;
    int32 lightsCount;

    struct LightSW {
        uint32 intensity;
        vec3   pos;
        float  radius;
    } lights[MAX_LIGHTS], lightsRel[MAX_LIGHTS];

    // Shader
    struct Shader {
        void init(Pass pass, int type, int* def, int defCount) {}
        void deinit() {}
        void bind() {}
        void setParam(UniformType uType, const vec4& value, int count = 1) {}
        void setParam(UniformType uType, const mat4& value, int count = 1) {}
    };

    // Texture
    struct Texture {
        uint8* memory;
        int        width, height, depth, origWidth, origHeight;
        TexFormat  fmt;
        uint32     opt;

        Texture(int width, int height, int depth, TexFormat fmt, uint32 opt = 0)
            : memory(NULL)
            , width(width)
            , height(height)
            , depth(depth)
            , origWidth(width)
            , origHeight(height)
            , fmt(fmt)
            , opt(opt)
        {
        }

        Texture(int width, int height, int depth, TexOption opt)
            : memory(NULL)
            , width(width)
            , height(height)
            , depth(depth)
            , origWidth(width)
            , origHeight(height)
            , fmt(FMT_RGBA)
            , opt((uint32)opt)
        {
        }

        Texture(int width, int height, int depth, uint32 opt)
            : memory(NULL)
            , width(width)
            , height(height)
            , depth(depth)
            , origWidth(width)
            , origHeight(height)
            , fmt(FMT_RGBA)
            , opt(opt)
        {
        }

        static int bytesPerPixel(TexFormat fmt) {
            switch (fmt) {
            case FMT_LUMINANCE: return 1;
            case FMT_RGB16:     return 2;
            case FMT_RGBA16:    return 2;
            case FMT_DEPTH:     return 2;
            case FMT_SHADOW:    return 2;
            case FMT_RG_HALF:   return 4;
            case FMT_RG_FLOAT:  return 8;
            case FMT_RGBA:      return 4;
            default:            return 4;
            }
        }

        int dataSize() const {
            return width * height * bytesPerPixel(fmt);
        }

        void init(void* data) {
            if (opt & OPT_PROXY)
                return;

            opt &= ~(OPT_CUBEMAP | OPT_MIPMAPS);

            const int size = width * height * bytesPerPixel(fmt);
            memory = new uint8[size];
            memset(memory, 0, size);

            if (data && !(opt & OPT_DYNAMIC)) {
                update(data);
            }
        }

        void deinit() {
            delete[] memory;
            memory = NULL;
        }

        void generateMipMap() {}

        void update(void* data) {
            if (!memory || !data)
                return;

            ASSERT((opt & (OPT_VOLUME | OPT_CUBEMAP)) == 0);

            if (fmt == FMT_RGBA) {
                const uint8* src = (const uint8*)data;
                uint8* dst = memory;

                const int srcPitch = origWidth * 4;
                const int dstPitch = width * 4;

                for (int y = 0; y < origHeight; y++) {
                    memcpy(dst + y * dstPitch, src + y * srcPitch, srcPitch);
                }
                return;
            }

            const int size = origWidth * origHeight * bytesPerPixel(fmt);
            memcpy(memory, data, size);
        }

        void bind(int sampler) {
            Core::active.textures[sampler] = this;

            if (opt & OPT_PROXY)
                return;

            ASSERT(memory);
            curTile = NULL;
        }

        void bindTileIndices(Tile8* tile) {
            curTile = (Tile8*)tile;
        }

        void unbind(int sampler) {}

        void setFilterQuality(int value) {
            if (value > Settings::LOW)
                opt &= ~OPT_NEAREST;
            else
                opt |= OPT_NEAREST;
        }
    };

    // Mesh
    struct Mesh {
        Index* iBuffer;
        GAPI::Vertex* vBuffer;

        int          iCount;
        int          vCount;
        bool         dynamic;

        Mesh(bool dynamic) : iBuffer(NULL), vBuffer(NULL), dynamic(dynamic) {}

        void init(Index* indices, int iCount, ::Vertex* vertices, int vCount, int aCount) {
            this->iCount = iCount;
            this->vCount = vCount;

            iBuffer = new Index[iCount];
            vBuffer = new Vertex[vCount];

            update(indices, iCount, vertices, vCount);
        }

        void deinit() {
            delete[] iBuffer;
            delete[] vBuffer;
        }

        void update(Index* indices, int iCount, ::Vertex* vertices, int vCount) {
            if (indices) {
                memcpy(iBuffer, indices, iCount * sizeof(indices[0]));
            }

            if (vertices) {
                memcpy(vBuffer, vertices, vCount * sizeof(vertices[0]));
            }
        }

        void bind(const MeshRange& range) const {}

        void initNextRange(MeshRange& range, int& aIndex) const {
            range.aIndex = -1;
        }
    };


    int cullMode, blendMode;

    ColorSW* swColor;
    DepthSW* swDepth;
    short4  swClipRect;

    static int piWidth = 0;
    static int piHeight = 0;
    static uint32 piClearColor = 0xFF000000u;

    static void piEnsureBuffers(int w, int h) {
        if (w <= 0 || h <= 0)
            return;

        if (swColor && swDepth && piWidth == w && piHeight == h)
            return;

        delete[] swColor;
        delete[] swDepth;

        piWidth = w;
        piHeight = h;
        swColor = new ColorSW[piWidth * piHeight];
        swDepth = new DepthSW[piWidth * piHeight];
        swClipRect = short4(0, 0, piWidth, piHeight);

        for (int i = 0; i < piWidth * piHeight; i++)
            swColor[i] = piClearColor;
        memset(swDepth, 0xFF, piWidth * piHeight * sizeof(DepthSW));
    }

    struct VertexSW {
        int32 x, y, z, w;
        int32 u, v, l;

        inline VertexSW operator + (const VertexSW& p) const {
            VertexSW ret;
            ret.x = x + p.x;
            ret.y = y;
            ret.z = z + p.z;
            ret.w = w + p.w;
            ret.u = u + p.u;
            ret.v = v + p.v;
            ret.l = l + p.l;
            return ret;
        }

        inline VertexSW operator - (const VertexSW& p) const {
            VertexSW ret;
            ret.x = x - p.x;
            ret.y = y;
            ret.z = z - p.z;
            ret.w = w - p.w;
            ret.u = u - p.u;
            ret.v = v - p.v;
            ret.l = l - p.l;
            return ret;
        }

        inline VertexSW operator * (const int32 s) const {
            VertexSW ret;
            ret.x = x * s;
            ret.y = y;
            ret.z = z * s;
            ret.w = w * s;
            ret.u = u * s;
            ret.v = v * s;
            ret.l = l * s;
            return ret;
        }

        inline VertexSW operator / (const int32 s) const {
            VertexSW ret;
            ret.x = x / s;
            ret.y = y;
            ret.z = z / s;
            ret.w = w / s;
            ret.u = u / s;
            ret.v = v / s;
            ret.l = l / s;
            return ret;
        }
    };

    Array<VertexSW> swVertices;
    Array<Index>    swIndices;
    Array<int32>    swTriangles;
    Array<int32>    swQuads;

    Tile8 swDummyTile;

    void init() {
        LOG("Renderer : %s\n", "PicoCalc");
        LOG("Version  : %s\n", "0.1");

        swColor = NULL;
        swDepth = NULL;
        curTile = NULL;
        ambient = 255;
        lightsCount = 0;
        swLightmap = swLightmapNone;
        swPalette = swPaletteColor;

        for (int i = 0; i < 256; i++) {
            swPaletteColor[i] = 0xFF000000u | (uint32(i) << 16) | (uint32(i) << 8) | uint32(i);
            swPaletteWater[i] = swPaletteColor[i];
            swPaletteGray[i] = swPaletteColor[i];
            swGradient[i] = uint8(i);
        }

        for (int i = 0; i < 256 * 32; i++) {
            swLightmapNone[i] = uint8(i & 255);
            swLightmapShade[i] = uint8(i & 255);
        }

        for (int y = 0; y < 256; y++) {
            for (int x = 0; x < 256; x++) {
                swDummyTile.index[(y << 8) + x] = uint8(x ? x : 1);
            }
        }

        swLightmap = swLightmapNone;
        swPalette = swPaletteColor;

        memset(swDummyTile.index, 0, sizeof(swDummyTile.index));

        piEnsureBuffers(Core::width, Core::height);
    }

    void deinit() {
        delete[] swColor;
        delete[] swDepth;
        swLightmap = swLightmapNone;
        swColor = NULL;
        swDepth = NULL;
        curTile = NULL;
        swVertices.clear();
        swIndices.clear();
        swTriangles.clear();
        swQuads.clear();
    }

    void resize() {
        piEnsureBuffers(Core::width, Core::height);
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
        piEnsureBuffers(Core::width, Core::height);
        return true;
    }

    void endFrame() {}

    void resetState() {}

    void bindTarget(Texture* texture, int face) {}

    void discardTarget(bool color, bool depth) {}

    void copyTarget(Texture* dst, int xOffset, int yOffset, int x, int y, int width, int height) {}

    void setVSync(bool enable) {}

    void waitVBlank() {}

    void clear(bool color, bool depth) {
        if (color && swColor) {
            for (int i = 0; i < piWidth * piHeight; i++)
                swColor[i] = piClearColor;
        }

        if (depth && swDepth) {
            memset(swDepth, 0xFF, piWidth * piHeight * sizeof(DepthSW));
        }
    }

    void setClearColor(const vec4& color) {
        auto toByte = [](float v) -> uint32 {
            if (v < 0.0f) v = 0.0f;
            if (v > 1.0f) v = 1.0f;
            return uint32(v * 255.0f + 0.5f);
            };

        uint32 r = toByte(color.x);
        uint32 g = toByte(color.y);
        uint32 b = toByte(color.z);
        uint32 a = toByte(color.w);
        piClearColor = (a << 24) | (r << 16) | (g << 8) | b;
    }

    void setViewport(const short4& v) {
        (void)v;
        swClipRect = short4(0, 0, piWidth, piHeight);
    }

    void setScissor(const short4& s) {
        int x1 = s.x;
        int y1 = piHeight - (s.y + s.w);
        int x2 = s.x + s.z;
        int y2 = piHeight - s.y;

        if (x1 < 0) x1 = 0;
        if (y1 < 0) y1 = 0;
        if (x2 > piWidth)  x2 = piWidth;
        if (y2 > piHeight) y2 = piHeight;

        if (x2 < x1) x2 = x1;
        if (y2 < y1) y2 = y1;

        swClipRect.x = x1;
        swClipRect.y = y1;
        swClipRect.z = x2;
        swClipRect.w = y2;
    }

    void setDepthTest(bool enable) {}
    void setDepthWrite(bool enable) {}
    void setColorWrite(bool r, bool g, bool b, bool a) {}
    void setAlphaTest(bool enable) {}
    void setCullMode(int rsMask) {}
    void setBlendMode(int rsMask) {}

    void setViewProj(const mat4& mView, const mat4& mProj) {
        mViewProj = mProj * mView;
    }

    void updateLights(vec4* lightPos, vec4* lightColor, int count) {
        ambient = clamp(int32(active.material.y * 255), 0, 255);

        lightsCount = 0;
        for (int i = 0; i < count; i++) {
            if (lightColor[i].w >= 1.0f)
                continue;

            LightSW& light = lights[lightsCount++];
            vec4& c = lightColor[i];
            light.intensity = uint32(((c.x + c.y + c.z) / 3.0f) * 255.0f);
            light.pos = lightPos[i].xyz();
            light.radius = lightColor[i].w;
        }
    }

    void setFog(const vec4& params) {}

    bool checkBackface(const VertexSW* a, const VertexSW* b, const VertexSW* c) {
        return ((b->x - a->x) >> 16) * (c->y - a->y) -
            ((c->x - a->x) >> 16) * (b->y - a->y) <= 0;
    }

    inline void sortVertices(VertexSW*& t, VertexSW*& m, VertexSW*& b) {
        if (t->y > m->y) swap(t, m);
        if (t->y > b->y) swap(t, b);
        if (m->y > b->y) swap(m, b);
    }

    inline void sortVertices(VertexSW*& t, VertexSW*& m, VertexSW*& b, VertexSW*& o) {
        if (t->y > m->y) swap(t, m);
        if (o->y > b->y) swap(o, b);
        if (t->y > o->y) swap(t, o);
        if (m->y > b->y) swap(m, b);
        if (m->y > o->y) swap(m, o);
    }

    inline void step(VertexSW& v, const VertexSW& d) {
        v.u += d.u;
        v.v += d.v;
        v.l += d.l;
    }

    inline void step(VertexSW& v, const VertexSW& d, int32 count) {
        v.u += d.u * count;
        v.v += d.v * count;
        v.l += d.l * count;
    }

    const int uvDither[8] = {
        32768, 16384,     0, 49152,
        49152,     0, 32768, 16384
    };

    void drawLine(const VertexSW& L, const VertexSW& R, int32 y) {
        if (!swColor || !curTile)
            return;

        if (y < 0 || y >= piHeight)
            return;

        VertexSW left = L;
        VertexSW right = R;

        if (left.x > right.x) {
            VertexSW t = left;
            left = right;
            right = t;
        }

        int32 x1 = left.x >> 16;
        int32 x2 = right.x >> 16;

        int32 f = x2 - x1;
        if (f == 0)
            return;

        VertexSW dS = (right - left) / f;
        VertexSW S = left;

        if (x1 < swClipRect.x) {
            int32 delta = swClipRect.x - x1;
            S.z += dS.z * delta;
            step(S, dS, delta);
            x1 = swClipRect.x;
        }

        if (x2 > swClipRect.z)
            x2 = swClipRect.z;

        if (x1 < 0) x1 = 0;
        if (x2 > piWidth) x2 = piWidth;
        if (x2 <= x1)
            return;

        int32 row = y * piWidth;

        Texture* tex = Core::active.textures[0];
        const bool rgbaTex = tex && tex->memory && tex->fmt == FMT_RGBA;

        for (int x = row + x1; x < row + x2; x++) {
            S.z += dS.z;

            if (rgbaTex) {
                int32 u = int32(uint32(S.u) >> 16);
                int32 v = int32(uint32(S.v) >> 16);

                if ((uint32)u < (uint32)tex->origWidth && (uint32)v < (uint32)tex->origHeight) {
                    const uint8* src = tex->memory + (v * tex->width + u) * 4;

                    uint32 r = src[0];
                    uint32 g = src[1];
                    uint32 b = src[2];
                    uint32 a = src[3];

                    swColor[x] = (a << 24) | (r << 16) | (g << 8) | b;
                }
            }
            else {
                int32 u = int32(uint32(S.u) >> 16);
                int32 v = int32(uint32(S.v) >> 16);

                if ((uint32)u < 256u && (uint32)v < 256u) {
                    uint8 index = curTile->index[(v << 8) + u];

                    if (index != 0) {
                        int32 light = (S.l >> (16 + 3));
                        if ((uint32)light < 32u) {
                            index = swLightmap[(light << 8) + index];
                            swColor[x] = swPalette[index];
                        }
                    }
                }
            }

            step(S, dS);
        }
    }

    void drawPart(const VertexSW& a, const VertexSW& b, const VertexSW& c, const VertexSW& d) {
        VertexSW L, R, dL, dR;
        int32 minY, maxY;

        int32 f = c.y - a.y;
        if (f == 0)
            return;

        dL = (c - a) / f;
        dR = (d - b) / f;

        L = a;
        R = b;

        minY = a.y;
        maxY = c.y;

        if (maxY < swClipRect.y || minY >= swClipRect.w)
            return;

        if (minY < swClipRect.y) {
            int32 delta = swClipRect.y - minY;
            L.x += dL.x * delta;
            L.z += dL.z * delta;
            R.x += dR.x * delta;
            R.z += dR.z * delta;
            step(L, dL, delta);
            step(R, dR, delta);
            minY = swClipRect.y;
        }

        if (maxY > swClipRect.w)
            maxY = swClipRect.w;

        for (int y = minY; y < maxY; y++) {
            drawLine(L, R, y);
            L.x += dL.x;
            L.z += dL.z;
            R.x += dR.x;
            R.z += dR.z;
            step(L, dL);
            step(R, dR);
        }
    }

    void drawTriangle(Index* indices) {
        VertexSW _n;
        VertexSW* t = swVertices.items + indices[0];
        VertexSW* m = swVertices.items + indices[1];
        VertexSW* b = swVertices.items + indices[2];
        VertexSW* n = &_n;

        if (checkBackface(t, m, b))
            return;

        int32 cx1 = swClipRect.x << 16;
        int32 cx2 = swClipRect.z << 16;

        if (t->x < cx1 && m->x < cx1 && b->x < cx1)
            return;

        if (t->x > cx2 && m->x > cx2 && b->x > cx2)
            return;

        sortVertices(t, m, b);

        if (b->y < swClipRect.y || t->y > swClipRect.w)
            return;

        int32 denom = b->y - t->y;
        if (denom == 0)
            return;

        *n = ((*b - *t) / denom * (m->y - t->y)) + *t;
        n->y = m->y;

        if (m->x > n->x)
            swap(m, n);

        if (m->y != t->y) drawPart(*t, *t, *m, *n);
        if (m->y != b->y) drawPart(*m, *n, *b, *b);
    }

    void drawQuad(Index* indices) {
        VertexSW _n;
        VertexSW _p;
        VertexSW* t = swVertices.items + indices[0];
        VertexSW* m = swVertices.items + indices[1];
        VertexSW* b = swVertices.items + indices[2];
        VertexSW* o = swVertices.items + indices[3];
        VertexSW* n = &_n;
        VertexSW* p = &_p;

        if (checkBackface(t, m, b))
            return;

        int32 cx1 = swClipRect.x << 16;
        int32 cx2 = swClipRect.z << 16;

        if (t->x < cx1 && m->x < cx1 && o->x < cx1 && b->x < cx1)
            return;

        if (t->x > cx2 && m->x > cx2 && o->x > cx2 && b->x > cx2)
            return;

        sortVertices(t, m, b, o);

        if (b->y < swClipRect.y || t->y > swClipRect.w)
            return;

        int32 denom = b->y - t->y;
        if (denom == 0)
            return;

        if (checkBackface(t, b, m) == checkBackface(t, b, o)) {
            VertexSW d = (*b - *t) / denom;

            *n = *t + d * (m->y - t->y);
            *p = *t + d * (o->y - t->y);

            n->y = m->y;
            p->y = o->y;
        }
        else {
            if (o->y != t->y) {
                *n = *t + ((*o - *t) / (o->y - t->y) * (m->y - t->y));
                n->y = m->y;
            }

            if (m->y != b->y) {
                *p = *b + ((*m - *b) / (m->y - b->y) * (o->y - b->y));
                p->y = o->y;
            }
        }

        if (o->y != t->y && m->x > n->x) swap(m, n);
        if (m->y != b->y && p->x > o->x) swap(p, o);

        if (t->y != m->y) drawPart(*t, *t, *m, *n);
        if (m->y != o->y) drawPart(*m, *n, *p, *o);
        if (o->y != b->y) drawPart(*p, *o, *b, *b);
    }

    void applyLighting(VertexSW& result, const Vertex& vertex, float depth) {
        vec3 coord = vec3(float(vertex.coord.x), float(vertex.coord.y), float(vertex.coord.z));
        vec3 normal = vec3(float(vertex.normal.x), float(vertex.normal.y), float(vertex.normal.z)).normal();

        float lighting = 0.0f;
        for (int i = 0; i < lightsCount; i++) {
            LightSW& light = lightsRel[i];
            vec3 dir = (light.pos - coord) * light.radius;
            float att = dir.length2();
            float lum = normal.dot(dir / sqrtf(att));
            lighting += (max(0.0f, lum) * max(0.0f, 1.0f - att)) * light.intensity;
        }

        lighting += result.l;

        depth -= SW_FOG_START;
        if (depth > 0.0f) {
            lighting *= clamp(1.0f - depth / (SW_MAX_DIST - SW_FOG_START), 0.0f, 1.0f);
        }

        result.l = (255 - min(255, int32(lighting))) << 16;
    }

    bool transform(const Index* indices, const Vertex* vertices, int iStart, int iCount, int vStart) {
        swVertices.reset();
        swIndices.reset();
        swTriangles.reset();
        swQuads.reset();

        mat4 swMatrix3D;
        swMatrix3D.viewport(0.0f, (float)piHeight, (float)piWidth, -(float)piHeight, 0.0f, 1.0f);
        swMatrix3D = swMatrix3D * mViewProj * mModel;

        const bool colored = vertices[vStart + indices[iStart]].color.w == 142;
        int vIndex = 0;
        bool isTriangle = false;

        for (int i = 0; i < iCount; i++) {
            const Index index = indices[iStart + i];
            const Vertex& vertex = vertices[vStart + index];

            vIndex++;

            if (vIndex == 1) {
                isTriangle = vertex.normal.w == 1;
            }
            else if (vIndex == 4) {
                vIndex++;
                i++;
                continue;
            }

            vec4 c;

            const bool is2DFace = (vertex.coord.w == 1 && vertex.coord.z == 0);

            if (is2DFace) {
                // 2D/UI-style faces: avoid the full 3D view/proj path
                c.x = float(vertex.coord.x) / 16384.0f;
                c.y = float(vertex.coord.y) / 16384.0f;
                c.z = 0.0f;
                c.w = 1.0f;

                c.x = (c.x * 0.5f + 0.5f) * float(piWidth);
                c.y = (1.0f - (c.y * 0.5f + 0.5f)) * float(piHeight);
            }
            else {
                c = swMatrix3D * vec4(vertex.coord.x, vertex.coord.y, vertex.coord.z, 1.0f);

                if (c.w < 0.0f || c.w > SW_MAX_DIST) {
                    if (isTriangle)
                        i += 3 - vIndex;
                    else
                        i += 6 - vIndex;
                    vIndex = 0;
                    continue;
                }

                c.x /= c.w;
                c.y /= c.w;
                c.z /= c.w;
                c.x = clamp(c.x, -16384.0f, 16384.0f);
                c.y = clamp(c.y, -16384.0f, 16384.0f);
            }

            VertexSW result;
            result.x = int32(c.x) << 16;
            result.y = int32(c.y);
            result.z = uint32(clamp(c.z, 0.0f, 1.0f) * 65535.0f) << 16;
            result.w = int32(c.w) << 16;

            Texture* tex = Core::active.textures[0];

            const bool isVideoQuad =
                is2DFace &&
                tex &&
                tex->memory &&
                tex->fmt == FMT_RGBA &&
                (tex->opt & OPT_DYNAMIC);

            if (colored) {
                result.u = vertex.color.x << 16;
                result.v = 0;
            }
            else {
                if (isVideoQuad) {
                    result.u = vertex.texCoord.x << 10;
                    result.v = vertex.texCoord.y << 9;
                }
                else if (is2DFace) {
                    //result.u = (vertex.texCoord.x >> 8) << 16;
                    //result.v = (vertex.texCoord.y >> 8) << 16;
                    result.u = vertex.texCoord.x << 11;
                    result.v = vertex.texCoord.y << 10;
                }
                else {
                    result.u = vertex.texCoord.x << 16;
                    result.v = vertex.texCoord.y << 16;
                }
            }

            result.l = ((vertex.light.x * ambient) >> 8);
            applyLighting(result, vertex, c.w);

            swIndices.push(swVertices.push(result));

            if (isTriangle && vIndex == 3) {
                swTriangles.push(swIndices.length - 3);
                vIndex = 0;
            }
            else if (vIndex == 6) {
                swQuads.push(swIndices.length - 4);
                vIndex = 0;
            }
        }

        return colored;
    }

    void transformLights() {
        memcpy(lightsRel, lights, sizeof(LightSW) * lightsCount);

        mat4 mModelInv = mModel.inverseOrtho();
        for (int i = 0; i < lightsCount; i++) {
            lightsRel[i].pos = mModelInv * lights[i].pos;
        }
    }

    void DIP(Mesh* mesh, const MeshRange& range) {
        transformLights();

        bool colored = transform(mesh->iBuffer, mesh->vBuffer, range.iStart, range.iCount, range.vStart);

        Tile8* oldTile = curTile;

        if (colored || curTile == NULL)
            curTile = &swDummyTile;

        for (int i = 0; i < swQuads.length; i++) {
            drawQuad(&swIndices[swQuads[i]]);
        }

        for (int i = 0; i < swTriangles.length; i++) {
            drawTriangle(&swIndices[swTriangles[i]]);
        }

        curTile = oldTile;
    }

    void initPalette(Color24* palette, uint8* lightmap) {
        for (uint32 i = 0; i < 256; i++) {
            const Color24& p = palette[i];
            swPaletteColor[i] = CONV_COLOR(p.r, p.g, p.b);
            swPaletteWater[i] = CONV_COLOR((uint32(p.r) * 150) >> 8, (uint32(p.g) * 230) >> 8, (uint32(p.b) * 230) >> 8);
            swPaletteGray[i] = CONV_COLOR((i * 57) >> 8, (i * 29) >> 8, (i * 112) >> 8);
            swGradient[i] = i;
        }

        for (uint32 i = 0; i < 256 * 32; i++) {
            swLightmapNone[i] = i % 256;
            swLightmapShade[i] = lightmap[i];
        }

        for (uint32 y = 0; y < 256; y++) {
            for (uint32 x = 0; x < 256; x++) {
                swDummyTile.index[(y << 8) + x] = uint8(x);
            }
        }

        swLightmap = swLightmapShade;
        swPalette = swPaletteColor;
    }

    void setPalette(ColorSW* palette) {
        swPalette = palette;
    }

    void setShading(bool enabled) {
        swLightmap = enabled ? swLightmapShade : swLightmapNone;
    }

    vec4 copyPixel(int x, int y) {
        if (!swColor || x < 0 || y < 0 || x >= piWidth || y >= piHeight)
            return vec4(0.0f);

        uint32 c = swColor[y * piWidth + x];
        return vec4(
            float((c >> 16) & 0xFF) / 255.0f,
            float((c >> 8) & 0xFF) / 255.0f,
            float((c >> 0) & 0xFF) / 255.0f,
            float((c >> 24) & 0xFF) / 255.0f
        );
    }

    const uint32* getPresentBuffer() {
        return (const uint32*)swColor;
    }

    int getPresentWidth() {
        return piWidth;
    }

    int getPresentHeight() {
        return piHeight;
    }
}

#endif
