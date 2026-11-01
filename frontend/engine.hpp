// arby-strata: the engine as a child process (from UnbiasedStrata's frontend/engine.hpp).
//
// The engines (`strata`, `strata-poly`) speak lines over stdin/stdout: `GEN <max_new> key=value... <id,id,...>` in,
// `T <id>` per token, `PP`, `RESUME`, `DONE ...`, `ERR ...` out, `STOP` to cancel, `QUIT` to end (docs/BACKENDS.md,
// "How a backend works"). This class owns one such child. It never blocks longer than the caller allows: the server
// loop polls it between tasks so a cancel from a disconnected client is seen within the poll interval.
//
// Two implementations behind one interface:
//   POSIX   - pipe()/fork()/execv(), poll() on the child's stdout.
//   Windows - CreateProcessA with inheritable anonymous pipes and a reader thread that blocks in ReadFile, so
//             `read_line` waits on a condition variable instead of spinning (Windows has no poll()). The child is
//             put in a job object with KILL_ON_JOB_CLOSE, so the engine dies with this process even on a hard kill.
#pragma once

#include <atomic>
#include <algorithm>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <vector>

#ifdef _WIN32
#  ifndef WIN32_LEAN_AND_MEAN
#  define WIN32_LEAN_AND_MEAN
#  endif
#  ifndef NOMINMAX
#  define NOMINMAX
#  endif
#  include <windows.h>
#  include <chrono>
#  include <condition_variable>
#  include <mutex>
#  include <thread>
#else
#  include <csignal>
#  include <fcntl.h>
#  include <poll.h>
#  include <sys/wait.h>
#  include <unistd.h>
#endif

namespace strata_frontend {

#ifndef _WIN32
// Reap a dead child: kill(pid, 0) would report a zombie as alive. The pid slot is shared with quit() on another
// thread (the supervisor polls this while a SIGTERM handler shuts down), so it is read once and cleared by
// compare-exchange. Nothing here or in quit() may pass a pid that is not > 0 to kill(): kill(-1, SIGKILL) kills every
// process the user owns (it took out two whole login sessions on 2026-10-17).
inline bool child_running(std::atomic<pid_t> & pid_slot, std::string & why) {
    pid_t pid = pid_slot.load();
    if (pid <= 0) {
        return false;
    }
    int status = 0;
    const pid_t result = ::waitpid(pid, &status, WNOHANG);
    if (result == 0 || (result < 0 && errno == EINTR)) {
        return true;
    }
    if (result < 0) {
        why = std::string("waitpid: ") + std::strerror(errno);
    } else if (WIFSIGNALED(status)) {
        why = "engine killed by signal " + std::to_string(WTERMSIG(status));
    } else if (WIFEXITED(status)) {
        why = "engine exited with code " + std::to_string(WEXITSTATUS(status));
    } else {
        why = "engine exited unexpectedly";
    }
    pid_slot.compare_exchange_strong(pid, -1);
    return false;
}
#endif

// One line from the engine, already classified.
struct EngineLine {
    enum Kind { NONE, TOKEN, PP, RESUME, REUSED, DONE, ERR, INFO, OTHER, EOF_ } kind = NONE;
    std::string raw;
    int32_t token = -1;
    // PP <done> <total> [<ms> <tok/s>]
    long long pp_done = 0, pp_total = 0;
    // RESUME <n> / REUSED <n>
    long long n = 0;
    // DONE <generated> <prompt> <prompt_ms> <decode_ms> <finish> [<drafts_accepted> <drafts_offered> <reused> ...]
    long long produced = 0, prompt_n = 0, draft_accepted = 0, draft_offered = 0, reused = 0;
    double prompt_ms = 0, decode_ms = 0;
    std::string finish;   // stop | length | cancel | nonfinite
    std::string message;  // ERR text
};

class Engine {
public:
    Engine() = default;
    Engine(const Engine &) = delete;
    Engine & operator=(const Engine &) = delete;
    ~Engine() { quit(); }

