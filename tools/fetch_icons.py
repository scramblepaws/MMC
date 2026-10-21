"""Fetches the provider icons and turns them into the png files MMC ships.

The icons are the trademarks of Google, Microsoft and Yandex. They are used
to say which service an account belongs to, nothing else.

Run it from the repository root:

    python tools/fetch_icons.py

It needs the internet and Pillow. The png files it writes are in the
repository, so a normal build does not need the network.
"""

import io
import os
import urllib.parse
import urllib.request

from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, "resources", "icons")
SIZE = 64

# file name on Wikimedia Commons -> the name we give it here
SOURCES = {
    "gmail": "Gmail icon (2020).svg",
    "outlook": "Outlook.com icon (2012-2019).svg",
    "yandex": "Yandex Mail icon.svg",
}

USER_AGENT = "MMC/0.1 (https://github.com/scramblepaws/MMC)"


def thumb_url(file_name: str, width: int = 250) -> str:
    query = urllib.parse.urlencode(
        {
            "action": "query",
            "titles": "File:" + file_name,
            "prop": "imageinfo",
            "iiprop": "url",
            "iiurlwidth": width,
            "format": "json",
        }
    )
    api = "https://commons.wikimedia.org/w/api.php?" + query
    request = urllib.request.Request(api, headers={"User-Agent": USER_AGENT})
    with urllib.request.urlopen(request, timeout=60) as answer:
        import json

        pages = json.load(answer)["query"]["pages"]
    page = list(pages.values())[0]
    if "imageinfo" not in page:
        raise SystemExit("no such file on Wikimedia Commons: " + file_name)
    return page["imageinfo"][0]["thumburl"]


def fetch(url: str) -> bytes:
    request = urllib.request.Request(url, headers={"User-Agent": USER_AGENT})
    with urllib.request.urlopen(request, timeout=60) as answer:
        return answer.read()


def main() -> None:
    os.makedirs(OUT, exist_ok=True)
    for name, source in SOURCES.items():
        image = Image.open(io.BytesIO(fetch(thumb_url(source)))).convert("RGBA")

        # Cut away the empty space around the logo.
        box = image.getbbox()
        if box:
            image = image.crop(box)

        # Scale it so it fits, then centre it on a square of exactly this size.
        image.thumbnail((SIZE, SIZE), Image.LANCZOS)
        canvas = Image.new("RGBA", (SIZE, SIZE), (0, 0, 0, 0))
        canvas.paste(image, ((SIZE - image.width) // 2, (SIZE - image.height) // 2), image)

        path = os.path.join(OUT, name + ".png")
        canvas.save(path, "PNG", optimize=True)
        print("wrote", path, canvas.size)


if __name__ == "__main__":
    main()
