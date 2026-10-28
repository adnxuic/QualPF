"""Render the manual's TeX as static MathML after Markdown parses math/code."""
from xml.etree import ElementTree

from latex2mathml.converter import convert
from markdown.extensions import Extension
from markdown.treeprocessors import Treeprocessor


class MathMLProcessor(Treeprocessor):
    def run(self, root):
        for parent in root.iter():
            for index, child in enumerate(list(parent)):
                kind = child.get("type", "")
                if child.tag != "script" or not kind.startswith("math/tex"):
                    continue
                expression = child.text or ""
                display = "block" if "mode=display" in kind else "inline"
                try:
                    math = ElementTree.fromstring(convert(expression, display=display))
                    required_children = {"mfrac": 2, "mroot": 2, "msub": 2, "msup": 2,
                                         "msubsup": 3, "munder": 2, "mover": 2, "munderover": 3}
                    for element in math.iter():
                        tag = element.tag.rsplit("}", 1)[-1]
                        if tag in required_children and len(element) != required_children[tag]:
                            raise ValueError(f"Incomplete MathML {tag}")
                except Exception as error:
                    raise ValueError(f"Cannot render manual formula: {expression}") from error
                # HTML recognizes MathML's default namespace. Avoid serializer
                # prefixes such as ns0:math, which HTML would treat as custom tags.
                for element in math.iter():
                    element.tag = element.tag.rsplit("}", 1)[-1]
                    if element.tag == "mtext" and element.text:
                        # The converter retains this TeX escape in text groups.
                        element.text = element.text.replace(r"\%", "%")
                math.set("xmlns", "http://www.w3.org/1998/Math/MathML")
                math.tail = child.tail
                parent.remove(child)
                parent.insert(index, math)
        return root


class MathMLExtension(Extension):
    def extendMarkdown(self, md):
        md.treeprocessors.register(MathMLProcessor(md), "qualpf-mathml", 15)


def on_config(config):
    extensions = config["markdown_extensions"]
    if not any(isinstance(extension, MathMLExtension) for extension in extensions):
        extensions.append(MathMLExtension())
    return config
