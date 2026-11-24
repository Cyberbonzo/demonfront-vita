#define GL_GLEXT_PROTOTYPES 1
#include "hd_video.h"
#include "board.h"
#include "m68k_rt.h"
#include "video.h"
#include <SDL_opengl.h>
#include <algorithm>
#include <array>
#include <cstdio>

namespace hd {
constexpr int width = 1920, height = 1080;
static GLuint program, framebuffer, color, vao, textures[5];
static GLint scroll, control, playable, forest_exit;
static int tile_count;

static const char* vertex = R"(#version 330 core
void main() {
    vec2 p = vec2((gl_VertexID << 1) & 2, gl_VertexID & 2);
    gl_Position = vec4(p * 2.0 - 1.0, 0, 1);
})";

static const char* fragment = R"(#version 330 core
uniform usampler2D tiles, letters, tilemap, palettes, sprites;
uniform ivec4 scroll;
uniform int ctrl, tileCount;
uniform bool playable, forestExit;
out vec4 color;
uint wordAt(int addr) { return texelFetch(tilemap, ivec2(addr & 127,addr >> 7),0).r; }
uint penAt(usampler2D atlas,int addr) { return texelFetch(atlas,ivec2(addr & 2047, addr >> 11),0).r; }
vec3 rgb(uint n) {
    uint c = texelFetch(palettes,ivec2(n & 63u,n >> 6),0).r;
    return vec3((c >> 10) & 31u,(c >> 5) & 31u,c & 31u) / 31.0;
}
vec3 sampleBoard(ivec2 at) {
    int y = clamp(at.y,0,223), x = clamp(at.x,0,597);
    // Fixed artwork is centered. Only the scrolling stage uses the expanded world coordinates.
    int worldX = playable ? x : x - 75;
    uint index = 0x3ffu;
    bool solid = false;
    if ((ctrl & 4096) == 0) {
        int mx = (worldX + scroll.x + int(wordAt(0x3800+y))) & 2047;
        // The forest exit ends at world X=0. The next cell belongs to a
        // different height band and is visible only with the wider viewport.
        if (forestExit && worldX >= 448 && mx < 160) mx = 2047 - mx;
        int my = (y + scroll.y) & 511;
        int entry = (my >> 5)*128 + (mx >> 5)*2;
        int tile = int(wordAt(entry)) % tileCount, attr = int(wordAt(entry+1));
        int px = mx & 31, py = my & 31;
        if ((attr & 64) != 0) px=31-px;
        if ((attr & 128) != 0) py=31-py;
        uint pen = penAt(tiles,tile*1024+py*32+px);
        solid = pen != 31u;
        if (solid) index = 1024u + uint((attr >> 1) & 31)*32u + pen;
    }
    if (worldX >= 0 && worldX < (playable ? 598 : 448)) {
        uint sprite = texelFetch(sprites,ivec2(worldX,y),0).r;
        if (sprite != 65535u && (!solid || (sprite & 4096u)==0u)) index=sprite & 1023u;
    }
    // The hardware text plane wraps at 512. Place its two HUD halves at the outer edges once.
    int tx = playable ? (x < 224 ? x : x-150) : worldX;
    bool middle = y >= 40 && y < 182;
    if (playable && middle) tx=x-75;
    bool visible = middle || !playable || x < 224 || x >= 374;
    if (visible && tx >= 0 && tx < 448 && (ctrl & 2048)==0) {
        int mx = (tx+scroll.z) & 511, my=(y+scroll.w) & 255;
        int entry = 0x2000+(my >> 3)*128+(mx >> 3)*2;
        int tile=int(wordAt(entry)), attr=int(wordAt(entry+1)), px=mx & 7, py=my & 7;
        if ((attr & 64)!=0) px=7-px;
        if ((attr & 128)!=0) py=7-py;
        uint pen=penAt(letters,tile*64+py*8+px);
        if (pen!=15u) index=2048u+uint((attr >> 1) & 31)*16u+pen;
    }
    return rgb(index);
}
void main() {
    // Pixel-art coverage at physical sample positions. There is no composited low-resolution frame.
    const vec2 stepSize = vec2(598.0/1920.0,224.0/1080.0);
    vec2 point = vec2(gl_FragCoord.x,1080.0-gl_FragCoord.y)*stepSize;
    vec2 base = floor(point-0.5);
    vec2 weight = clamp((point-base-1.0)/stepSize+0.5,0.0,1.0);
    ivec2 p = ivec2(base);
    if (weight.x==1.0) { p.x++; weight.x=0.0; }
    if (weight.y==1.0) { p.y++; weight.y=0.0; }
    vec3 result = sampleBoard(p);
    if (weight.x>0.0) result=mix(result,sampleBoard(p+ivec2(1,0)),weight.x);
    if (weight.y>0.0) {
        vec3 below=sampleBoard(p+ivec2(0,1));
        if (weight.x>0.0) below=mix(below,sampleBoard(p+ivec2(1)),weight.x);
        result=mix(result,below,weight.y);
    }
    color=vec4(result,1);
})";

