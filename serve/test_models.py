"""The model registry (serve/models.py) and the switch between models, with engines that are not processes."""
import contextlib
import io
import json
import struct
import sys
import tempfile
import threading
import time
import unittest
import urllib.error
import urllib.request
from pathlib import Path
from unittest import mock

from serve import models as registry
from serve import server as S
from serve.frontend import ChatTemplate
from serve.server import ByteTokenizer, MockEngine, ModelBusy, Service, SwitchFailed, serve


def gguf(path: Path, **meta) -> Path:
    """A GGUF with metadata and no tensors: what the registry reads of a model file."""
    def text(s):
        b = s.encode()
        return struct.pack("<Q", len(b)) + b
    out = b"GGUF" + struct.pack("<IQQ", 3, 0, len(meta))
    for key, v in meta.items():
        key = key.replace("__", ".")
        if isinstance(v, bool):
            out += text(key) + struct.pack("<I?", 7, v)
        elif isinstance(v, int):
            out += text(key) + struct.pack("<II", 4, v)
        else:
            out += text(key) + struct.pack("<I", 8) + text(v)
    path.write_bytes(out)
    return path


def install(root: Path, name: str, arch: str, file: str, **more) -> Path:
    """A model as setup leaves it: its file, its tokenizer directory and its run config."""
    model = gguf(root / file, general__architecture=arch, general__name=name.title(),
                 general__size_label="35B-A3B", **{arch + "__context_length": 262144, **more})
    tok = root / (name + "-tok")
    tok.mkdir()
    (tok / "vocab.json").write_text("{}")
    exe = root / "engine"
    exe.write_text("")
    cfg = root / f"strata-{name}.json"
    cfg.write_text(json.dumps({"exe": str(exe), "args": ["--model", str(model), "--max-context", "8192"],
                               "cwd": str(root), "tokenizer": str(tok), "model_name": name}))
    return cfg


class Stopped(MockEngine):
    """An engine that can be stopped and started again, and counts both."""
    def __init__(self, tok, say="Hello."):
        super().__init__(tok, say, max_context=4096)
        self.running, self.stops, self.starts = True, 0, 0

    def alive(self):
        return self.running

    def unload(self):
        self.running = False
        self.stops += 1

    def restart(self):
        self.running = True
        self.starts += 1

    def close(self):
        self.running = False


class Registry(unittest.TestCase):
    def setUp(self):
        self.dir = tempfile.TemporaryDirectory()
        self.root = Path(self.dir.name)
        install(self.root, "qwen36", "qwen35moe", "Qwen3.6-35B-A3B-UD-Q3_K_M.gguf", qwen35moe__nextn_predict_layers=1)
        install(self.root, "gemma", "gemma4", "gemma-4-26B-A4B-it-UD-Q3_K_M.gguf")
        registry._seen.clear()

    def tearDown(self):
        self.dir.cleanup()

    def test_a_model_is_what_its_file_says(self):
        q = registry.find(registry.discover(self.root), "qwen36")
        self.assertEqual((q["backend"], q["mtp"], q["context"], q["status"]), ("qwen35moe", True, 8192, "available"))
        self.assertEqual((q["params_total"], q["params_active"], q["quantization"]), ("35B", "3B", "UD-Q3_K_M"))
        self.assertEqual(q["display_name"], "Qwen36")
        g = registry.find(registry.discover(self.root), "gemma")
        self.assertEqual((g["backend"], g["mtp"]), ("gemma4", False))

    def test_the_name_setup_gave_comes_before_the_files(self):
        cfg = self.root / "strata-gemma.json"
        cfg.write_text(json.dumps({**json.loads(cfg.read_text()), "display_name": "Gemma 4 26B-A4B UD-Q3_K_M"}))
        g = registry.find(registry.discover(self.root), "gemma")
        self.assertEqual(g["display_name"], "Gemma 4 26B-A4B UD-Q3_K_M")

    def test_what_cannot_run_says_why(self):
        install(self.root, "other", "llama", "other.gguf")
        cfg = install(self.root, "gone", "gemma4", "gone.gguf")
        (self.root / "gone.gguf").unlink()
        by = {e["id"]: e for e in registry.discover(self.root)}
        self.assertEqual(by["other"]["status"], "unavailable")
        self.assertIn("'llama'", by["other"]["problem"])
        self.assertIn("missing", by["gone"]["problem"])
        self.assertEqual(Path(by["gone"]["config"]), cfg)

    def test_models_json_adds_and_corrects(self):
        (self.root / "models.json").write_text(json.dumps({"gemma": {"display_name": "Gemma 4", "status": "x"}}))
        g = registry.find(registry.discover(self.root), "gemma")
        self.assertEqual((g["display_name"], g["status"]), ("Gemma 4", "available"))

    def test_find_takes_a_name_a_file_or_the_one_match(self):
        models = registry.discover(self.root)
        self.assertEqual(registry.find(models, "strata-gemma.json")["id"], "gemma")
        self.assertEqual(registry.find(models, "QWEN")["id"], "qwen36")
        self.assertIsNone(registry.find(models, "e"))       # in neither name alone
        self.assertIsNone(registry.find(models, "glm"))


