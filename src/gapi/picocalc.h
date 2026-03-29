#pragma once

#define PROFILE_MARKER(title)
#define PROFILE_LABEL(id, name, label)
#define PROFILE_TIMING(time)

namespace GAPI {

    static uint32* piColor = NULL;
    static uint16* piDepth = NULL;
    static int     piWidth = 0;
    static int     piHeight = 0;

    static uint32 piClearColor = 0xFF000000;

    static constexpr int kPiScreenWidth = 320;
    static constexpr int kPiScreenHeight = 320;

    static constexpr int kPiRenderWidth = kPiScreenWidth;
    static constexpr int kPiRenderHeight = kPiRenderWidth * 9 / 16;

    static constexpr int kPiPresentX = 0;
    static constexpr int kPiPresentY = (kPiScreenHeight - kPiRenderHeight) / 2;
    static constexpr int kPiPresentWidth = kPiRenderWidth;
    static constexpr int kPiPresentHeight = kPiRenderHeight;

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

    // -------------------------------------------------------------------------
    // PicoCalc debug framebuffer
    // -------------------------------------------------------------------------


    static int piDipCounter = 0;
    static int piSoloDip = -1;

    static void piEnsureBuffers(int w, int h) {
        if (piColor && piDepth && piWidth == w && piHeight == h)
            return;

        delete[] piColor;
        delete[] piDepth;
        piColor = NULL;
        piDepth = NULL;
        piWidth = w;
        piHeight = h;

        if (w > 0 && h > 0) {
            piColor = new uint32[w * h];
            piDepth = new uint16[w * h];

            memset(piColor, 0, w * h * sizeof(piColor[0]));
            for (int i = 0; i < w * h; i++)
                piDepth[i] = 0xFFFF;
        }
    }

    static void piPutPixel(int x, int y, uint32 c) {
        if (!piColor) return;
        if (x < 0 || y < 0 || x >= piWidth || y >= piHeight) return;
        piColor[y * piWidth + x] = c;
    }

    static void piPutPixelDepth(int x, int y, uint16 z, uint32 c) {
        if (!piColor || !piDepth) return;
        if (x < 0 || y < 0 || x >= piWidth || y >= piHeight) return;

        int idx = y * piWidth + x;
        if (z <= piDepth[idx]) {
            piDepth[idx] = z;
            piColor[idx] = c;
        }
    }

    static void piDrawLine(int x0, int y0, int x1, int y1, uint32 c) {
        int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
        int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
        int err = dx + dy;

        while (true) {
            piPutPixel(x0, y0, c);
            if (x0 == x1 && y0 == y1) break;
            int e2 = err * 2;
            if (e2 >= dy) { err += dy; x0 += sx; }
            if (e2 <= dx) { err += dx; y0 += sy; }
        }
    }

    static inline float piEdgeFunction(float ax, float ay, float bx, float by, float cx, float cy) {
        return (cx - ax) * (by - ay) - (cy - ay) * (bx - ax);
    }

    struct PiDebugVertex {
        vec4 clip;
        vec3 ndc;
        float sx, sy;
        bool valid;
    };

    static void piFillTriangle(const PiDebugVertex& a, const PiDebugVertex& b, const PiDebugVertex& c, uint32 color) {
        if (!piColor || piWidth <= 0 || piHeight <= 0)
            return;

        float minXf = floorf(min(a.sx, min(b.sx, c.sx)));
        float minYf = floorf(min(a.sy, min(b.sy, c.sy)));
        float maxXf = ceilf(max(a.sx, max(b.sx, c.sx)));
        float maxYf = ceilf(max(a.sy, max(b.sy, c.sy)));

        int minX = max(0, (int)minXf);
        int minY = max(0, (int)minYf);
        int maxX = min(piWidth - 1, (int)maxXf);
        int maxY = min(piHeight - 1, (int)maxYf);

        float area = piEdgeFunction(a.sx, a.sy, b.sx, b.sy, c.sx, c.sy);
        if (fabsf(area) < 0.5f)
            return;

        bool positive = area > 0.0f;

        for (int y = minY; y <= maxY; y++) {
            for (int x = minX; x <= maxX; x++) {
                float px = (float)x + 0.5f;
                float py = (float)y + 0.5f;

                float w0 = piEdgeFunction(b.sx, b.sy, c.sx, c.sy, px, py);
                float w1 = piEdgeFunction(c.sx, c.sy, a.sx, a.sy, px, py);
                float w2 = piEdgeFunction(a.sx, a.sy, b.sx, b.sy, px, py);

                if (positive) {
                    if (w0 >= 0.0f && w1 >= 0.0f && w2 >= 0.0f)
                        piPutPixel(x, y, color);
                }
                else {
                    if (w0 <= 0.0f && w1 <= 0.0f && w2 <= 0.0f)
                        piPutPixel(x, y, color);
                }
            }
        }
    }

