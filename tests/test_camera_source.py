"""What a camera gives (screen_manager/app/camera_source.py, docs/CAMERA.md "Live on the P4 boards"): a camera with a
stream until WebRTC's first frame, its stills first, and an offer Home Assistant refuses giving way to the snapshots."""
import asyncio
import contextlib
import importlib.util
import io
from pathlib import Path
import sys
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'screen_manager/app'))
import camera_source  # noqa: E402
import live_stream  # noqa: E402
from PIL import Image  # noqa: E402


def jpeg(width, height, **options):
    out = io.BytesIO()
    Image.new('RGB', (width, height), (40, 120, 200)).save(out, 'JPEG', **options)
    return out.getvalue()


class WaitForWebRTC(unittest.TestCase):
    """A camera with a stream until WebRTC's first frame: its stills keep the screen's stream going (the screen closes one
    silent for 6 s, live_view::QUIET_MS), and an offer Home Assistant refuses gives way to the snapshots at once."""

    def test_the_still_comes_again_until_the_first_frame(self):
        async def main():
            stills = []

            async def fetch(entity):
                stills.append(entity)
                return jpeg(64, 36)

            source = camera_source.Source(live_stream.LiveFeed(fetch, None, lambda e: True), 'camera.door', True)
            source.viewers = 1

            async def rtc(kinds, take, keep):
                await asyncio.sleep(0.35)  # a camera's key frame that takes its time
                await take('video', object())
                return True
            source.rtc = rtc
            before = camera_source.STILL_SECONDS
            camera_source.STILL_SECONDS = 0.1
            try:
                self.assertTrue(await source.webrtc())
                came = len(stills)
                await asyncio.sleep(0.3)
            finally:
                camera_source.STILL_SECONDS = before
            # A still every STILL_SECONDS while WebRTC waited, the first at once; none once its frame came.
            self.assertGreaterEqual(came, 3)
            self.assertEqual(len(stills), came)
            self.assertIsNotNone(source.frame.video)

        asyncio.run(main())

    @unittest.skipUnless(importlib.util.find_spec('aiortc'), 'needs aiortc (the app has it)')
    def test_a_refused_offer_gives_way_at_once(self):
        class Message:
            def json(self):
                return {'id': 1, 'type': 'result', 'success': False,
                        'error': {'message': 'Camera does not support WebRTC'}}

        sent = []

        class Socket:
            async def send_json(self, data):
                sent.append(asyncio.get_running_loop().time())

            def __aiter__(self):
                async def messages():
                    yield Message()
                    await asyncio.sleep(60)
                return messages()

        @contextlib.asynccontextmanager
        async def websocket():
            yield Socket()

        async def main():
            source = camera_source.Source(live_stream.LiveFeed(None, websocket, lambda e: True), 'camera.door', True)
            started = asyncio.get_running_loop().time()
            with self.assertLogs('camera_source', 'INFO') as logged:
                gave = await source.rtc(('video',), None, lambda: True)
            done = asyncio.get_running_loop().time()
            return gave, sent[0] - started, done - sent[0], logged.output

        gave, offering, refused, output = asyncio.run(main())
        self.assertFalse(gave)
        # The offer goes out at once (no STUN server to wait for, about five seconds before), and the refusal ends the
        # session at once, not after WEBRTC_FIRST_SECONDS.
        self.assertLess(offering, 1.5)
        self.assertLess(refused, 1.5)
        self.assertTrue(any('Camera does not support WebRTC' in line for line in output), output)


if __name__ == '__main__':
    unittest.main()