class Switch(unittest.TestCase):
    def setUp(self):
        self.dir = tempfile.TemporaryDirectory()
        self.root = Path(self.dir.name)
        self.a = install(self.root, "alpha", "qwen35moe", "alpha.gguf")
        self.b = install(self.root, "beta", "gemma4", "beta.gguf")
        registry._seen.clear()
        self.tok = ByteTokenizer()
        self.old = Stopped(self.tok)
        self.svc = Service(self.old, self.tok, ChatTemplate(S.ROOT / "serve/chat_template.jinja"), model_name="alpha")
        self.svc.config_path, self.svc.models_root = str(self.a), self.root
        self.new = Stopped(self.tok, "Hi.")
        patches = [mock.patch.object(S, "load_tokenizer", lambda p: self.tok),
                   mock.patch.object(S, "start_model", lambda cfg, tok, lazy=False: (self.new, None, {"top_k": 5}, None, []))]
        for p in patches:
            p.start()
            self.addCleanup(p.stop)

    def tearDown(self):
        self.dir.cleanup()

    def test_the_old_model_stops_before_the_new_one_answers(self):
        e = self.svc.switch_model("beta")
        self.assertEqual((e["id"], e["status"], e["current"]), ("beta", "loaded", True))
        self.assertEqual((self.old.stops, self.old.running), (1, False))
        self.assertIs(self.svc.engine, self.new)
        self.assertEqual((self.svc.model, self.svc.sampling_defaults), ("beta", {"top_k": 5}))
        self.assertEqual([m["id"] for m in self.svc.models() if m["current"]], ["beta"])

    def test_a_model_that_does_not_start_leaves_the_old_one_loaded(self):
        def fails(cfg, tok, lazy=False):
            raise RuntimeError("no VRAM")
        with mock.patch.object(S, "start_model", fails):
            with self.assertRaisesRegex(SwitchFailed, "no VRAM.*alpha is loaded again"):
                self.svc.switch_model("beta")
        self.assertIs(self.svc.engine, self.old)
        self.assertEqual((self.old.stops, self.old.starts, self.old.running, self.svc.model), (1, 1, True, "alpha"))

    def test_no_switch_under_a_request_and_none_to_a_model_that_cannot_run(self):
        with self.svc.status_lock:
            self.svc.status["busy"] = True
        with self.assertRaises(ModelBusy):
            self.svc.switch_model("beta")
        self.assertEqual(self.old.stops, 0)
        with self.svc.status_lock:
            self.svc.status["busy"] = False
        (self.root / "beta.gguf").unlink()
        with self.assertRaisesRegex(ValueError, "missing"):
            self.svc.switch_model("beta")
        with self.assertRaises(KeyError):
            self.svc.switch_model("gamma")
        self.assertEqual(self.old.stops, 0)

    def configure(self, cfg_path, **keys):
        """Keys added to a model's run config, as a user writes them (None: the key taken out again)."""
        cfg = json.loads(cfg_path.read_text())
        cfg.update(keys)
        cfg_path.write_text(json.dumps({k: v for k, v in cfg.items() if v is not None}))

    def fresh_engines(self):
        """start_model() gives a new engine at every start, and the configs it was given are kept."""
        started = []

        def start(cfg, tok, lazy=False):
            started.append(cfg)
            return Stopped(tok, "Hi."), None, S.sampling_defaults_from_config(cfg), None, []
        p = mock.patch.object(S, "start_model", start)
        p.start()
        self.addCleanup(p.stop)
        return started

    def test_the_new_model_brings_its_own_settings(self):
        """#37: a switch used to keep the first model's settings except sampling, aliases and effort_position."""
        self.fresh_engines()
        self.configure(self.b, drop_oldest=True, preserve_thinking=True, reasoning_effort="low",
                       reasoning_budget_tokens=512, anthropic_thinking="on_request", tool_call_recovery=True,
                       repeat_stop_tokens=9, reasoning_loop_recovery="stop", min_free_vram_mib=1000,
                       before_load=["true"], aliases=["b2"], sampling={"temperature": 0.3})
        self.svc.switch_model("beta")
        s = self.svc
        self.assertEqual((s.drop_oldest, s.preserve_thinking, s.config_effort, s.reasoning_budget_tokens,
                          s.anthropic_think_unasked, s.tool_call_recovery, s.repeat_stop_tokens,
                          s.reasoning_loop_recovery, s.min_free_vram_mib, s.before_load, s.aliases,
                          s.sampling_defaults),
                         (True, True, "low", 512, False, True, 9, "stop", 1000, ["true"], ["b2"], {"temperature": 0.3}))
        self.svc.switch_model("alpha")                    # alpha's config sets none of them: each one's default again
        self.assertEqual((s.drop_oldest, s.preserve_thinking, s.config_effort, s.reasoning_budget_tokens,
                          s.anthropic_think_unasked, s.tool_call_recovery, s.repeat_stop_tokens,
                          s.reasoning_loop_recovery, s.min_free_vram_mib, s.before_load, s.aliases,
                          s.sampling_defaults),
                         (False, False, None, 0, True, False, S.REPEAT_STOP_TOKENS, False, 0, None, [], {}))
        self.assertEqual(s.reasoning_budget({}), None)

    def test_command_line_options_stay_for_every_model(self):
        started = self.fresh_engines()
        self.svc.overrides = {"fit_max_tokens": True, "idle_unload_s": None, "min_free_vram_mib": 77, "before_load": None}
        self.svc.cfg_overrides = {"gpu": 1}
        self.configure(self.b, min_free_vram_mib=5000, fit_max_tokens=False, before_load="true")
        self.svc.switch_model("beta")
        self.assertEqual((self.svc.min_free_vram_mib, self.svc.fit_max_tokens, self.svc.before_load), (77, True, "true"))
        self.assertEqual((started[-1]["gpu"], self.svc.gpu_indices, self.svc.gpu_index), (1, [1], 1))

    def test_a_bad_setting_is_refused_before_the_old_model_stops(self):
        for key, value, words in (("repeat_stop_tokens", -1, "repeat_stop_tokens"),
                                  ("reasoning_effort", "extreme", "reasoning_effort"),
                                  ("aliases", 5, "aliases"), ("idle_unload_s", "soon", "idle_unload_s")):
            with self.subTest(key=key):
                self.configure(self.b, **{key: value})
                with self.assertRaisesRegex(ValueError, f"beta cannot be loaded: its config: .*{words}"):
                    self.svc.switch_model("beta")
                self.assertEqual((self.old.stops, self.svc.model), (0, "alpha"))
                self.configure(self.b, **{key: None})

    def test_a_model_that_does_not_start_keeps_the_old_settings(self):
        self.svc.drop_oldest, self.svc.config_effort = True, "high"
        self.configure(self.b, drop_oldest=False, reasoning_effort="none")

        def fails(cfg, tok, lazy=False):
            raise RuntimeError("no VRAM")
        with mock.patch.object(S, "start_model", fails):
            with self.assertRaises(SwitchFailed):
                self.svc.switch_model("beta")
        self.assertEqual((self.svc.model, self.svc.drop_oldest, self.svc.config_effort), ("alpha", True, "high"))

    def test_the_shared_chat_settings_follow_the_model(self):
        self.fresh_engines()
        Path(str(self.b.with_suffix("")) + ".shared-settings.json").write_text(json.dumps({"top_k": 9}))
        self.svc.read_shared(self.a)
        self.assertEqual(self.svc.shared, {})
        self.svc.switch_model("beta")
        self.assertEqual((self.svc.shared, Path(self.svc.shared_path).name), ({"top_k": 9}, "strata-beta.shared-settings.json"))
        self.svc.set_shared({"top_k": 3})                 # POST /settings saves it for the model in use
        self.svc.switch_model("alpha")
        self.assertEqual(self.svc.shared, {})
        self.svc.switch_model("beta")
        self.assertEqual(self.svc.shared, {"top_k": 3})

    def test_what_belonged_to_the_old_model_is_left_behind(self):
        self.fresh_engines()
        saved = self.root / "image-job.session"
        saved.write_bytes(b"alpha's conversation")
        self.svc.restore_after_load, self.svc.vision_held, self.svc.warm_ids = str(saved), True, [1, 2, 3]
        self.svc.switch_model("beta")
        self.assertEqual((self.svc.restore_after_load, self.svc.vision_held, self.svc.warm_ids), (None, False, None))
        self.assertFalse(saved.exists())

    def test_idle_unload_starts_with_the_first_model_that_has_it(self):
        self.fresh_engines()
        self.svc.start_idle_unload()                     # alpha has none: no thread
        self.assertIsNone(self.svc.idle_thread)
        self.configure(self.b, idle_unload_s=600)
        self.svc.switch_model("beta")
        thread = self.svc.idle_thread
        self.assertTrue(thread is not None and thread.is_alive())
        self.assertEqual(self.svc.idle_unload_s, 600)
        self.svc.switch_model("alpha")                    # alpha stays loaded: 0 again, the one thread waits
        self.assertEqual((self.svc.idle_unload_s, self.svc.idle_thread), (0, thread))

    def test_shutdown_ends_the_engine_that_runs_now(self):
        """#37: Ctrl+C after a switch closed the engine the server started with, which had already stopped."""
        self.fresh_engines()
        self.svc.switch_model("beta")
        now, httpd = self.svc.engine, mock.Mock()
        self.assertIsNot(now, self.old)
        S.shutdown(self.svc, httpd)
        httpd.shutdown.assert_called_once_with()
        self.assertFalse(now.running)

    def test_the_routes(self):
        httpd = serve(self.svc, port=0)
        self.addCleanup(httpd.shutdown)
        base = f"http://127.0.0.1:{httpd.server_address[1]}"

        def call(path, body=None):
            req = urllib.request.Request(base + path, None if body is None else json.dumps(body).encode(),
                                         {"Content-Type": "application/json"})
            try:
                with urllib.request.urlopen(req, timeout=30) as r:
                    return r.status, json.load(r)
            except urllib.error.HTTPError as e:
                return e.code, json.load(e)

        code, listing = call("/api/models")
        self.assertEqual((code, listing["current"], [m["id"] for m in listing["models"]]), (200, "alpha", ["alpha", "beta"]))
        self.assertEqual(call("/api/models/load", {"model": "gamma"})[0], 404)
        code, out = call("/api/models/load", {"model": "beta"})
        self.assertEqual((code, out["model"]["id"]), (200, "beta"))
        code, cur = call("/api/models/current")
        self.assertEqual((cur["model"]["id"], cur["loaded"], cur["name"]), ("beta", True, "beta"))
        self.assertEqual(call("/api/models/unload", {}), (200, {"status": "unloaded"}))
        self.assertEqual(call("/api/models/current")[1]["loaded"], False)