    struct PiBmpFileHeader {
        uint32 bfSize;
        uint16 bfReserved1;
        uint16 bfReserved2;
        uint32 bfOffBits;
    };

    struct PiBmpInfoHeader {
        uint32 biSize;
        uint32 biWidth;
        uint32 biHeight;
        uint16 biPlanes;
        uint16 biBitCount;
        uint32 biCompression;
        uint32 biSizeImage;
        uint32 biXPelsPerMeter;
        uint32 biYPelsPerMeter;
        uint32 biClrUsed;
        uint32 biClrImportant;
    };

    static void piSaveBMP(const char* name, const uint32* data32, int width, int height) {
        if (!data32 || width <= 0 || height <= 0)
            return;

        PiBmpFileHeader fhdr;
        PiBmpInfoHeader ihdr;

        memset(&fhdr, 0, sizeof(fhdr));
        memset(&ihdr, 0, sizeof(ihdr));

        ihdr.biSize = sizeof(ihdr);
        ihdr.biWidth = (uint32)width;
        ihdr.biHeight = (uint32)height;
        ihdr.biPlanes = 1;
        ihdr.biBitCount = 32;
        ihdr.biSizeImage = (uint32)(width * height * 4);

        fhdr.bfOffBits = 2 + sizeof(fhdr) + sizeof(ihdr);
        fhdr.bfSize = fhdr.bfOffBits + ihdr.biSizeImage;

        uint32* bmp = new uint32[width * height];

        for (int y = 0; y < height; y++) {
            const uint32* src = data32 + (height - 1 - y) * width;
            uint32* dst = bmp + y * width;

            for (int x = 0; x < width; x++) {
                uint32 c = src[x];
                uint32 a = (c >> 24) & 0xFF;
                uint32 r = (c >> 16) & 0xFF;
                uint32 g = (c >> 8) & 0xFF;
                uint32 b = (c >> 0) & 0xFF;
                dst[x] = (a << 24) | (b << 16) | (g << 8) | r;
            }
        }

        FILE* f = fopen(name, "wb");
        if (f) {
            uint16 type = 'B' + ('M' << 8);
            fwrite(&type, sizeof(type), 1, f);
            fwrite(&fhdr, sizeof(fhdr), 1, f);
            fwrite(&ihdr, sizeof(ihdr), 1, f);
            fwrite(bmp, ihdr.biSizeImage, 1, f);
            fclose(f);
            LOG("saved %s\n", name);
        }
        else {
            LOG("failed to save %s\n", name);
        }

        delete[] bmp;
    }

    static uint32 piColorForDip(int dipIndex) {
        static const uint32 colors[] = {
            0xFFFF4040, 0xFF40FF40, 0xFF4080FF, 0xFFFFFF40,
            0xFFFF40FF, 0xFF40FFFF, 0xFFFFFFFF, 0xFFFF8040
        };
        return colors[dipIndex % (sizeof(colors) / sizeof(colors[0]))];
    }

    static float piTriArea2(float x0, float y0, float x1, float y1, float x2, float y2) {
        return (x1 - x0) * (y2 - y0) - (x2 - x0) * (y1 - y0);
    }

    static bool piConsumeDumpKey() {
#ifdef _OS_WIN
        static bool wasDown = false;
        bool isDown = (GetAsyncKeyState(VK_F5) & 0x8000) != 0;
        bool pressed = isDown && !wasDown;
        wasDown = isDown;
        return pressed;
#else
        return false;
#endif
    }

