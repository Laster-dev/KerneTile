import subprocess
import os
import sys

def main():
    root_dir = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    os.chdir(root_dir)

    build_dir = os.path.join(root_dir, "build-web")
    html_src = os.path.join(build_dir, "ktui_studio_demo.html")
    js_src = os.path.join(build_dir, "ktui_studio_demo.js")
    wasm_src = os.path.join(build_dir, "ktui_studio_demo.wasm")

    if not (os.path.exists(html_src) and os.path.exists(js_src) and os.path.exists(wasm_src)):
        print("[Error] Web build artifacts not found in build-web/! Please run scripts/build_web.bat first.")
        sys.exit(1)

    nojekyll_path = os.path.join(build_dir, ".nojekyll")
    with open(nojekyll_path, "w") as f:
        f.write("")

    readme_path = os.path.join(build_dir, "README_PAGES.md")
    with open(readme_path, "w", encoding="utf-8") as f:
        f.write("# KerneTile KTUI Web Demo\n\nLive WebAssembly demo hosted on GitHub Pages.\n")

    files = {
        "index.html": html_src,
        "ktui_studio_demo.js": js_src,
        "ktui_studio_demo.wasm": wasm_src,
        ".nojekyll": nojekyll_path,
        "README.md": readme_path
    }

    print("[1/3] Hashing WebAssembly distribution artifacts into Git object store...")
    tree_entries = []
    for dest_name, src_path in files.items():
        res = subprocess.run(["git", "hash-object", "-w", src_path], capture_output=True, text=True, check=True)
        blob_hash = res.stdout.strip()
        tree_entries.append(f"100644 blob {blob_hash}\t{dest_name}")

    tree_input = "\n".join(tree_entries) + "\n"
    p = subprocess.Popen(["git", "mktree"], stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    stdout, stderr = p.communicate(tree_input.encode("utf-8"))
    if p.returncode != 0:
        print("[Error] git mktree failed:", stderr.decode())
        sys.exit(1)
    tree_hash = stdout.decode().strip()

    parent_arg = []
    try:
        res_parent = subprocess.run(["git", "rev-parse", "gh-pages"], capture_output=True, text=True, check=True)
        parent_arg = ["-p", res_parent.stdout.strip()]
    except Exception:
        pass

    print("[2/3] Creating commit for gh-pages branch...")
    commit_msg = "Deploy KerneTile KTUI WebAssembly build to GitHub Pages"
    res_commit = subprocess.run(["git", "commit-tree", tree_hash] + parent_arg + ["-m", commit_msg], capture_output=True, text=True, check=True)
    commit_hash = res_commit.stdout.strip()
    subprocess.run(["git", "update-ref", "refs/heads/gh-pages", commit_hash], check=True)

    print("[3/3] gh-pages branch updated locally.")
    print("      Commit:", commit_hash)
    print("\nTo push to GitHub, run:")
    print("      git push origin gh-pages")

if __name__ == "__main__":
    main()
