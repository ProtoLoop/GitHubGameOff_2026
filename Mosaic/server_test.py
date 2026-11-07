import http.client
from functools import partial
from http.server import ThreadingHTTPServer
from pathlib import Path
import tempfile
import threading
import unittest

from server import Handler


class ServerTest(unittest.TestCase):
    def test_directory_get_and_restricted_put(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            projects = root / "projects"
            (projects / "one").mkdir(parents=True)
            (projects / "two").mkdir()
            (root / "index.html").write_text("Mosaic")
            server = ThreadingHTTPServer(("localhost", 0), partial(
                Handler, directory=str(root), projects_root=projects))
            thread = threading.Thread(target=server.serve_forever)
            thread.start()
            connection = http.client.HTTPConnection("localhost", server.server_port)
            try:
                def request(method, path, body=None):
                    connection.request(method, path, body)
                    response = connection.getresponse()
                    return response.status, response.read()
                self.assertEqual(request("GET", "/"), (200, b"Mosaic"))
                self.assertIn(b'one/', request("GET", "/projects/")[1])
                self.assertEqual(request("GET", "/projects/one/level.json")[0], 404)
                self.assertEqual(request("PUT", "/projects/one/level.json", b'{"one":1}')[0], 204)
                self.assertEqual(request("PUT", "/projects/two/level.json", b'{"two":2}')[0], 204)
                self.assertEqual(request("GET", "/projects/one/level.json"), (200, b'{"one":1}'))
                for path in ["/index.html", "/projects/one/a.png", "/projects/../level.json",
                             "/projects/%2e%2e/level.json", "/projects/missing/level.json"]:
                    self.assertEqual(request("PUT", path, b"bad")[0], 403)
                (projects / "escape").symlink_to(root, target_is_directory=True)
                self.assertEqual(request("PUT", "/projects/escape/level.json", b"bad")[0], 403)
                self.assertFalse((root / "level.json").exists())
                self.assertEqual((projects / "two/level.json").read_text(), '{"two":2}')
            finally:
                connection.close()
                server.shutdown()
                server.server_close()
                thread.join()


if __name__ == "__main__":
    unittest.main()
