"""The one painter (picture_paint) against the pictures the code it replaced made (camera_feed's encoders, tile_art).

Each case paints the same raw picture both ways and compares the pixels a screen would draw. Where the painter does
exactly what the old code did, the pictures are the same within a rounding step; where it changed on purpose (every
rounded corner is drawn smooth now, a cover's too), the difference stays at the corners and is small everywhere else.
"""
import io
from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'screen_manager/app'))

from PIL import Image, ImageChops, ImageDraw, ImageStat  # noqa: E402

sys.path.insert(0, str(Path(__file__).resolve().parent / "fixtures"))
import paint_before as camera_feed  # noqa: E402  (the painters of before, frozen)
import picture_paint as paint  # noqa: E402
import paint_before as tile_art  # noqa: E402
from picture_question import Owner, Place, Question  # noqa: E402


def jpeg(size, turned=False):
    """A camera-like JPEG: colour ramps and a few shapes, so a crop or a scale that differs shows."""
    image = Image.new('RGB', size)
    draw = ImageDraw.Draw(image)
    for x in range(size[0]):
        draw.line((x, 0, x, size[1]), fill=(x * 255 // size[0], 90, 255 - x * 255 // size[0]))
    draw.ellipse((size[0] // 4, size[1] // 4, size[0] // 2, size[1] // 2), fill=(250, 240, 20))
    draw.rectangle((size[0] * 2 // 3, size[1] // 3, size[0] - 10, size[1] - 10), fill=(10, 200, 60))
    out = io.BytesIO()
    if turned:
        exif = Image.Exif()
        exif[0x0112] = 6
        image.save(out, 'JPEG', quality=92, exif=exif)
    else:
        image.save(out, 'JPEG', quality=92)
    return out.getvalue()


def png_alpha(size):
    """A logo with transparent corners."""
    image = Image.new('RGBA', size, (0, 0, 0, 0))
    ImageDraw.Draw(image).ellipse((8, 8, size[0] - 8, size[1] - 8), fill=(220, 40, 40, 255))
    out = io.BytesIO()
    image.save(out, 'PNG')
    return out.getvalue()


def pixels(data):
    with Image.open(io.BytesIO(data)) as image:
        return image.convert('RGB').copy()


def difference(a, b):
    """(mean, worst) difference per channel between two pictures of the same size."""
    diff = ImageChops.difference(a, b)
    return sum(ImageStat.Stat(diff).mean) / 3, max(high for _, high in diff.getextrema())


def question(box, *places, fmt='bmp24'):
    return Question(Owner('view'), tuple(None for _ in places), box, tuple(places), formats=(fmt,))


class SameAsBefore(unittest.TestCase):
    def close(self, old, new, mean=0.6, size=None):
        a, b = pixels(old), pixels(new)
        self.assertEqual(a.size, b.size if size is None else size)
        self.assertEqual(a.size, b.size)
        got, _worst = difference(a, b)
        self.assertLess(got, mean, f'mean difference {got:.2f}')

    def test_a_full_view_still_at_its_own_proportions(self):
        raw = jpeg((1280, 720))
        for box in ((480, 480), (800, 480), (1024, 600)):
            old = camera_feed.encode(raw, box)
            new = paint.paint(question(box, Place(0, 0, *box, fit='shrink')), [raw])
            self.assertEqual(old, new, box)

    def test_an_alert_frame_made_for_the_picture(self):
        raw = jpeg((1280, 720))
        old = camera_feed.encode(raw, (392, 220), exact=True)
        new = paint.paint(question((392, 220), Place(0, 0, 392, 220, fit='exact')), [raw])
        self.close(old, new, mean=1.0)

    def test_a_turned_camera_is_measured_and_shown_upright(self):
        raw = jpeg((640, 360), turned=True)
        self.assertEqual(paint.size_of(raw), (360, 640))
        old = camera_feed.encode(raw, (480, 480))
        new = paint.paint(question((480, 480), Place(0, 0, 480, 480, fit='shrink')), [raw])
        self.assertEqual(old, new)

    def test_a_cover_on_its_card(self):
        raw = jpeg((600, 600))
        for size, ground in ((96, 0x1C1C1E), (320, 0xEEEEEE)):
            old = camera_feed.encode_cover(raw, size, ground)
            radius = max(4, size // camera_feed.COVER_RADIUS_SHARE)
            new = paint.paint(question((size, size), Place(0, 0, size, size, radius, fit='cover', ground=ground)), [raw])
            # The corners are smooth now (drawn four times larger); everything else is the same picture.
            self.close(old, new, mean=1.5)

    def test_a_page_of_tiles_with_their_own_grounds_fits_and_shades(self):
        raws = [jpeg((640, 360)), jpeg((400, 400)), png_alpha((200, 200)), None]
        atlas = (380, 200, ((0, 0, 180, 100, 12, 0), (190, 0, 180, 100, 12, 170), (0, 110, 100, 90, 8, 0),
                            (110, 110, 100, 80, 8, 0)))
        grounds = [0x202020, 0x3A3A3C, 0xF2F2F7, 0x123456]
        modes = [('fill', True), ('contain', False), ('fill', False), ('fill', False)]
        old = tile_art.encode_atlas(raws, grounds, atlas, modes, compact=False)
        places = tuple(Place(x, y, w, h, r, s, 'contain' if mode == 'contain' else 'cover', g, fade=fade)
                       for (x, y, w, h, r, s), g, (mode, fade) in zip(atlas[2], grounds, modes))
        q = Question(Owner('tile', 0), (None,) * 4, atlas[:2], places, formats=('bmp24',))
        new = paint.paint(q, raws)
        # Between the tiles of an older screen's page nothing is drawn: each tile shows its own piece. So each piece alone.
        a, b = pixels(old), pixels(new)
        self.assertEqual(a.size, b.size)
        for x, y, w, h, _r, _s in atlas[2]:
            got, _ = difference(a.crop((x, y, x + w, y + h)), b.crop((x, y, x + w, y + h)))
            self.assertLess(got, 1.0, (x, y, got))

    def test_a_plugin_cover_flow_with_turns_and_reflections(self):
        raws = [jpeg((342, 513)) for _ in range(3)]
        atlas = (300, 160, ((0, 10, 90, 140, 6, 0), (105, 0, 90, 156, 6, 0), (210, 10, 90, 140, 6, 0)))
        styles = [('fill', 'right', True), ('fill', '', True), ('fill', 'left', True)]
        old = tile_art.layers(raws, 0x101010, atlas, styles, compact=False)
        places = tuple(Place(x, y, w, h, r, 0, 'cover', 0x101010, turn=turn, mirror=mirror)
                       for (x, y, w, h, r, _), (_, turn, mirror) in zip(atlas[2], styles))
        new = paint.paint(Question(Owner('plugin_tile', 0), (None,) * 3, atlas[:2], places), raws)
        self.close(old, new, mean=0.6)

    def test_a_plugin_place_shown_whole_lets_the_place_under_it_show(self):
        raws = [jpeg((400, 400)), png_alpha((120, 60))]
        atlas = (200, 200, ((0, 0, 200, 200, 0, 0), (40, 40, 120, 120, 0, 0)))
        old = tile_art.layers(raws, 0x222222, atlas, [('fill', '', False), ('within', '', False)], compact=False)
        places = (Place(0, 0, 200, 200, ground=0x222222), Place(40, 40, 120, 120, fit='within', ground=0x222222))
        new = paint.paint(Question(Owner('plugin_tile', 0), (None, None), (200, 200), places), raws)
        self.close(old, new, mean=0.6)

    def test_the_screensaver_cover_and_camera(self):
        cover, camera = jpeg((500, 500)), jpeg((1280, 720))
        for box in ((480, 480), (800, 480), (480, 800)):
            old = camera_feed.encode_saver(cover, box, 'media', 0x334455)
            new = paint.paint(question(box, Place(0, 0, *box, fit='saver', ground=0x334455, dim=30), fmt='bmp8'), [cover])
            self.close(old, new, mean=1.5)
            old = camera_feed.encode_saver(camera, box, 'camera')
            new = paint.paint(question(box, Place(0, 0, *box, fit='cover', dim=30), fmt='bmp8'), [camera])
            self.close(old, new, mean=1.5)

    def test_the_photos_of_the_screensaver(self):
        wide, tall = jpeg((1600, 900)), jpeg((900, 1600))
        box = (800, 480)
        for raw, fit, mode in ((wide, 'auto', 'auto'), (tall, 'auto', 'auto'), (wide, 'fill', 'cover'),
                               (tall, 'whole', 'whole')):
            old = camera_feed.encode_photo(raw, box, fit)
            new = paint.paint(question(box, Place(0, 0, *box, fit=mode), fmt='bmp8'), [raw])
            self.close(old, new, mean=1.5)

    def test_a_photo_that_moves_is_cut_larger(self):
        raw = jpeg((1600, 900))
        box = (800, 480)
        moving = camera_feed.motion_box(box)
        old = camera_feed.encode_photo(raw, box, 'auto', moving)
        new = paint.paint(question(box, Place(0, 0, *box, fit='auto', move=8), fmt='bmp8'), [raw])
        self.assertEqual(pixels(new).size, moving)
        self.close(old, new, mean=1.5)

    def test_an_older_screens_strip_of_squares(self):
        raws, grounds, size = [jpeg((640, 360)), None, jpeg((300, 300))], [0x202020, 0x445566, 0x101010], 96
        old = camera_feed.encode_live(raws, size, grounds)
        radius = max(4, size // camera_feed.LIVE_RADIUS_SHARE)
        places = tuple(Place(0, n * size, size, size, radius, ground=g) for n, g in enumerate(grounds))
        new = paint.paint(Question(Owner('tile', 0), (None,) * 3, (size, size * 3), places), raws)
        self.close(old, new, mean=1.5)

    def test_formats(self):
        image = Image.new('RGB', (64, 40), (200, 30, 30))
        self.assertTrue(paint.encode(image, 'bmp24').startswith(b'BM'))
        eight = paint.encode(image, 'bmp8')
        self.assertTrue(eight.startswith(b'BM'))
        self.assertLess(len(eight), len(paint.encode(image, 'bmp24')))
        jpg = paint.encode(image, 'jpeg')
        self.assertTrue(jpg.startswith(b'\xff\xd8'))
        # Baseline (SOF0), never progressive: the ESP32-P4's hardware decoder reads baseline only.
        self.assertIn(b'\xff\xc0', jpg)
        self.assertNotIn(b'\xff\xc2', jpg)


if __name__ == '__main__':
    unittest.main()