    static bool piConsumeSoloNextKey() {
#ifdef _OS_WIN
        static bool wasDown = false;
        bool isDown = (GetAsyncKeyState(VK_F7) & 0x8000) != 0;
        bool pressed = isDown && !wasDown;
        wasDown = isDown;
        return pressed;
#else
        return false;
#endif
    }

    static bool piConsumeSoloClearKey() {
#ifdef _OS_WIN
        static bool wasDown = false;
        bool isDown = (GetAsyncKeyState(VK_F8) & 0x8000) != 0;
        bool pressed = isDown && !wasDown;
        wasDown = isDown;
        return pressed;
#else
        return false;
#endif
    }

    // -------------------------------------------------------------------------
    // PicoCalc temporary primitive storage
    // -------------------------------------------------------------------------

    struct PiVertexSW {
        int32 x, y, z, w;
        bool  valid;
    };

    Array<PiVertexSW> piVertices;
    Array<Index>      piIndices;
    Array<int32>      piTriangles;
    Array<int32>      piQuads;

    static PiVertexSW piTransformVertex(const Vertex& vertex) {
        PiVertexSW out;
        out.x = out.y = out.z = out.w = 0;
        out.valid = false;

        vec4 c = (Core::mViewProj * Core::mModel) * vec4(
            (float)vertex.coord.x,
            (float)vertex.coord.y,
            (float)vertex.coord.z,
            1.0f
        );

        if (fabsf(c.w) < 0.00001f)
            return out;

        const float PI_MAX_DIST = 20.0f * 1024.0f;
        if (c.w < 0.0f || c.w > PI_MAX_DIST)
            return out;

        c.x /= c.w;
        c.y /= c.w;
        c.z /= c.w;

        c.x = clamp(c.x, -16384.0f, 16384.0f);
        c.y = clamp(c.y, -16384.0f, 16384.0f);

        out.x = (int32)(((c.x * 0.5f + 0.5f) * (float)piWidth) * 65536.0f);
        out.y = (int32)((1.0f - (c.y * 0.5f + 0.5f)) * (float)piHeight);
        out.z = (int32)(clamp(c.z, 0.0f, 1.0f) * 65535.0f * 65536.0f);
        out.w = (int32)(c.w * 65536.0f);
        out.valid = true;
        return out;
    }

    static void piTransformRange(const Index* indices, const Vertex* vertices, int iStart, int iCount, int vStart) {
        piVertices.reset();
        piIndices.reset();
        piTriangles.reset();
        piQuads.reset();

        int vIndex = 0;
        bool isTriangle = false;

        for (int i = 0; i < iCount; i++) {
            const Index index = indices[iStart + i];
            const Vertex& vertex = vertices[vStart + index];

            vIndex++;

            if (vIndex == 1) {
                isTriangle = vertex.normal.w == 1;
            }
            else {
                if (vIndex == 4) {
                    // loader splits quads to two triangles with indices 012[02]3
                    // ignore duplicated [02] to rebuild a quad like sw.h
                    vIndex++;
                    i++;
                    continue;
                }
            }

            PiVertexSW result = piTransformVertex(vertex);

            if (!result.valid) {
                if (isTriangle) {
                    i += 3 - vIndex;
                }
                else {
                    i += 6 - vIndex;
                }
                vIndex = 0;
                continue;
            }

            piIndices.push(piVertices.push(result));

            if (isTriangle && vIndex == 3) {
                piTriangles.push(piIndices.length - 3);
                vIndex = 0;
            }
            else if (vIndex == 6) {
                piQuads.push(piIndices.length - 4);
                vIndex = 0;
            }
        }
    }

