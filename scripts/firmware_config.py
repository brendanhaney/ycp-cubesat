"""Read numeric firmware defaults without requiring a C compiler."""

import ast
import operator
from pathlib import Path
import re


CONFIG_PATH = Path(__file__).resolve().parents[1] / "include" / "config.h"
_BINARY = {ast.Add: operator.add, ast.Sub: operator.sub, ast.Mult: operator.mul,
           ast.Div: operator.floordiv, ast.FloorDiv: operator.floordiv,
           ast.LShift: operator.lshift, ast.RShift: operator.rshift,
           ast.BitOr: operator.or_, ast.BitAnd: operator.and_}


def config_value(name, path=CONFIG_PATH):
    """Resolve an integer #define and arithmetic aliases in the shared header."""
    source = Path(path).read_text().replace("\\\n", "")
    source = re.sub(r"/\*.*?\*/|//[^\n]*", "", source, flags=re.S)
    definitions = dict(re.findall(r"^[ \t]*#define[ \t]+(\w+)[ \t]+([^\n]+)", source, re.M))

    def resolve(key, active):
        if key in active:
            raise ValueError(f"Circular configuration macro: {key}")
        if key not in definitions:
            raise ValueError(f"Missing configuration macro: {key}")
        expression = re.sub(r"\b(0[xX][0-9a-fA-F]+|[0-9]+)[uUlL]+\b", r"\1",
                            definitions[key])
        return evaluate(ast.parse(expression, mode="eval").body, active | {key})

    def evaluate(node, active):
        if isinstance(node, ast.Constant) and type(node.value) is int:
            return node.value
        if isinstance(node, ast.Name):
            return resolve(node.id, active)
        if isinstance(node, ast.BinOp) and type(node.op) in _BINARY:
            return _BINARY[type(node.op)](evaluate(node.left, active), evaluate(node.right, active))
        if isinstance(node, ast.UnaryOp) and isinstance(node.op, (ast.UAdd, ast.USub)):
            value = evaluate(node.operand, active)
            return -value if isinstance(node.op, ast.USub) else value
        raise ValueError(f"Unsupported numeric configuration expression for {name}")

    return resolve(name, set())