    // Starts `executable` with `args` (args[0] is argv[0]), its stdout/stdin on pipes.
    bool start(const std::string & executable, const std::vector<std::string> & args, std::string & err) {
#ifdef _WIN32
        return start_windows(executable, args, err);
#else
        int to_child[2], from_child[2];
        if (::pipe(to_child) != 0 || ::pipe(from_child) != 0) {
            err = std::string("pipe: ") + std::strerror(errno);
            return false;
        }
        const pid_t pid = ::fork();
        if (pid < 0) {
            err = std::string("fork: ") + std::strerror(errno);
            return false;
        }
        if (pid == 0) {
            ::dup2(to_child[0], STDIN_FILENO);
            ::dup2(from_child[1], STDOUT_FILENO);
            ::close(to_child[0]); ::close(to_child[1]); ::close(from_child[0]); ::close(from_child[1]);
            std::vector<char *> argv;
            for (const std::string & a : args) {
                argv.push_back(const_cast<char *>(a.c_str()));
            }
            argv.push_back(nullptr);
            ::execv(executable.c_str(), argv.data());
            std::fprintf(stderr, "arby-strata: could not start the engine %s: %s\n", executable.c_str(), std::strerror(errno));
            ::_exit(127);
        }
        pid_ = pid;
        ::close(to_child[0]);
        ::close(from_child[1]);
        in_fd_  = to_child[1];
        out_fd_ = from_child[0];
        alive_  = true;
        return true;
#endif
    }

    // Reads start-up lines until READY. Loading a model takes minutes; `timeout_s` bounds the wait (<= 0: forever).
    // `READY <max_context> [stop]`: the context the engine actually allocated (its VRAM plan may lower the request);
    // `stop` means it honours STOP mid-request. INFO lines land in `info`.
    bool wait_ready(std::string & err, int timeout_s = 0) {
        const int64_t deadline = timeout_s > 0 ? now_ms() + (int64_t) timeout_s * 1000 : 0;
        std::string line;
        while (true) {
            int wait_ms = 1000;
            if (deadline) {
                const int64_t left = deadline - now_ms();
                if (left <= 0) {
                    err = "the engine did not become ready within " + std::to_string(timeout_s) + " s";
                    return false;
                }
                wait_ms = (int) std::min<int64_t>(left, 1000);
            }
            const int rc = read_line(line, wait_ms);
            if (rc < 0) {
                err = "the engine exited before it was ready (its messages are above)";
#ifdef _WIN32
                // The exit code names the cause when nothing reached the pipes: 0xC0000135 (missing DLL),
                // 0xC0000139 (entry point not found), 1 (usage), or the engine's own.
                const long long code = exit_code();
                if (code >= 0) {
                    char hex[32];
                    std::snprintf(hex, sizeof hex, "0x%08llX", (unsigned long long) code);
                    err += std::string(", exit code ") + std::to_string(code) + " (" + hex + ")";
                }
#endif
                return false;
            }
            if (rc == 0) {
                continue;
            }
            if (line.rfind("READY", 0) == 0) {
                char stop[16] = {0};
                long long ctx = 0;
                std::sscanf(line.c_str(), "READY %lld %15s", &ctx, stop);
                max_context_ = ctx;
                can_stop_    = std::strcmp(stop, "stop") == 0;
                ready_       = true;
                return true;
            }
            if (line.rfind("INFO ", 0) == 0) {
                parse_info(line.substr(5));
                continue;
            }
            if (line.rfind("ERR", 0) == 0) {
                err = line.size() > 4 ? line.substr(4) : "the engine reported an error";
                return false;
            }
            if (line.rfind("FATAL", 0) == 0) {
                err = line;
                return false;
            }
            // anything else before READY is chatter on stdout; the engine's log is on stderr
        }
    }