    static void piFillTriangleDepth(const PiVertexSW& a, const PiVertexSW& b, const PiVertexSW& c, uint32 color) {
        if (!piColor || !piDepth || piWidth <= 0 || piHeight <= 0)
            return;

        float ax = a.x / 65536.0f;
        float ay = (float)a.y;
        float bx = b.x / 65536.0f;
        float by = (float)b.y;
        float cx = c.x / 65536.0f;
        float cy = (float)c.y;

        float minXf = floorf(min(ax, min(bx, cx)));
        float minYf = floorf(min(ay, min(by, cy)));
        float maxXf = ceilf(max(ax, max(bx, cx)));
        float maxYf = ceilf(max(ay, max(by, cy)));

        int minX = max(0, (int)minXf);
        int minY = max(0, (int)minYf);
        int maxX = min(piWidth - 1, (int)maxXf);
        int maxY = min(piHeight - 1, (int)maxYf);

        float area = piEdgeFunction(ax, ay, bx, by, cx, cy);
        if (fabsf(area) < 0.5f)
            return;

        bool positive = area > 0.0f;

        float az = (float)(a.z >> 16);
        float bz = (float)(b.z >> 16);
        float cz = (float)(c.z >> 16);

        for (int y = minY; y <= maxY; y++) {
            for (int x = minX; x <= maxX; x++) {
                float px = (float)x + 0.5f;
                float py = (float)y + 0.5f;

                float w0 = piEdgeFunction(bx, by, cx, cy, px, py);
                float w1 = piEdgeFunction(cx, cy, ax, ay, px, py);
                float w2 = piEdgeFunction(ax, ay, bx, by, px, py);

                bool inside;
                if (positive)
                    inside = (w0 >= 0.0f && w1 >= 0.0f && w2 >= 0.0f);
                else
                    inside = (w0 <= 0.0f && w1 <= 0.0f && w2 <= 0.0f);

                if (!inside)
                    continue;

                w0 /= area;
                w1 /= area;
                w2 /= area;

                float zf = az * w0 + bz * w1 + cz * w2;
                if (zf < 0.0f) zf = 0.0f;
                if (zf > 65535.0f) zf = 65535.0f;

                piPutPixelDepth(x, y, (uint16)zf, color);
            }
        }
    }

    static void piDrawTriangle(Index* indices, uint32 color, bool drawEdges) {
        const PiVertexSW& a = piVertices[indices[0]];
        const PiVertexSW& b = piVertices[indices[1]];
        const PiVertexSW& c = piVertices[indices[2]];

        float ax = a.x / 65536.0f, ay = (float)a.y;
        float bx = b.x / 65536.0f, by = (float)b.y;
        float cx = c.x / 65536.0f, cy = (float)c.y;

        float area2 = piTriArea2(ax, ay, bx, by, cx, cy);
        if (fabsf(area2) < 0.5f)
            return;

        piFillTriangleDepth(a, b, c, color);

        if (drawEdges) {
            piDrawLine((int)ax, (int)ay, (int)bx, (int)by, 0xFFFFFFFF);
            piDrawLine((int)bx, (int)by, (int)cx, (int)cy, 0xFFFFFFFF);
            piDrawLine((int)cx, (int)cy, (int)ax, (int)ay, 0xFFFFFFFF);
        }
    }

    static bool piBackface2D(float ax, float ay, float bx, float by, float cx, float cy) {
        return ((bx - ax) * (cy - ay) - (cx - ax) * (by - ay)) <= 0.0f;
    }

    static void piDrawQuad(Index* indices, uint32 color, bool drawEdges) {
        const PiVertexSW& a = piVertices[indices[0]];
        const PiVertexSW& b = piVertices[indices[1]];
        const PiVertexSW& c = piVertices[indices[2]];
        const PiVertexSW& d = piVertices[indices[3]];

        if (!a.valid || !b.valid || !c.valid || !d.valid)
            return;

        float ax = a.x / 65536.0f, ay = (float)a.y;
        float bx = b.x / 65536.0f, by = (float)b.y;
        float cx = c.x / 65536.0f, cy = (float)c.y;
        float dx = d.x / 65536.0f, dy = (float)d.y;

        bool sameSide =
            piBackface2D(ax, ay, cx, cy, bx, by) ==
            piBackface2D(ax, ay, cx, cy, dx, dy);

        if (sameSide) {
            piFillTriangleDepth(a, b, c, color);
            piFillTriangleDepth(a, c, d, color);
        }
        else {
            piFillTriangleDepth(a, b, d, color);
            piFillTriangleDepth(b, c, d, color);
        }

        if (drawEdges) {
            piDrawLine((int)ax, (int)ay, (int)bx, (int)by, 0xFFFFFFFF);
            piDrawLine((int)bx, (int)by, (int)cx, (int)cy, 0xFFFFFFFF);
            piDrawLine((int)cx, (int)cy, (int)dx, (int)dy, 0xFFFFFFFF);
            piDrawLine((int)dx, (int)dy, (int)ax, (int)ay, 0xFFFFFFFF);
        }
    }

