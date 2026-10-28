"""Run with the build dependencies in docs/requirements.txt installed."""
from pathlib import Path
import sys
import unittest
from xml.etree import ElementTree

import markdown

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / "tools/documentation"))
from render_mathml import MathMLExtension, on_config

NS = {"m": "http://www.w3.org/1998/Math/MathML"}


class DocumentationMathTests(unittest.TestCase):
    def render(self, text):
        config = {"markdown_extensions": ["tables", "fenced_code", "pymdownx.arithmatex"]}
        on_config(config)
        return markdown.markdown(text, extensions=config["markdown_extensions"],
            extension_configs={"pymdownx.arithmatex": {"preview": False}})

    def test_inline_symbols_subscripts_and_table_cells(self):
        html = self.render("| Coordinate | Radiation |\n| --- | --- |\n| $2\\theta$ | $K\\alpha_2$ |")
        root = ElementTree.fromstring("<div>" + html + "</div>")
        self.assertEqual(len(root.findall(".//m:math", NS)), 2)
        self.assertIn("θ", "".join(root.itertext()))
        self.assertIsNotNone(root.find(".//m:msub", NS))
        self.assertNotIn("<script", html)
        self.assertNotIn("$", html)

    def test_display_fraction_sum_and_following_text(self):
        html = self.render(r"$$W_k = \frac{A_k}{\sum_j A_j}\times 100\%$$" + "\n\nAfter.")
        root = ElementTree.fromstring("<div>" + html + "</div>")
        math = root.find(".//m:math", NS)
        self.assertIsNotNone(math)
        self.assertEqual(math.get("display"), "block")
        self.assertIsNotNone(math.find(".//m:mfrac", NS))
        self.assertIn("∑", "".join(math.itertext()))
        self.assertIn("After.", "".join(root.itertext()))

    def test_code_and_escaped_dollars_remain_literal(self):
        html = self.render("`$x_1$`\n\n```text\n$y_2$\n```\n\nCost: \\$5.")
        self.assertNotIn("<math", html)
        self.assertIn("$x_1$", html)
        self.assertIn("$y_2$", html)
        self.assertIn("$5", html)

    def test_percent_in_a_text_group_is_literal(self):
        html = self.render(r"$\text{Wt.\%}$")
        root = ElementTree.fromstring("<div>" + html + "</div>")
        self.assertEqual(root.find(".//m:mtext", NS).text, "Wt.%")

    def test_invalid_formula_stops_publication(self):
        for expression in (r"$\frac{x}$", r"$\left(x$"):
            with self.subTest(expression=expression):
                with self.assertRaisesRegex(ValueError, "Cannot render manual formula"):
                    self.render(expression)

    def test_repeated_configuration_keeps_one_renderer(self):
        config = {"markdown_extensions": []}
        on_config(config)
        on_config(config)
        self.assertEqual(sum(isinstance(extension, MathMLExtension)
                             for extension in config["markdown_extensions"]), 1)


if __name__ == "__main__":
    unittest.main()