    // Writes one request line. `header` is `GEN <max_new> key=value...` or `GENI <max_new> key=value... <file>`.
    bool send_request(const std::string & header, const std::vector<int32_t> & ids) {
        std::string req = header;
        req.reserve(header.size() + ids.size() * 7 + 2);
        req += ' ';
        for (size_t i = 0; i < ids.size(); ++i) {
            if (i) {
                req += ',';
            }
            req += std::to_string(ids[i]);
        }
        req += '\n';
        busy_ = true;
        return send(req);
    }

    // Asks the engine to end the current request. Its output continues until a DONE with finish=cancel.
    bool stop() {
        return send("STOP\n");
    }

    // One classified line, waiting at most `timeout_ms` (0: only what is buffered). Returns false on timeout.
    // kind == EOF_ when the engine closed its stdout (it exited).
    bool next_line(EngineLine & out, int timeout_ms) {
        std::string line;
        const int rc = read_line(line, timeout_ms);
        if (rc == 0) {
            return false;
        }
        out = EngineLine{};
        out.raw = line;
        if (rc < 0) {
            out.kind = EngineLine::EOF_;
            return true;
        }
        classify(line, out);
        if (out.kind == EngineLine::DONE || out.kind == EngineLine::ERR) {
            busy_ = false;
        }
        return true;
    }

    // One command between requests with a one-line reply whose first word is `reply_prefix` (`STATS`, `VRAM`).
    bool command(const std::string & line, const char * reply_prefix, std::string & reply, std::string & err, int timeout_ms = 5000) {
        if (!alive_) { err = "the engine is not running"; return false; }
        if (!send(line + "\n")) { err = "the engine is not running"; return false; }
        std::string got;
        const int64_t deadline = now_ms() + timeout_ms;
        while (true) {
            const int64_t left = deadline - now_ms();
            if (left <= 0) { err = "no reply from the engine"; return false; }
            const int rc = read_line(got, (int) left);
            if (rc < 0) { err = "the engine exited"; return false; }
            if (rc == 0) { continue; }
            if (got.rfind(reply_prefix, 0) == 0) { reply = got; return true; }
            if (got.rfind("ERR", 0) == 0) { err = got.size() > 4 ? got.substr(4) : "the engine reported an error"; return false; }
        }
    }

    // Stop the engine: QUIT, up to 10 s for it to leave, then kill it. Safe against reap_exit() on another thread.
    void quit() {
#ifdef _WIN32
        quit_windows();
#else
        // Take the pid out of the shared slot first: from here only this call owns the child, and kill() below
        // targets this positive local copy, never the slot (which reads -1 once the supervisor has reaped it).
        const pid_t pid = pid_.exchange(-1);
        if (pid > 0) {
            send("QUIT\n");
            int status = 0;
            pid_t reaped = 0;
            for (int i = 0; i < 100 && (reaped = ::waitpid(pid, &status, WNOHANG)) == 0; ++i) {
                ::usleep(100 * 1000);
            }
            if (reaped == 0) {   // still running; -1 means another thread already reaped it
                ::kill(pid, SIGKILL);
                ::waitpid(pid, &status, 0);
            }
        }
        if (in_fd_  >= 0) { ::close(in_fd_);  in_fd_  = -1; }
        if (out_fd_ >= 0) { ::close(out_fd_); out_fd_ = -1; }
        alive_ = false;
#endif
    }

    // Non-blocking: has the child exited? `why` names the signal or code.
    bool reap_exit(std::string & why) {
#ifdef _WIN32
        HANDLE h = nullptr;
        {
            std::lock_guard<std::mutex> lk(handle_mu_);
            h = proc_;
            if (h == nullptr) {
                return false;
            }
            if (WaitForSingleObject(h, 0) == WAIT_TIMEOUT) {
                return false;   // still running (the lock is held across a non-blocking wait only)
            }
            DWORD code = 0;
            GetExitCodeProcess(h, &code);
            CloseHandle(h);
            proc_ = nullptr;
            pid_.store(-1);
            why = "engine exited with code " + std::to_string((long long) code);
        }
        alive_ = false;
        return true;
#else
        if (child_running(pid_, why)) {
            return false;
        }
        alive_ = false;
        return true;
#endif
    }

#ifdef _WIN32
    // The child's exit code once it has exited, -1 while it is running (or when we no longer hold the handle).
    long long exit_code() {
        std::lock_guard<std::mutex> lk(handle_mu_);
        if (proc_ == nullptr) {
            return -1;
        }
        DWORD code = 0;
        if (!GetExitCodeProcess(proc_, &code) || code == STILL_ACTIVE) {
            return -1;
        }
        return (long long) code;
    }
#endif

