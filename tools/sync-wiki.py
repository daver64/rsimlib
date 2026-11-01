#!/usr/bin/env python3
"""Regenerates the simlib GitHub wiki from README.md and docs/ and optionally pushes it.

    tools/sync-wiki.py [--out DIR] [--push]

Pushing uses SSH (git@github.com). Without --out the wiki repo is cloned into a temp dir. Without --push nothing is committed or pushed, so a plain run
is a dry run (use --out to keep the result for inspection). The docs stay the source of truth: every wiki page is
overwritten, so do not hand-edit the wiki. Wiki page names are flat: Home (README), Tutorial, Tutorial-<lesson>,
Examples, Example-<name> and the top-level guides keep their file names. Links to other docs become wiki links;
images and source files point at the repository on GitHub.
"""
import argparse
import os
import re
import shutil
import subprocess
import sys
import tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
REPO = "daver64/rsimlib"
BRANCH = "main"
WIKI_URL = f"git@github.com:{REPO}.wiki.git"
RAW = f"https://raw.githubusercontent.com/{REPO}/{BRANCH}/"
BLOB = f"https://github.com/{REPO}/blob/{BRANCH}/"
TREE = f"https://github.com/{REPO}/tree/{BRANCH}/"
IMAGE_EXT = (".png", ".jpg", ".jpeg", ".gif", ".svg", ".webp")


def page_name(rel):
    """Wiki page name for a repo-relative markdown path, or None if it is not a published page."""
    rel = rel.replace(os.sep, "/")
    if rel == "README.md":
        return "Home"
    if not rel.startswith("docs/") or not rel.endswith(".md"):
        return None
    sub = rel[len("docs/"):-3]
    if sub == "tutorial/README":
        return "Tutorial"
    if sub == "examples/README":
        return "Examples"
    if sub.startswith("tutorial/"):
        return "Tutorial-" + sub[len("tutorial/"):]
    if sub.startswith("examples/"):
        return "Example-" + sub[len("examples/"):]
    if "/" in sub:
        return None
    return sub


def collect_pages():
    pages = {}
    for dirpath, _, files in os.walk(os.path.join(ROOT, "docs")):
        for f in files:
            if f.endswith(".md"):
                rel = os.path.relpath(os.path.join(dirpath, f), ROOT)
                name = page_name(rel)
                if name:
                    pages[rel.replace(os.sep, "/")] = name
    pages["README.md"] = "Home"
    return pages


def rewrite_target(target, src_rel, pages):
    if re.match(r"^([a-z][a-z0-9+.-]*:|#|//)", target, re.I):
        return target
    path, hash_, anchor = target.partition("#")
    if not path:
        return target
    base = os.path.dirname(src_rel)
    resolved = os.path.normpath(os.path.join(base, path)).replace(os.sep, "/")
    if resolved.startswith(".."):
        return target
    if resolved in pages:
        return pages[resolved] + (hash_ + anchor if hash_ else "")
    full = os.path.join(ROOT, resolved)
    if resolved.lower().endswith(IMAGE_EXT):
        return RAW + resolved
    if os.path.isdir(full):
        return TREE + resolved.rstrip("/")
    return BLOB + resolved + (hash_ + anchor if hash_ else "")


LINK = re.compile(r"(\]\()([^)\s]+)((?:\s+\"[^\"]*\")?\))")
HTML = re.compile(r"((?:src|href)=\")([^\"]+)(\")")


def convert(text, src_rel, pages):
    text = LINK.sub(lambda m: m.group(1) + rewrite_target(m.group(2), src_rel, pages) + m.group(3), text)
    return HTML.sub(lambda m: m.group(1) + rewrite_target(m.group(2), src_rel, pages) + m.group(3), text)


def title_of(rel):
    with open(os.path.join(ROOT, rel), encoding="utf-8") as f:
        for line in f:
            if line.startswith("# "):
                return re.sub(r"\[([^\]]*)\]\([^)]*\)", r"\1", line[2:].strip()).replace("[", "(").replace("]", ")")
    return page_name(rel).replace("_", " ").replace("-", " ")


def sidebar(pages):
    guides = sorted((r for r in pages if r.startswith("docs/") and r.count("/") == 1), key=lambda r: title_of(r).lower())
    lines = ["**[simlib](Home)**", "", "- [Home](Home)", "- [Tutorial](Tutorial)", "- [Example gallery](Examples)", "",
             "**Guides**", ""]
    lines += [f"- [{title_of(r)}]({pages[r]})" for r in guides]
    lines += ["", f"[Source on GitHub](https://github.com/{REPO})", ""]
    return "\n".join(lines)


def run(*cmd, cwd=None):
    subprocess.run(cmd, cwd=cwd, check=True)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out")
    ap.add_argument("--push", action="store_true")
    args = ap.parse_args()

    out = args.out or tempfile.mkdtemp(prefix="simlib-wiki-")
    if args.push or not args.out:
        if os.path.isdir(os.path.join(out, ".git")):
            run("git", "pull", "--ff-only", cwd=out)
        else:
            run("git", "clone", WIKI_URL, out)
    os.makedirs(out, exist_ok=True)

    for f in os.listdir(out):
        if f.endswith(".md"):
            os.remove(os.path.join(out, f))

    pages = collect_pages()
    for rel, name in sorted(pages.items()):
        with open(os.path.join(ROOT, rel), encoding="utf-8") as f:
            text = f.read()
        with open(os.path.join(out, name + ".md"), "w", encoding="utf-8") as f:
            f.write(convert(text, rel, pages))
    with open(os.path.join(out, "_Sidebar.md"), "w", encoding="utf-8") as f:
        f.write(sidebar(pages))
    with open(os.path.join(out, "_Footer.md"), "w", encoding="utf-8") as f:
        f.write("Generated from the repository docs by `tools/sync-wiki.py`; edit the docs, not this wiki.\n")

    print(f"{len(pages)} pages written to {out}")
    if not args.push:
        return 0
    run("git", "add", "-A", cwd=out)
    if subprocess.run(["git", "diff", "--cached", "--quiet"], cwd=out).returncode == 0:
        print("wiki already up to date")
        return 0
    run("git", "commit", "-q", "-m", "Regenerate wiki from docs\n\nCo-authored-by: Copilot <223556219+Copilot@users.noreply.github.com>", cwd=out)
    run("git", "push", cwd=out)
    return 0


if __name__ == "__main__":
    sys.exit(main())
