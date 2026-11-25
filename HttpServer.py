from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path


REPOSITORY_ROOT = Path(__file__).resolve().parent
PROJECT_ROOT = REPOSITORY_ROOT / "projects" / "project1"


class Handler(SimpleHTTPRequestHandler):
    def translate_path(self, path):
        relative_path = Path(super().translate_path(path)).relative_to(self.directory)
        root = REPOSITORY_ROOT if relative_path.parts[:1] == ("build",) else PROJECT_ROOT
        return str(root / relative_path)

    def end_headers(self):
        self.send_header("Cache-Control", "no-cache")
        super().end_headers()


ThreadingHTTPServer(("localhost", 8000), Handler).serve_forever()