    bool alive() const { return alive_; }
    bool busy()  const { return busy_; }
    bool ready() const { return ready_; }
    bool can_stop() const { return can_stop_; }
    long long max_context() const { return max_context_; }
    const std::map<std::string, std::string> & info() const { return info_; }
    int64_t pid() const { return pid_; }

    static int64_t now_ms() {
#ifdef _WIN32
        return (int64_t) GetTickCount64();
#else
        struct timespec ts;
        ::clock_gettime(CLOCK_MONOTONIC, &ts);
        return (int64_t) ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
#endif
    }

private:
#ifdef _WIN32
    static std::string quote_arg(const std::string & a) {
        if (!a.empty() && a.find_first_of(" \t\"") == std::string::npos) {
            return a;
        }
        std::string out = "\"";
        size_t backslashes = 0;
        for (const char c : a) {
            if (c == '\\') {
                ++backslashes;
                continue;
            }
            if (c == '"') {
                out.append(backslashes * 2 + 1, '\\');
                out += '"';
                backslashes = 0;
                continue;
            }
            out.append(backslashes, '\\');
            backslashes = 0;
            out += c;
        }
        out.append(backslashes * 2, '\\');
        out += '"';
        return out;
    }

    static std::string win_error(const char * what) {
        const DWORD e = GetLastError();
        return std::string(what) + " failed, Windows error " + std::to_string((long long) e);
    }