static GLuint compile(GLenum kind, const char* source) {
    GLuint shader=glCreateShader(kind);
    glShaderSource(shader,1,&source,nullptr); glCompileShader(shader);
    GLint ok=0; glGetShaderiv(shader,GL_COMPILE_STATUS,&ok);
    if (!ok) {
        char msg[2048]; glGetShaderInfoLog(shader,sizeof msg,nullptr,msg);
        std::fprintf(stderr,"Shader: %s\n",msg); glDeleteShader(shader); return 0;
    }
    return shader;
}

static void texture(GLuint id, int w, int h, GLint format, GLenum layout, GLenum type, const void* data) {
    glBindTexture(GL_TEXTURE_2D,id);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D,0,format,w,h,0,layout,type,data);
}

bool init(SDL_Window* window) {
    GLuint vs=compile(GL_VERTEX_SHADER,vertex), fs=compile(GL_FRAGMENT_SHADER,fragment);
    if (!vs || !fs) return false;
    program=glCreateProgram(); glAttachShader(program,vs); glAttachShader(program,fs);
    glLinkProgram(program); glDeleteShader(vs); glDeleteShader(fs);
    GLint ok=0; glGetProgramiv(program,GL_LINK_STATUS,&ok);
    if (!ok) { char msg[2048]; glGetProgramInfoLog(program,sizeof msg,nullptr,msg); std::fprintf(stderr,"%s\n",msg); return false; }
    glUseProgram(program);
    glGenVertexArrays(1,&vao); glBindVertexArray(vao);
    glGenTextures(5,textures);
    glPixelStorei(GL_UNPACK_ALIGNMENT,1);
    auto bg=video::background_tiles(tile_count);
    texture(textures[0],2048,(tile_count+1)/2,GL_R8UI,GL_RED_INTEGER,GL_UNSIGNED_BYTE,bg);
    texture(textures[1],2048,2048,GL_R8UI,GL_RED_INTEGER,GL_UNSIGNED_BYTE,video::text_tiles());
    texture(textures[2],128,128,GL_R16UI,GL_RED_INTEGER,GL_UNSIGNED_SHORT,nullptr);
    texture(textures[3],64,40,GL_R16UI,GL_RED_INTEGER,GL_UNSIGNED_SHORT,nullptr);
    texture(textures[4],video::W,video::H,GL_R16UI,GL_RED_INTEGER,GL_UNSIGNED_SHORT,nullptr);
    glGenTextures(1,&color);
    texture(color,width,height,GL_RGBA8,GL_RGBA,GL_UNSIGNED_BYTE,nullptr);
    glGenFramebuffers(1,&framebuffer); glBindFramebuffer(GL_FRAMEBUFFER,framebuffer);
    glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,color,0);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER)!=GL_FRAMEBUFFER_COMPLETE || glGetError()!=GL_NO_ERROR) return false;
    const char* names[]={"tiles","letters","tilemap","palettes","sprites"};
    for (int i=0;i<5;i++) {
        glActiveTexture(GL_TEXTURE0+i); glBindTexture(GL_TEXTURE_2D,textures[i]);
        glUniform1i(glGetUniformLocation(program,names[i]),i);
    }
    glUniform1i(glGetUniformLocation(program,"tileCount"),tile_count);
    forest_exit=glGetUniformLocation(program,"forestExit");
    scroll=glGetUniformLocation(program,"scroll");
    control=glGetUniformLocation(program,"ctrl"); playable=glGetUniformLocation(program,"playable");
    int w,h; SDL_GL_GetDrawableSize(window,&w,&h);
    std::fprintf(stderr,"Demon Front: %dx%d framebuffer, %dx%d output, %s\n",width,height,w,h,glGetString(GL_RENDERER));
    return true;
}
static void upload(int slot,int w,int h,const uint16_t* source) {
    glActiveTexture(GL_TEXTURE0+slot);
    glTexSubImage2D(GL_TEXTURE_2D,0,0,0,w,h,GL_RED_INTEGER,GL_UNSIGNED_SHORT,source);
}
// Continue the last painted run of a scenery row only in newly exposed cells.
// Work on the presentation snapshot; game RAM, collisions and object positions stay authoritative.
static void extend_tile_rows(video::Frame& frame) {
    std::array<bool, 1024> visited{};
    for (int y = 0; y < video::H; ++y) {
        int start = frame.bg_xscroll + frame.video_ram[0x3800 + y];
        int row = ((frame.bg_yscroll + y) & 511) >> 5;
        int first = (start + 448) >> 5;
        int last = (start + video::W - 1) >> 5;
        int key = row * 64 + (first & 63);
        if (visited[key]) continue;
        visited[key] = true;
        auto cell = [&](int col) { return frame.video_ram + row * 128 + (col & 63) * 2; };
        for (int col = first; col <= last; ++col) {
            if (cell(col)[0] || cell(col)[1]) continue;
            int edge = col;
            while (edge > first - 6 && !cell(edge-1)[0] && !cell(edge-1)[1]) --edge;
            int length = 0;
            while (length < 8 && (cell(edge-1-length)[0] || cell(edge-1-length)[1])) ++length;
            int end = col + 1;
            while (end <= last && !cell(end)[0] && !cell(end)[1]) ++end;
            if (length) {
                for (int dest = col; dest < end; ++dest) {
                    int offset = (dest - edge) % (2 * length);
                    bool reflected = offset < length;
                    int src = reflected ? edge-1-offset : edge-2*length+offset;
                    cell(dest)[0] = cell(src)[0];
                    cell(dest)[1] = cell(src)[1] ^ (reflected ? 0x40 : 0);
                }
            }
            col = end - 1;
        }
    }
}
void render() {
    static video::Frame state;
    static std::array<uint16_t,video::W*video::H> indices;
    const auto* ram=board::main_ram;
    const auto map=uint32_t(ram[0x1cad4/2])<<16 | ram[0x1cad6/2];
    const bool stage = m68k::stage_active() && ram[0xa038/2] && map>=0x100000 && map<0x200000 && map!=0x1330d0;
    video::snapshot(state);
    if (stage) extend_tile_rows(state);
    video::sprite_layer(state,indices.data(),stage);
    upload(2,128,128,state.video_ram); upload(3,64,40,state.palette);
    upload(4,video::W,video::H,indices.data());
    glBindFramebuffer(GL_FRAMEBUFFER,framebuffer); glViewport(0,0,width,height);
    glUniform4i(scroll,state.bg_xscroll,state.bg_yscroll,state.tx_xscroll,state.tx_yscroll);
    glUniform1i(control,state.ctrl);
    glUniform1i(playable,stage);
    glUniform1i(forest_exit,stage && map==0x133b0e);
    glDrawArrays(GL_TRIANGLES,0,3);
}
void present(SDL_Window* window) {
    int w,h; SDL_GL_GetDrawableSize(window,&w,&h);
    int dw=std::min(w,h*16/9), dh=std::min(h,w*9/16);
    int x=(w-dw)/2,y=(h-dh)/2;
    glBindFramebuffer(GL_READ_FRAMEBUFFER,framebuffer); glBindFramebuffer(GL_DRAW_FRAMEBUFFER,0);
    if (dw!=w || dh!=h) glClear(GL_COLOR_BUFFER_BIT);
    glBlitFramebuffer(0,0,width,height,x,y,x+dw,y+dh,GL_COLOR_BUFFER_BIT,GL_NEAREST);
    SDL_GL_SwapWindow(window);
}
void close() {
    glDeleteFramebuffers(1,&framebuffer); glDeleteTextures(1,&color); glDeleteTextures(5,textures);
    glDeleteVertexArrays(1,&vao); glDeleteProgram(program);
}
}
