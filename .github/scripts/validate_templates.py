"""
CI helper: validates every Jinja template in flask-server/templates parses
without a syntax error. Run from the flask-server/ directory:

    python ../.github/scripts/validate_templates.py
"""

import os
import sys
from jinja2 import Environment, FileSystemLoader, TemplateSyntaxError

TEMPLATES_DIR = "templates"


def main():
    env = Environment(loader=FileSystemLoader(TEMPLATES_DIR))
    errors = []

    for fname in os.listdir(TEMPLATES_DIR):
        if fname.endswith(".html"):
            try:
                env.get_template(fname)
            except TemplateSyntaxError as e:
                errors.append(f"{fname}: {e}")

    if errors:
        print("Template syntax errors found:")
        for e in errors:
            print(" -", e)
        sys.exit(1)

    print("All templates in templates/ parsed successfully.")


if __name__ == "__main__":
    main()