    bool start_windows(const std::string & executable, const std::vector<std::string> & args, std::string & err) {
        SECURITY_ATTRIBUTES sa{};
        sa.nLength        = sizeof(SECURITY_ATTRIBUTES);
        sa.lpSecurityDescriptor = nullptr;
        sa.bInheritHandle = TRUE;

        HANDLE child_in_r = nullptr, child_in_w = nullptr, child_out_r = nullptr, child_out_w = nullptr;
        HANDLE child_err_r = nullptr, child_err_w = nullptr;
        if (!CreatePipe(&child_in_r, &child_in_w, &sa, 0) ||
            !CreatePipe(&child_out_r, &child_out_w, &sa, 0) ||
            !CreatePipe(&child_err_r, &child_err_w, &sa, 0)) {
            err = win_error("CreatePipe");
            return false;
        }
        // only the child's ends are inherited
        SetHandleInformation(child_in_w, HANDLE_FLAG_INHERIT, 0);
        SetHandleInformation(child_out_r, HANDLE_FLAG_INHERIT, 0);
        SetHandleInformation(child_err_r, HANDLE_FLAG_INHERIT, 0);

        // argv[0] convention: the caller passes args[0] = the executable name (POSIX execv semantics). On POSIX
        // that is argv[0]; on Windows the command line's first token IS argv[0], so emitting the executable and
        // then args[0] again would hand the engine its own path as its first real argument - it then exits with a
        // usage error before it prints anything.
        std::string cmd;
        if (!args.empty()) {
            cmd = quote_arg(args[0]);
            for (size_t i = 1; i < args.size(); ++i) {
                cmd += ' ';
                cmd += quote_arg(args[i]);
            }
        } else {
            cmd = quote_arg(executable);
        }
        std::vector<char> cmdline(cmd.begin(), cmd.end());
        cmdline.push_back('\0');
        std::fprintf(stderr, "arby-strata: engine command line: %s\n", cmd.c_str());
        std::fflush(stderr);

        // The engine's log is carried on its own pipe and forwarded by a thread (see start_error_reader): a console
        // stderr handle is not reliably inheritable, and a child that dies during load taking its diagnosis with it
        // is exactly how the first Windows run failed - "its messages are above" with nothing above.
        STARTUPINFOA si{};
        si.cb          = sizeof(STARTUPINFOA);
        si.dwFlags     = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
        si.wShowWindow = SW_HIDE;
        si.hStdInput   = child_in_r;
        si.hStdOutput  = child_out_w;
        si.hStdError   = child_err_w;   // the engine's log, forwarded to ours

        PROCESS_INFORMATION pi{};
        if (!CreateProcessA(executable.c_str(), cmdline.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW,
                            nullptr, nullptr, &si, &pi)) {
            err = win_error(("could not start " + executable).c_str());
            CloseHandle(child_in_r); CloseHandle(child_in_w);
            CloseHandle(child_out_r); CloseHandle(child_out_w);
            CloseHandle(child_err_r); CloseHandle(child_err_w);
            return false;
        }
        CloseHandle(pi.hThread);
        CloseHandle(child_in_r);
        CloseHandle(child_out_w);
        CloseHandle(child_err_w);
        err_r_ = child_err_r;

        // The engine must not outlive this process, even on a hard kill: a job object with KILL_ON_JOB_CLOSE does
        // what the process group does on POSIX. Failing to assign is not fatal (we may already be in a job).
        {
            std::lock_guard<std::mutex> lk(handle_mu_);
            job_ = CreateJobObjectA(nullptr, nullptr);
            if (job_ != nullptr) {
                JOBOBJECT_EXTENDED_LIMIT_INFORMATION li{};
                li.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
                SetInformationJobObject(job_, JobObjectExtendedLimitInformation, &li, sizeof li);
                AssignProcessToJobObject(job_, pi.hProcess);
            }
            proc_ = pi.hProcess;
            pid_.store((int64_t) pi.dwProcessId);
        }
        in_w_  = child_in_w;
        out_r_ = child_out_r;
        alive_ = true;
        start_reader();
        start_error_reader();
        return true;
    }

    void start_reader() {
        reader_ = std::thread([this]() {
            char chunk[65536];
            while (true) {
                DWORD got = 0;
                if (!ReadFile(out_r_, chunk, (DWORD) sizeof chunk, &got, nullptr) || got == 0) {
                    break;
                }
                {
                    std::lock_guard<std::mutex> lk(mu_);
                    buf_.append(chunk, (size_t) got);
                }
                cv_.notify_all();
            }
            {
                std::lock_guard<std::mutex> lk(mu_);
                eof_ = true;
            }
            cv_.notify_all();
        });
    }

    // The engine's stderr, forwarded to ours as it arrives. ReadFile blocks, so this thread lives until the child
    // closes the pipe - which a start-up failure does immediately, still delivering the message.
    void start_error_reader() {
        err_reader_ = std::thread([this]() {
            char chunk[4096];
            size_t total = 0;
            while (true) {
                DWORD got = 0;
                if (!ReadFile(err_r_, chunk, (DWORD) sizeof chunk, &got, nullptr) || got == 0) {
                    break;
                }
                total += (size_t) got;
                std::fwrite(chunk, 1, (size_t) got, stderr);
                std::fflush(stderr);
            }
            if (total == 0) {
                std::fprintf(stderr, "arby-strata: the engine wrote nothing to stderr\n");
                std::fflush(stderr);
            }
        });
    }