    // -------------------------------------------------------------------------
    // Shader
    // -------------------------------------------------------------------------

    struct Shader {
        void init(Pass pass, int type, int* def, int defCount) {
            (void)pass; (void)type; (void)def; (void)defCount;
        }
        void deinit() {}
        void bind() {}
        void setParam(UniformType uType, const vec4& value, int count = 1) {
            (void)uType; (void)value; (void)count;
        }
        void setParam(UniformType uType, const mat4& value, int count = 1) {
            (void)uType; (void)value; (void)count;
        }
    };

    // -------------------------------------------------------------------------
    // Texture
    // -------------------------------------------------------------------------

    struct Texture {
        uint8* memory;
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

        void init(void* data) {
            ASSERT((opt & OPT_PROXY) == 0);

            opt &= ~(OPT_CUBEMAP | OPT_MIPMAPS);

            memory = new uint8[width * height * 4];
            if (data) {
                update(data);
            }
            else {
                memset(memory, 0, width * height * 4);
            }
        }

        void deinit() {
            if (memory) {
                delete[] memory;
                memory = NULL;
            }
        }

        void generateMipMap() {}

        void update(void* data) {
            memcpy(memory, data, width * height * 4);
        }

        void bind(int sampler) {
            Core::active.textures[sampler] = this;
            if (!this || (opt & OPT_PROXY)) return;
            ASSERT(memory);
        }

