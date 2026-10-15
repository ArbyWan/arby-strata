"""#25: "preserve_thinking" keeps the thinking of earlier turns in the prompt of a template that drops it by default.

    python -m unittest serve.test_preserve_thinking -v
"""
import tempfile
import unittest
from pathlib import Path

from serve.frontend import ChatTemplate, openai_to_messages
from serve.server import ByteTokenizer, MockEngine, Service

ROOT = Path(__file__).resolve().parents[1]
# Qwen3.6's template is Qwen3.8's but for one line: it keeps an earlier turn's thinking only when told to
KEEPS = "{%- if preserve_thinking is undefined or preserve_thinking is true or loop.index0 > ns.last_query_index %}"
ASKS = "{%- if (preserve_thinking is defined and preserve_thinking is true) or (loop.index0 > ns.last_query_index) %}"
CHAT = [{"role": "user", "content": "Think of two numbers, tell me the second."},
        {"role": "assistant", "reasoning_content": "first 4821930571, second 9917263540", "content": "9917263540"},
        {"role": "user", "content": "Now the other one."}]


class PreserveThinking(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        source = (ROOT / "serve/chat_template.jinja").read_text(encoding="utf-8")
        assert KEEPS in source
        cls.dir = tempfile.TemporaryDirectory()
        path = Path(cls.dir.name) / "chat_template.jinja"
        path.write_text(source.replace(KEEPS, ASKS), encoding="utf-8")
        cls.path = path

    @classmethod
    def tearDownClass(cls):
        cls.dir.cleanup()

    def setUp(self):
        tok = ByteTokenizer()
        self.svc = Service(MockEngine(tok, "ok"), tok, ChatTemplate(self.path))

    def prompt(self, req=None):
        messages, tools, kwargs = openai_to_messages(req or {"messages": CHAT})
        return self.svc.render_prompt(messages, tools, kwargs)

    def test_off_drops_the_earlier_thinking(self):
        self.assertNotIn("4821930571", self.prompt())

    def test_on_keeps_it(self):
        self.svc.preserve_thinking = True
        self.assertIn("4821930571", self.prompt())

    def test_a_request_can_ask_for_it(self):
        self.assertIn("4821930571", self.prompt({"messages": CHAT,
                                                 "chat_template_kwargs": {"preserve_thinking": True}}))

    def test_a_request_can_turn_the_default_off(self):
        self.svc.preserve_thinking = True
        self.assertNotIn("4821930571", self.prompt({"messages": CHAT,
                                                    "chat_template_kwargs": {"preserve_thinking": False}}))

    def test_with_thinking_off_too(self):
        req = {"messages": CHAT, "chat_template_kwargs": {"enable_thinking": False, "preserve_thinking": True}}
        self.assertIn("4821930571", self.prompt(req))


if __name__ == "__main__":
    unittest.main()