    void quit_windows() {
        HANDLE h = nullptr;
        {
            std::lock_guard<std::mutex> lk(handle_mu_);
            h = proc_;
            proc_ = nullptr;
            pid_.store(-1);
        }
        if (h != nullptr) {
            send("QUIT\n");
            if (WaitForSingleObject(h, 10000) != WAIT_OBJECT_0) {
                TerminateProcess(h, 1);
                WaitForSingleObject(h, 5000);
            }
            CloseHandle(h);
        }
        if (in_w_ != nullptr) { CloseHandle(in_w_);  in_w_  = nullptr; }
        if (out_r_ != nullptr) {
            // the child is gone, so its stdout is closed and the reader sees EOF by itself; closing the handle
            // here would race that ReadFile, so only join.
            if (reader_.joinable()) {
                reader_.join();
            }
            CloseHandle(out_r_);
            out_r_ = nullptr;
        } else if (reader_.joinable()) {
            reader_.join();
        }
        if (err_r_ != nullptr) {
            if (err_reader_.joinable()) {
                err_reader_.join();
            }
            CloseHandle(err_r_);
            err_r_ = nullptr;
        } else if (err_reader_.joinable()) {
            err_reader_.join();
        }
        if (job_ != nullptr) {
            CloseHandle(job_);   // KILL_ON_JOB_CLOSE: nothing of the engine survives this
            job_ = nullptr;
        }
        alive_ = false;
    }

    bool send(const std::string & s) {
        if (in_w_ == nullptr) {
            return false;
        }
        std::lock_guard<std::mutex> lk(write_mu_);
        size_t off = 0;
        while (off < s.size()) {
            DWORD wrote = 0;
            if (!WriteFile(in_w_, s.data() + off, (DWORD) (s.size() - off), &wrote, nullptr) || wrote == 0) {
                alive_ = false;
                return false;
            }
            off += (size_t) wrote;
        }
        return true;
    }

    // 1: a line; 0: timeout; -1: EOF/error
    int read_line(std::string & line, int timeout_ms) {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms <= 0 ? 0 : timeout_ms);
        std::unique_lock<std::mutex> lk(mu_);
        auto take_line = [&]() -> bool {
            const size_t nl = buf_.find('\n');
            if (nl == std::string::npos) {
                return false;
            }
            line.assign(buf_, 0, nl);
            buf_.erase(0, nl + 1);
            while (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }
            return true;
        };
        while (true) {
            if (take_line()) {
                return 1;
            }
            if (eof_) {
                if (!buf_.empty()) {   // a last line without its newline
                    line.swap(buf_);
                    buf_.clear();
                    return 1;
                }
                alive_ = false;
                return -1;
            }
            if (timeout_ms <= 0) {
                return 0;
            }
            if (cv_.wait_until(lk, deadline) == std::cv_status::timeout) {
                if (take_line()) {
                    return 1;
                }
                return 0;
            }
        }
    }

    int64_t proc_pid() const { return pid_.load(); }

    HANDLE proc_ = nullptr;
    HANDLE job_  = nullptr;
    HANDLE in_w_ = nullptr;
    HANDLE out_r_ = nullptr;
    HANDLE err_r_ = nullptr;
    std::thread reader_;
    std::thread err_reader_;
    std::mutex mu_;            // guards buf_ and eof_
    std::mutex write_mu_;      // one writer at a time on the engine's stdin
    std::mutex handle_mu_;     // guards proc_ (the supervisor and quit() share it)
    std::condition_variable cv_;
    bool eof_ = false;
#else
    bool send(const std::string & s) {
        if (in_fd_ < 0) {
            return false;
        }
        size_t off = 0;
        while (off < s.size()) {
            const ssize_t n = ::write(in_fd_, s.data() + off, s.size() - off);
            if (n < 0 && errno == EINTR) {
                continue;
            }
            if (n <= 0) {
                alive_ = false;
                return false;
            }
            off += (size_t) n;
        }
        return true;
    }

