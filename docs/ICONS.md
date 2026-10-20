# The provider icons

The pictures in `resources/icons` are the logos of Google, Microsoft and
Yandex. They are there to say which service an account belongs to, the same
way every mail client does it. They are trademarks of their owners and stay
the property of their owners.

| File | Logo | Comes from |
|---|---|---|
| `gmail.png` | Gmail | [Gmail icon (2020)](https://commons.wikimedia.org/wiki/File:Gmail_icon_(2020).svg) on Wikimedia Commons |
| `outlook.png` | Outlook | [Outlook.com icon (2012-2019)](https://commons.wikimedia.org/wiki/File:Outlook.com_icon_(2012-2019).svg) on Wikimedia Commons |
| `yandex.png` | Yandex Mail | [Yandex Mail icon](https://commons.wikimedia.org/wiki/File:Yandex_Mail_icon.svg) on Wikimedia Commons |

They are fetched and turned into the 64 by 64 png files MMC ships with

```
python tools/fetch_icons.py
```

The script needs Pillow and the internet. The png files are in the
repository, so building MMC does not need the network.