        void bindTileIndices(Tile8* tile) {
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

    // -------------------------------------------------------------------------
    // Mesh
    // -------------------------------------------------------------------------

    struct Mesh {
        Index* iBuffer;
        Vertex* vBuffer;

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

        void init(Index* indices, int iCount, ::Vertex* vertices, int vCount, int aCount) {
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

        void update(Index* indices, int iCount, ::Vertex* vertices, int vCount) {
            if (indices) {
                memcpy(iBuffer, indices, iCount * sizeof(indices[0]));
            }

            if (vertices) {
                memcpy(vBuffer, vertices, vCount * sizeof(vertices[0]));
            }
        }

        void bind(const MeshRange& range) const {
            (void)range;
        }

        void initNextRange(MeshRange& range, int& aIndex) const {
            (void)aIndex;
            range.aIndex = -1;
        }
    };

    // -------------------------------------------------------------------------
    // Lifecycle
    // -------------------------------------------------------------------------

    void init() {
        LOG("Renderer : %s\n", "PicoCalc");
        LOG("Version  : %s\n", "0.1");
        LOG("PicoCalc render target: %dx%d\n", kPiRenderWidth, kPiRenderHeight);
        LOG("PicoCalc present rect : x=%d y=%d w=%d h=%d within %dx%d\n",
            kPiPresentX, kPiPresentY, kPiPresentWidth, kPiPresentHeight,
            kPiScreenWidth, kPiScreenHeight);

        Core::support.texNPOT = true;

        stats.resetAll();
        piEnsureBuffers(kPiRenderWidth, kPiRenderHeight);
    }

    void deinit() {
        delete[] piColor;
        delete[] piDepth;
        piColor = NULL;
        piDepth = NULL;
        piWidth = 0;
        piHeight = 0;

        piVertices.clear();
        piIndices.clear();
        piTriangles.clear();
        piQuads.clear();
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
        piDipCounter = 0;
        return true;
    }

    void endFrame() {
        if (piConsumeSoloNextKey()) {
            if (piSoloDip < 0)
                piSoloDip = 0;
            else
                piSoloDip++;
            LOG("solo DIP = %d\n", piSoloDip);
        }

        if (piConsumeSoloClearKey()) {
            piSoloDip = -1;
            LOG("solo DIP off\n");
        }

        if (piConsumeDumpKey() && piColor && piDepth && piWidth > 0 && piHeight > 0) {
            piSaveBMP("picocalc_debug.bmp", piColor, piWidth, piHeight);
            LOG("saved picocalc_debug.bmp at frame=%d dips=%d tris=%d verts=%d soloDip=%d\n",
                stats.frames, stats.dips, stats.triangles, stats.verticesTouched, piSoloDip);
        }
    }

    void resetState() {
    }

    // -------------------------------------------------------------------------
    // Targets / presentation
    // -------------------------------------------------------------------------

    void bindTarget(Texture* texture, int face) {
        (void)texture;
        (void)face;
    }

    void discardTarget(bool color, bool depth) {
        (void)color;
        (void)depth;
    }

    void copyTarget(Texture* dst, int xOffset, int yOffset, int x, int y, int width, int height) {
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

    // -------------------------------------------------------------------------
    // State
    // -------------------------------------------------------------------------

    void clear(bool color, bool depth) {
        if (color && piColor) {
            for (int i = 0; i < piWidth * piHeight; i++)
                piColor[i] = piClearColor;
        }

        if (depth && piDepth) {
            for (int i = 0; i < piWidth * piHeight; i++)
                piDepth[i] = 0xFFFF;
        }
    }

    void setClearColor(const vec4& color) {
        auto clampByte = [](float x) -> uint32 {
            if (x < 0.0f) x = 0.0f;
            if (x > 1.0f) x = 1.0f;
            return (uint32)(x * 255.0f + 0.5f);
            };

        uint32 r = clampByte(color.x);
        uint32 g = clampByte(color.y);
        uint32 b = clampByte(color.z);
        uint32 a = clampByte(color.w);
        piClearColor = (a << 24) | (r << 16) | (g << 8) | b;
    }

    void setViewport(const short4& v) {
        static bool logged = false;
        if (!logged) {
            LOG("PicoCalc viewport ignored: x=%d y=%d w=%d h=%d\n", v.x, v.y, v.z, v.w);
            logged = true;
        }
    }

    void setScissor(const short4& s) {
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

    void setViewProj(const mat4& mView, const mat4& mProj) {
        mViewProjCache = mProj * mView;
    }

    void updateLights(vec4* lightPos, vec4* lightColor, int count) {
        (void)lightPos;
        (void)lightColor;
        (void)count;
    }

    void setFog(const vec4& params) {
        (void)params;
    }

    // -------------------------------------------------------------------------
    // Draw
    // -------------------------------------------------------------------------

    void DIP(Mesh* mesh, const MeshRange& range) {
        int thisDip = piDipCounter++;

        stats.dips++;
        stats.ranges++;

        if (!mesh || !mesh->vBuffer || !mesh->iBuffer)
            return;

        stats.meshes++;
        stats.verticesTouched += mesh->vCount;
        stats.triangles += range.iCount / 3;

        if (piSoloDip >= 0 && thisDip != piSoloDip)
            return;

        piTransformRange(mesh->iBuffer, mesh->vBuffer, range.iStart, range.iCount, range.vStart);

        const uint32 color = piColorForDip(thisDip);
        const bool drawEdges = false;

        for (int i = 0; i < piQuads.length; i++) {
            piDrawQuad(&piIndices[piQuads[i]], color, drawEdges);
        }

        for (int i = 0; i < piTriangles.length; i++) {
            piDrawTriangle(&piIndices[piTriangles[i]], color, drawEdges);
        }
    }

    void initPalette(Color24* palette, uint8* lightmap) {
        (void)palette;
        (void)lightmap;
    }

    vec4 copyPixel(int x, int y) {
        (void)x;
        (void)y;
        return vec4(0.0f);
    }

    const uint32* getPresentBuffer() {
        return piColor;
    }

    int getPresentWidth() {
        return piWidth;
    }

    int getPresentHeight() {
        return piHeight;
    }
}