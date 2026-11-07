"""Serve Mosaic and ordinary project directories, with PUT for level.json."""
import argparse
from functools import partial
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
import os
import tempfile
from urllib.parse import unquote, urlsplit


class Handler(SimpleHTTPRequestHandler):
    def __init__(self, *args, projects_root, **kwargs):
        self.projects_root = Path(projects_root).resolve()
        super().__init__(*args, **kwargs)

    def translate_path(self, path):
        name = unquote(urlsplit(path).path)
        if name == "/projects" or name.startswith("/projects/"):
            target = (self.projects_root / name.removeprefix("/projects").lstrip("/")).resolve()
            if target.is_relative_to(self.projects_root):
                return str(target)
            return str(self.projects_root / ".invalid-path")
        return super().translate_path(path)

    def end_headers(self):
        self.send_header("Cache-Control", "no-store")
        super().end_headers()

    def do_PUT(self):
        parts = unquote(urlsplit(self.path).path).split("/")
        target = (self.projects_root / "/".join(parts[2:])).resolve()
        if (len(parts) != 4 or parts[1] != "projects" or parts[2] in ("", ".", "..")
                or parts[3] != "level.json" or not target.is_relative_to(self.projects_root)
                or not target.parent.is_dir()):
            self.send_error(403, "Only project level.json files may be written")
            return
        try:
            length = int(self.headers.get("Content-Length", "-1"))
        except ValueError:
            length = -1
        if length < 0 or length > 16 * 1024 * 1024:
            self.send_error(400, "Expected Content-Length up to 16 MiB")
            return
        temporary = None
        try:
            with tempfile.NamedTemporaryFile(dir=target.parent, delete=False) as output:
                temporary = output.name
                remaining = length
                while remaining:
                    chunk = self.rfile.read(min(remaining, 65536))
                    if not chunk:
                        raise OSError("Incomplete request body")
                    output.write(chunk)
                    remaining -= len(chunk)
            os.replace(temporary, target)
            self.send_response(204)
            self.end_headers()
        except OSError as error:
            self.send_error(500, str(error))
        finally:
            if temporary and os.path.exists(temporary):
                os.unlink(temporary)


def main():
    root = Path(__file__).resolve().parent
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--projects-root", type=Path, default=root / "projects")
    parser.add_argument("--port", type=int, default=8001)
    args = parser.parse_args()
    handler = partial(Handler, directory=str(root / "build/dist"), projects_root=args.projects_root)
    print(f"Mosaic: http://localhost:{args.port}/", flush=True)
    ThreadingHTTPServer(("localhost", args.port), handler).serve_forever()


if __name__ == "__main__":
    main()