    // 1: a line; 0: timeout; -1: EOF/error
    int read_line(std::string & line, int timeout_ms) {
        const int64_t deadline = now_ms() + timeout_ms;
        while (true) {
            const size_t nl = buf_.find('\n');
            if (nl != std::string::npos) {
                line.assign(buf_, 0, nl);
                buf_.erase(0, nl + 1);
                while (!line.empty() && line.back() == '\r') {
                    line.pop_back();
                }
                return 1;
            }
            if (out_fd_ < 0) {
                return -1;
            }
            int wait = timeout_ms <= 0 ? 0 : (int) std::max<int64_t>(0, deadline - now_ms());
            struct pollfd pfd = { out_fd_, POLLIN, 0 };
            const int pr = ::poll(&pfd, 1, wait);
            if (pr < 0) {
                if (errno == EINTR) {
                    continue;
                }
                alive_ = false;
                return -1;
            }
            if (pr == 0) {
                return 0;
            }
            char chunk[65536];
            const ssize_t n = ::read(out_fd_, chunk, sizeof chunk);
            if (n < 0 && errno == EINTR) {
                continue;
            }
            if (n <= 0) {
                // EOF: the engine closed stdout. Hand out what is buffered without a newline, then -1.
                if (!buf_.empty()) {
                    line.swap(buf_);
                    buf_.clear();
                    return 1;
                }
                alive_ = false;
                return -1;
            }
            buf_.append(chunk, (size_t) n);
        }
    }

    int in_fd_   = -1;
    int out_fd_  = -1;
#endif

    void parse_info(const std::string & kv) {
        size_t i = 0;
        while (i < kv.size()) {
            while (i < kv.size() && kv[i] == ' ') ++i;
            const size_t j = kv.find(' ', i);
            const std::string tok = kv.substr(i, j == std::string::npos ? std::string::npos : j - i);
            const size_t eq = tok.find('=');
            if (eq != std::string::npos) {
                info_[tok.substr(0, eq)] = tok.substr(eq + 1);
            }
            if (j == std::string::npos) break;
            i = j + 1;
        }
    }

    static void classify(const std::string & line, EngineLine & out) {
        if (line.size() > 2 && line[0] == 'T' && line[1] == ' ') {
            out.kind  = EngineLine::TOKEN;
            out.token = (int32_t) std::atoi(line.c_str() + 2);
        } else if (line.rfind("DONE ", 0) == 0) {
            char finish[32] = {0};
            // DONE <generated> <prompt> <prompt_ms> <decode_ms> <finish> <drafts_accepted> <drafts_offered> <reused> ...
            const int got = std::sscanf(line.c_str(), "DONE %lld %lld %lf %lf %31s %lld %lld %lld", &out.produced, &out.prompt_n,
                                        &out.prompt_ms, &out.decode_ms, finish, &out.draft_accepted, &out.draft_offered, &out.reused);
            out.kind   = EngineLine::DONE;
            out.finish = got >= 5 ? finish : "stop";
        } else if (line.rfind("PP ", 0) == 0) {
            out.kind = EngineLine::PP;
            std::sscanf(line.c_str(), "PP %lld %lld", &out.pp_done, &out.pp_total);
        } else if (line.rfind("RESUME ", 0) == 0) {
            out.kind = EngineLine::RESUME;
            out.n = std::atoll(line.c_str() + 7);
        } else if (line.rfind("REUSED ", 0) == 0) {
            out.kind = EngineLine::REUSED;
            out.n = std::atoll(line.c_str() + 7);
        } else if (line.rfind("ERR", 0) == 0) {
            out.kind    = EngineLine::ERR;
            out.message = line.size() > 4 ? line.substr(4) : "the engine reported an error";
        } else if (line.rfind("FATAL", 0) == 0) {
            out.kind    = EngineLine::ERR;
            out.message = line;
        } else if (line.rfind("INFO ", 0) == 0 || line.rfind("WARN ", 0) == 0) {
            out.kind = EngineLine::INFO;
        } else {
            out.kind = EngineLine::OTHER;
        }
    }

#ifdef _WIN32
    std::atomic<int64_t> pid_{-1};
#else
    std::atomic<pid_t> pid_{-1};
#endif
    std::string buf_;
    std::atomic<bool> alive_{false};
    bool busy_      = false;
    bool ready_     = false;
    bool can_stop_  = false;
    long long max_context_ = 0;
    std::map<std::string, std::string> info_;
};

} // namespace strata_frontend