class Start(unittest.TestCase):
    """main() with --engine strata as far as "ready": 0.1.42's line for the replicas' cards read `groups`, a name of
    start_model()'s, and every start of a real engine ended in a NameError."""

    def start(self, keys=None, argv=()):
        """main() for a run config with `keys`, stopped at once (Ctrl+C): (its output, the Service it served)."""
        real_sleep = time.sleep

        def sleep(s):                       # Ctrl+C in main()'s wait, real sleeps in the server's threads
            if threading.current_thread() is threading.main_thread():
                raise KeyboardInterrupt
            real_sleep(s)
        served = []
        real_serve = S.serve

        def serve_it(svc, host="127.0.0.1", port=0):
            served.append(svc)
            return real_serve(svc, host=host, port=port)
        with tempfile.TemporaryDirectory() as d:
            cfg = install(Path(d), "alpha", "qwen35moe", "a.gguf")
            cfg.write_text(json.dumps({**json.loads(cfg.read_text()), **(keys or {})}))
            tok = ByteTokenizer()
            out = io.StringIO()
            with mock.patch.object(sys, "argv", ["server.py", "--engine", "strata", "--config", str(cfg), "--port", "0",
                                                 *argv]), \
                    mock.patch.object(S, "load_tokenizer", lambda p: tok), \
                    mock.patch.object(S, "start_model", lambda cfg, tok, lazy=False:
                                      (Stopped(tok), None, S.sampling_defaults_from_config(cfg), None, None)), \
                    mock.patch.object(S, "serve", serve_it), \
                    mock.patch.object(S.time, "sleep", sleep), mock.patch.object(S.signal, "signal"), \
                    contextlib.redirect_stdout(out):
                self.assertEqual(S.main(), 0)
        return out.getvalue(), served[0]

    def test_main_reaches_ready(self):
        out, _ = self.start()
        self.assertIn("ready: http://127.0.0.1:0/v1", out)

    def test_the_config_settings_are_as_before(self):
        """#37 moved the model's settings into model_settings(): main() sets what it set before (0.1.44.1-poly.6)."""
        keys = {"drop_oldest": True, "preserve_thinking": True, "reasoning_close_retry": True, "fit_max_tokens": True,
                "reasoning_effort": "low", "codex_thread_titles": True, "codex_compaction_cache": True,
                "aliases": ["a1", "a2"], "anthropic_thinking": "on_request", "tool_call_recovery": True,
                "warm_start": True, "repeat_stop_tokens": 7, "reasoning_budget_tokens": 512,
                "reasoning_loop_recovery": True, "idle_unload_s": 60, "min_free_vram_mib": 1000, "before_load": "true",
                "before_load_timeout_s": 5, "before_load_ignore_failure": True, "before_load_strict": True,
                "memory_debug": True, "gpu": 0, "backend": "cuda", "sampling": {"temperature": 0.7}}
        out, svc = self.start(keys)
        got = {k: getattr(svc, k) for k in (
            "aliases", "anthropic_think_unasked", "backend", "before_load", "before_load_ignore_failure",
            "before_load_strict", "before_load_timeout_s", "codex_compaction_cache", "codex_thread_titles",
            "config_effort", "drop_oldest", "fit_max_tokens", "gpu_index", "gpu_indices", "idle_unload_s",
            "memory_debug", "min_free_vram_mib", "preserve_thinking", "reasoning_budget_tokens", "reasoning_close_retry",
            "reasoning_loop_recovery", "repeat_stop_tokens", "sampling_defaults", "tool_call_recovery", "warm_start")}
        self.assertEqual(got, {
            "aliases": ["a1", "a2"], "anthropic_think_unasked": False, "backend": "cuda", "before_load": "true",
            "before_load_ignore_failure": True, "before_load_strict": True, "before_load_timeout_s": 5.0,
            "codex_compaction_cache": True, "codex_thread_titles": True, "config_effort": "low", "drop_oldest": True,
            "fit_max_tokens": True, "gpu_index": 0, "gpu_indices": [0], "idle_unload_s": 60.0, "memory_debug": True,
            "min_free_vram_mib": 1000, "preserve_thinking": True, "reasoning_budget_tokens": 512,
            "reasoning_close_retry": True, "reasoning_loop_recovery": "recover", "repeat_stop_tokens": 7,
            "sampling_defaults": {"temperature": 0.7}, "tool_call_recovery": True, "warm_start": True})
        for line in ("[strata] thinking level: low", "[strata] model aliases: a1, a2",
                     "[strata] thinking budget: 512 tokens", "[strata] the model unloads after 60.0 s"):
            self.assertIn(line, out)

    def test_the_command_line_wins_and_is_kept_for_a_switch(self):
        out, svc = self.start({"idle_unload_s": 60, "min_free_vram_mib": 5},
                              ["--idle-unload", "5", "--fit-max-tokens", "--min-free-vram-mib", "77",
                               "--before-load", "echo hi", "--gpu", "1"])
        self.assertEqual((svc.idle_unload_s, svc.fit_max_tokens, svc.min_free_vram_mib, svc.before_load),
                         (5.0, True, 77, "echo hi"))
        self.assertEqual(svc.overrides, {"fit_max_tokens": True, "idle_unload_s": 5.0, "min_free_vram_mib": 77,
                                         "before_load": "echo hi"})
        self.assertEqual(svc.cfg_overrides, {"gpu": 1})

    def test_a_bad_value_still_stops_the_start(self):
        for keys, words in (({"repeat_stop_tokens": -1}, '"repeat_stop_tokens" must be a whole number'),
                            ({"idle_unload_s": "abc"}, '"idle_unload_s" must be a number of seconds')):
            with self.subTest(keys=keys), self.assertRaisesRegex(SystemExit, r"^\[strata\] config " + words):
                self.start(keys)


if __name__ == "__main__":
    unittest.main()
