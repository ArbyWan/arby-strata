// UnbiasedStrata: the front end's program. Mirrors llama.cpp's tools/server/server.cpp without router mode, MCP,
// server-side tools, the web UI and model downloads.
//
//   arby-strata --engine build/bin/strata-poly -m model.gguf [llama-server options] -- <engine options>
//
// Options before `--` are llama-server's (common_params_parse): -m/--model gives the GGUF the vocabulary and chat
// template are read from, --host/--port/--api-key/--threads-http the listener, --temp/--top-k/... the sampling
// defaults, --reasoning-format/--chat-template-kwargs/--jinja the chat handling. Options after `--` go to the engine
// as they are; `--serve` is added. The engine's READY line gives the context size.

#include "strata_context.hpp"
#include "ui_live.hpp"   // [LIVE-UI] the console block
#include "engine.hpp"    // [LIVE-UI] engine_log_sink: the engine's own log routing

#include "arg.h"
#include "common.h"
#include "llama.h"
#include "log.h"
#include "server-common.h"
#include "server-stream.h"

#include <atomic>
#include <chrono>
#include <clocale>
#include <csignal>
#include <thread>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <string>
#include <thread>
#include <vector>

#ifdef _WIN32
#  ifndef WIN32_LEAN_AND_MEAN
#  define WIN32_LEAN_AND_MEAN
#  endif
#  include <windows.h>
#else
#  include <unistd.h>
#endif

namespace {

std::function<void(int)> shutdown_handler;
std::atomic_flag is_terminating = ATOMIC_FLAG_INIT;

void signal_handler(int signal) {
    if (is_terminating.test_and_set()) {
        std::fprintf(stderr, "Received second interrupt, terminating immediately.\n");
        std::exit(1);
    }
    if (shutdown_handler) {
        shutdown_handler(signal);
    }
}

#ifdef _WIN32
// Windows has no SIGTERM: the console control handler is the equivalent, and CTRL_CLOSE_EVENT is what a window
// close or a logoff sends. Returning TRUE keeps the process alive long enough for the clean shutdown to run.
BOOL WINAPI console_ctrl_handler(DWORD type) {
    switch (type) {
        case CTRL_C_EVENT:
        case CTRL_BREAK_EVENT:
        case CTRL_CLOSE_EVENT:
        case CTRL_SHUTDOWN_EVENT:
            signal_handler((int) type);
            return TRUE;
        default:
            return FALSE;
    }
}
#endif

void usage() {
    std::fprintf(stderr,
        "usage: arby-strata --engine <strata|strata-poly|path> -m <model.gguf> [llama-server options] -- <engine options>\n"
        "\n"
        "  --engine PATH          the engine binary; a bare name is looked up beside this program\n"
        "  -m, --model FILE       the GGUF the vocabulary and chat template are read from (no tensors are loaded)\n"
        "  --host, --port, --api-key, --threads-http, --temp, --top-k, --top-p, --min-p, --reasoning-format,\n"
        "  --chat-template-kwargs, --alias ...   llama-server options (see llama-server --help)\n"
        "  --mmproj FILE          the model's vision projector: image_url parts are encoded in this process and\n"
        "                         handed to the engine (Qwen family); on the CPU (--threads N) unless --mmproj-gpu\n"
        "  --mmproj-gpu           encode on the card: about 1 GB of VRAM plus work buffers, taken before the engine\n"
        "                         sizes its expert cache only if the encoder loads first; for cards with room\n"
        "  --picture-cache N      keep the encoded rows of the last N pictures (default 64, up to 1 GiB) so a chat\n"
        "                         that resends its pictures every turn encodes each once; 0 turns it off\n"
        "  --ui MODE              console block: off (default), plain or live (opt-in; live demotes to plain\n"
        "                         without a VT terminal)\n"
        "  --log-file FILE        where log lines go while the block owns the console\n"
        "  --                     everything after it is passed to the engine; --serve is added\n"
        "\n"
        "example (Qwen3.6-35B-A3B from its GGUF):\n"
        "  arby-strata --engine strata-poly -m Qwen3.6-35B-A3B-UD-Q3_K_M.gguf --port 8080 \\\n"
        "      -- --model Qwen3.6-35B-A3B-UD-Q3_K_M.gguf --max-context 32768\n");
}

std::string self_dir() {
#ifdef _WIN32
    char buf[MAX_PATH * 4];
    const DWORD n = GetModuleFileNameA(nullptr, buf, (DWORD) sizeof buf);
    if (n == 0 || n >= sizeof buf) {
        return ".";
    }
    const std::string p(buf, (size_t) n);
    const size_t slash = p.find_last_of("\\/");
    return slash == std::string::npos ? "." : p.substr(0, slash);
#else
    char buf[4096];
    const ssize_t n = ::readlink("/proc/self/exe", buf, sizeof buf - 1);
    if (n <= 0) {
        return ".";
    }
    buf[n] = 0;
    std::string p(buf);
    const size_t slash = p.find_last_of('/');
    return slash == std::string::npos ? "." : p.substr(0, slash);
#endif
}

bool file_exists(const std::string & p) {
#ifdef _WIN32
    const DWORD attr = GetFileAttributesA(p.c_str());
    return attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY) == 0;
#else
    return ::access(p.c_str(), X_OK) == 0;
#endif
}

} // namespace

