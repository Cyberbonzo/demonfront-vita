#include "board.h"
#include "scheduler.h"
#include "sound.h"
#include "romset.h"
#include "video.h"
#include <SDL.h>
#ifdef DF_WIDE
#include "hd_video.h"
#endif
#include <algorithm>
#include <array>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <thread>
#include <vector>

static volatile std::sig_atomic_t stopping = 0;
static void stop(int) { stopping = 1; }

static bool load_words(const char* name, std::vector<uint16_t>& words) {
    std::vector<uint8_t> bytes;
    if (!romset::read(name, bytes) || bytes.size() % 2) return false;
    words.resize(bytes.size() / 2);
    std::memcpy(words.data(), bytes.data(), bytes.size());
    return true;
}

using Ram = std::array<uint16_t, sizeof board::main_ram / sizeof(uint16_t)>;
static Ram saved{};
static bool restored = false;
static void restore() {
    std::ifstream in("sram.bin", std::ios::binary | std::ios::ate);
    if (!in || in.tellg() != sizeof saved) return;
    in.seekg(0);
    if (in.read(reinterpret_cast<char*>(saved.data()), sizeof saved)) {
        std::memcpy(board::main_ram, saved.data(), sizeof saved);
        restored = true;
    }
}
static void save() {
    if (restored && !std::memcmp(board::main_ram, saved.data(), sizeof saved)) return;
    std::ofstream out("sram.bin.tmp", std::ios::binary | std::ios::trunc);
    out.write(reinterpret_cast<const char*>(board::main_ram), sizeof saved);
    out.close();
    if (out && std::rename("sram.bin.tmp","sram.bin") == 0) {
        std::memcpy(saved.data(), board::main_ram, sizeof saved);
        restored = true;
    } else std::perror("sram.bin");
}

static void input(SDL_GameController* pad) {
    const auto k = SDL_GetKeyboardState(nullptr);
    auto button = [pad](SDL_GameControllerButton b) { return pad && SDL_GameControllerGetButton(pad,b); };
    int x = pad ? SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_LEFTX) : 0;
    int y = pad ? SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_LEFTY) : 0;
    unsigned bits = 0;
    if (k[SDL_SCANCODE_RETURN] || button(SDL_CONTROLLER_BUTTON_START)) bits |= 1;
    if (k[SDL_SCANCODE_UP] || button(SDL_CONTROLLER_BUTTON_DPAD_UP) || y < -14000) bits |= 2;
    if (k[SDL_SCANCODE_DOWN] || button(SDL_CONTROLLER_BUTTON_DPAD_DOWN) || y > 14000) bits |= 4;
    if (k[SDL_SCANCODE_LEFT] || button(SDL_CONTROLLER_BUTTON_DPAD_LEFT) || x < -14000) bits |= 8;
    if (k[SDL_SCANCODE_RIGHT] || button(SDL_CONTROLLER_BUTTON_DPAD_RIGHT) || x > 14000) bits |= 16;
    if (k[SDL_SCANCODE_Z] || button(SDL_CONTROLLER_BUTTON_A)) bits |= 32;
    if (k[SDL_SCANCODE_X] || button(SDL_CONTROLLER_BUTTON_B)) bits |= 64;
    if (k[SDL_SCANCODE_C] || button(SDL_CONTROLLER_BUTTON_X) || button(SDL_CONTROLLER_BUTTON_LEFTSHOULDER)) bits |= 128;
    board::in_p1p2 = ~bits;
    board::in_service = (k[SDL_SCANCODE_V] || button(SDL_CONTROLLER_BUTTON_Y) || button(SDL_CONTROLLER_BUTTON_RIGHTSHOULDER)) ? 0xfeff : 0xffff;
}
int main() {
    std::filesystem::current_path(std::filesystem::canonical("/proc/self/exe").parent_path());
    romset::init(".");
    if (!video::load_gfx() || !sound::load() || !romset::arm_rom(board::arm_rom) ||
        !load_words("pgm_p02s.u20",board::bios) || !load_words("v105_16m.u5",board::game)) return 1;
    board::arm_rom.resize(0x800000,0xff);
    board::arm_init(); board::m68k_init(); restore(); sched::init();
    board::in_dsw &= ~0x0008;
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER | SDL_INIT_AUDIO)) {
        std::fprintf(stderr,"%s\n",SDL_GetError()); return 1;
    }
#ifdef DF_WIDE
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_CORE);
    auto win=SDL_CreateWindow("Demon Front",SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,1920,1080,
                              SDL_WINDOW_OPENGL | SDL_WINDOW_FULLSCREEN_DESKTOP);
    auto gl=win ? SDL_GL_CreateContext(win) : nullptr;
    if (!gl || !hd::init(win)) { std::fprintf(stderr,"HD display: %s\n",SDL_GetError()); return 1; }
    SDL_GL_SetSwapInterval(0);
    constexpr auto period=std::chrono::nanoseconds(16666667);