int main(int argc, char ** argv) {
    std::setlocale(LC_NUMERIC, "C");
#ifndef _WIN32
    signal(SIGPIPE, SIG_IGN);
#endif

    // split: ours (--engine), llama-server's, and after `--` the engine's
    std::string engine_exe;
    std::vector<std::string> engine_args;
    std::vector<char *> llama_argv;
    bool mmproj_gpu = false; // the vision encoder on the card: only where the engine leaves room (a 48 GB card)
    long picture_cache = 64; // --picture-cache N: encoded pictures kept for the turns that resend them
    std::string ui_mode  = "off";  // [LIVE-UI] off (default) | plain | live (opt-in)
    std::string ui_log;            // [LIVE-UI] where log lines go while the block owns the console
    llama_argv.push_back(argv[0]);
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        if (a == "--") {
            for (++i; i < argc; ++i) {
                engine_args.push_back(argv[i]);
            }
            break;
        }
        if (a == "--engine") {
            if (i + 1 >= argc) { usage(); return 2; }
            engine_exe = argv[++i];
            continue;
        }
        if (a.rfind("--engine=", 0) == 0) {
            engine_exe = a.substr(9);
            continue;
        }
        if (a == "--mmproj-gpu") {
            mmproj_gpu = true;
            continue;
        }
        if (a.rfind("--ui=", 0) == 0) {
            ui_mode = a.substr(5);
            continue;
        }
        if (a == "--log-file") {
            if (i + 1 >= argc) { usage(); return 2; }
            ui_log = argv[++i];
            continue;
        }
        if (a == "--picture-cache") {
            if (i + 1 >= argc) { usage(); return 2; }
            picture_cache = std::strtol(argv[++i], nullptr, 10);
            if (picture_cache < 0) { usage(); return 2; }
            continue;
        }
        llama_argv.push_back(argv[i]);
    }
    if (engine_exe.empty()) {
        usage();
        return 2;
    }
    if (engine_exe.find('/') == std::string::npos && engine_exe.find('\\') == std::string::npos) {
        const std::string beside = self_dir() + "/" + engine_exe;
        if (file_exists(beside)) {
            engine_exe = beside;
        }
    }
    if (!file_exists(engine_exe)) {
        std::fprintf(stderr, "arby-strata: engine not found or not executable: %s\n", engine_exe.c_str());
        return 2;
    }

    common_params params;
    common_init();
    server_stream_session_manager_start();

    if (!common_params_parse((int) llama_argv.size(), llama_argv.data(), params, LLAMA_EXAMPLE_SERVER)) {
        return 1;
    }
    // llama-server encodes images on the GPU unless --no-mmproj-offload; here the card is the engine's, so the
    // encoder runs on the CPU unless --mmproj-gpu asks for the card (upstream PolyStrata's choice as well)
    params.mmproj_use_gpu = mmproj_gpu;
    if (params.model.path.empty()) {
        std::fprintf(stderr, "arby-strata: -m <model.gguf> is required (the vocabulary and chat template come from it)\n");
        return 2;
    }
    auto model_name = params.model.get_name();
    if (params.model_alias.empty() && !model_name.empty()) {
        params.model_alias.insert(model_name);
    }

    // [LIVE-UI] take stdout before the engine starts writing its load banner. A non-TTY or a console
    // without VT demotes itself to plain inside init(); ~ConsoleUI restores the cursor on every exit.
    live_ui::ConsoleUI ui;
    {
        using Mode = live_ui::ConsoleUI::Mode;
        Mode mode = Mode::Live;
        if (ui_mode == "plain") {
            mode = Mode::Plain;
        } else if (ui_mode == "off") {
            mode = Mode::Off;
        } else if (ui_mode != "live") {
            std::fprintf(stderr, "arby-strata: unknown --ui value '%s' (live, plain or off)\n", ui_mode.c_str());
            return 2;
        }
        std::string ui_err;
        if (!ui.init(mode, ui_log, ui_err)) {
            std::fprintf(stderr, "arby-strata: %s\n", ui_err.c_str());
            return 2;
        }
        // [LIVE-UI] SRV_INF/LOG_INF do NOT go through llama_log_set: llama.cpp's common logger has its
        // own sink (common_log_main()). Left alone it writes straight to the console, which is exactly
        // the wall of text the block would have to fight. Point it at the same file.
        if (!ui_log.empty()) {
            common_log_set_file(common_log_main(), ui_log.c_str());
        }
        // [LIVE-UI] and the engine's own stderr is forwarded by a thread straight to our stderr
        // (engine.hpp). Give it a sink so it lands in the log file, line by line.
        if (!ui_log.empty()) {
            auto carry = std::make_shared<std::string>();
            strata_frontend::engine_log_sink() = [&ui, carry](const char * p, size_t n) {
                carry->append(p, n);
                size_t nl;
                while ((nl = carry->find('\n')) != std::string::npos) {
                    std::string line = carry->substr(0, nl);
                    carry->erase(0, nl + 1);
                    if (!line.empty() && line.back() == '\r') line.pop_back();
                    if (!line.empty()) {
                        ui.log_line(line, false);
                        // [status-endpoint] the same line feeds the engine counters behind /status
                        if (auto & sink = strata_telemetry::engine_text_sink()) sink(line);
                    }
                }
                if (carry->size() > 8192) {   // never let a runaway stream grow without bound
                    ui.log_line(*carry, false);
                    carry->clear();
                }
            };
        }
    }

    llama_backend_init();
    llama_numa_init(params.numa);
    // [LIVE-UI] every LOG_* line goes through the console object once the block owns stdout, so no log
    // line can ever be written through the block
    llama_log_set([](ggml_log_level level, const char * text, void * user) {
        live_ui::ConsoleUI::log_callback((int) level, text, user);
    }, &ui);

    strata_context ctx_server;
    ctx_server.set_picture_cache((size_t) picture_cache, (size_t) 1 << 30);
    server_http_context ctx_http;
    if (!ctx_http.init(params)) {
        SRV_ERR("%s", "failed to initialize HTTP server\n");
        return 1;
    }

    // routes first (handlers are not called until is_ready), then the engine (forked before any thread exists),
    // then the HTTP thread, which answers /health with 503 while the engine loads
    ctx_server.set_ui(&ui);   // [LIVE-UI]
    ctx_server.init_routes(ctx_http);

    std::string err;
    if (!ctx_server.start_engine(params, engine_exe, engine_args, err)) {
        SRV_ERR("could not start the engine: %s\n", err.c_str());
        return 1;
    }

    // [LIVE-UI] the counters need their own beat: the engine read loop blocks on the pipe
    std::atomic<bool> ui_ticking{true};
    std::thread ui_ticker([&] {
        while (ui_ticking.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            ctx_server.tick_ui();
        }
    });

    auto clean_up = [&]() {
        SRV_INF("%s", "cleaning up before exit...\n");
        ui_ticking.store(false);
        if (ui_ticker.joinable()) ui_ticker.join();
        server_stream_session_manager_stop();
        ctx_http.stop();
        ctx_server.terminate();
        ui.shutdown();   // [LIVE-UI] cursor back, block cleared
        llama_backend_free();
    };

    if (!ctx_http.start()) {
        clean_up();
        SRV_ERR("%s", "exiting due to HTTP server error\n");
        return 1;
    }

    if (!ctx_server.load(params, err)) {
        SRV_ERR("%s\n", err.c_str());
        clean_up();
        if (ctx_http.thread.joinable()) {
            ctx_http.thread.join();
        }
        return 1;
    }
    ctx_http.is_ready.store(true);
    SRV_INF("listening on %s, context %lld tokens\n", ctx_http.listening_address.c_str(), ctx_server.n_ctx());
    // [LIVE-UI] the single info line above the frame
    ui.log_line("arby-strata  :" + std::to_string(params.port) + "  " +
                (params.model_alias.empty() ? std::string("model") : *params.model_alias.begin()) +
                "  ctx " + std::to_string(ctx_server.n_ctx()) +
                (ui_log.empty() ? std::string() : "   log: " + ui_log));

    shutdown_handler = [&](int) {
        ctx_server.terminate();
    };