#else
    auto win = SDL_CreateWindow("Demon Front",SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,1920,1080,SDL_WINDOW_FULLSCREEN_DESKTOP);
    auto renderer = win ? SDL_CreateRenderer(win,-1,SDL_RENDERER_ACCELERATED) : nullptr;
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY,"linear");
    auto tex = renderer ? SDL_CreateTexture(renderer,SDL_PIXELFORMAT_ABGR8888,SDL_TEXTUREACCESS_STREAMING,video::W,video::H) : nullptr;
    if (!tex) { std::fprintf(stderr,"%s\n",SDL_GetError()); return 1; }
    constexpr auto period=std::chrono::microseconds(16896);
#endif
    SDL_ShowCursor(SDL_DISABLE);
    SDL_AudioSpec spec{}; spec.freq = 48000; spec.format = AUDIO_S16SYS; spec.channels = 2; spec.samples = 512;
    auto audio = SDL_OpenAudioDevice(nullptr,0,&spec,nullptr,0);
    if (!audio) { std::fprintf(stderr,"%s\n",SDL_GetError()); return 1; }
    SDL_GameController* pad = nullptr;
    bool paused = false;
    std::signal(SIGTERM,stop); std::signal(SIGINT,stop);
    using clock = std::chrono::steady_clock;
    auto next = clock::now();
#ifndef DF_WIDE
    static uint32_t pixels[video::W*video::H];
#endif
    SDL_AudioStream* stream=nullptr;
    int streamRate=0;
    unsigned counted=0;
    auto measured=clock::now();
    static int16_t samples[2048];
    unsigned frames = 0;
    bool started_audio = false;
    while (!stopping) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) stopping = 1;
            if (e.type == SDL_CONTROLLERDEVICEREMOVED && pad && !SDL_GameControllerGetAttached(pad)) {
                SDL_GameControllerClose(pad); pad = nullptr;
            }
            if (!pad && (e.type == SDL_CONTROLLERDEVICEADDED || e.type == SDL_CONTROLLERDEVICEREMOVED)) {
                for (int i=0;i<SDL_NumJoysticks() && !pad;i++)
                    if (SDL_IsGameController(i)) pad = SDL_GameControllerOpen(i);
                if (pad) std::fprintf(stderr,"Controle: %s\n",SDL_GameControllerName(pad));
            }
            if ((e.type == SDL_CONTROLLERBUTTONDOWN && e.cbutton.button == SDL_CONTROLLER_BUTTON_BACK) ||
                (e.type == SDL_KEYDOWN && !e.key.repeat && e.key.keysym.sym == SDLK_ESCAPE)) {
                paused = !paused; if (paused) save();
                SDL_PauseAudioDevice(audio,paused || !started_audio);
            }
        }
        input(pad);
        if (!paused) {
            sched::run_frame();
#ifdef DF_WIDE
            hd::render();
#else
            video::render(pixels);
            SDL_UpdateTexture(tex,nullptr,pixels,video::W*4);
#endif
            const double outputPeriod=std::chrono::duration<double>(period).count();
            int rate=int(sound::rate()*0.016896/outputPeriod+0.5);
            if (rate!=streamRate) {
                if (stream) SDL_FreeAudioStream(stream);
                stream=SDL_NewAudioStream(AUDIO_S16SYS,2,rate,AUDIO_S16SYS,2,48000);
                if (!stream) { std::fprintf(stderr,"%s\n",SDL_GetError()); stopping=1; continue; }
                streamRate=rate;
            }
            int count;
            while ((count = sound::pull(samples,1024)) > 0) SDL_AudioStreamPut(stream,samples,count*4);
            while ((count=SDL_AudioStreamGet(stream,samples,sizeof(samples)))>0)
                SDL_QueueAudio(audio,samples,count);
            if (!started_audio && SDL_GetQueuedAudioSize(audio) >= 4096) {
                SDL_PauseAudioDevice(audio,0); started_audio=true;
            }
            if (++frames % 592 == 0) save();
        }
#ifdef DF_WIDE
        hd::present(win);
#else
        int w,h; SDL_GetRendererOutputSize(renderer,&w,&h);
        int dw=std::min(w,h*4/3), dh=std::min(h,w*3/4);
        SDL_Rect area{(w-dw)/2,(h-dh)/2,dw,dh};
        SDL_RenderClear(renderer); SDL_RenderCopy(renderer,tex,nullptr,&area); SDL_RenderPresent(renderer);
#endif
        if (++counted==600) {
            auto now=clock::now();
            std::fprintf(stderr,"Presentation %.2f fps, audio queue %.1f ms\n",
                         counted/std::chrono::duration<double>(now-measured).count(),SDL_GetQueuedAudioSize(audio)/192.0);
            counted=0; measured=now;
        }
        next += period;
        if (clock::now() > next + std::chrono::milliseconds(67)) next=clock::now();
        std::this_thread::sleep_until(next);
    }
    save();
    if (pad) SDL_GameControllerClose(pad);
    if (stream) SDL_FreeAudioStream(stream);
    SDL_CloseAudioDevice(audio);
#ifdef DF_WIDE
    hd::close(); SDL_GL_DeleteContext(gl);
#else
    SDL_DestroyTexture(tex); SDL_DestroyRenderer(renderer);
#endif
    SDL_DestroyWindow(win); SDL_Quit();
}