#ifdef _WIN32
    SetConsoleCtrlHandler(console_ctrl_handler, TRUE);
#else
    struct sigaction sigint_action;
    sigint_action.sa_handler = signal_handler;
    sigemptyset(&sigint_action.sa_mask);
    sigint_action.sa_flags = 0;
    sigaction(SIGINT,  &sigint_action, NULL);
    sigaction(SIGTERM, &sigint_action, NULL);
#endif

    // the engine's exit ends this process too, with a non-zero code, so a unit with Restart=on-failure brings both
    // back. A replacement engine is not forked from this multithreaded process (UnbiasedStrata 1 main.cpp:873).
    std::atomic<bool> engine_died{false};
    std::atomic<bool> stop_supervisor{false};
    std::thread supervisor([&]() {
        std::string why;
        while (!stop_supervisor) {
            if (ctx_server.engine_exited(why)) {
                SRV_ERR("%s; shutting down so the service manager can restart\n", why.c_str());
                engine_died = true;
                ctx_server.terminate();
                return;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    });

    ctx_server.start_loop(); // blocks until terminate()

    stop_supervisor = true;
    supervisor.join();
    clean_up();
    if (ctx_http.thread.joinable()) {
        ctx_http.thread.join();
    }
    return engine_died ? 1 : 0;
}
