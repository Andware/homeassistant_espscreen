// A list whose rows are dragged into another order (the screensaver's steps, a top bar's items in the bar at the head of
// its inspector) follows the pointer on the
// whole page while a row is held. A drawer that closes in the middle of it (another tile chosen, Escape) lets go of the
// page: no listener on the document and no hold that still starts a drag after it is gone.
import { mount } from "@vue/test-utils";
import { beforeEach, describe, expect, it, vi } from "vitest";
import ScreensaverCard from "../src/components/ScreensaverCard.vue";
import TopbarInspector from "../src/components/TopbarInspector.vue";
import type { Inventory } from "../src/types";
import { loadLayout } from "./helpers/fixtures";
import { liveListeners } from "./helpers/browser";
import { useScreenStore } from "../src/stores/screen";
import { useInventoryStore } from "../src/stores/inventory";
import { useDocumentStore } from "../src/stores/document";
import { useInspectorStore } from "../src/stores/inspector";

let insp: ReturnType<typeof useInspectorStore>;
beforeEach(() => { insp = useInspectorStore(); });

let doc: ReturnType<typeof useDocumentStore>;
beforeEach(() => { doc = useDocumentStore(); });

// The pointer and touch listeners on the document (jsdom adds some of its own the first time a selector runs).
const following = (live: () => string[]) => live().filter((name) => /^(pointer|touch)/.test(name));
const press = (target: Element, pointerType: string) => target.dispatchEvent(Object.assign(
  new Event("pointerdown", { bubbles: true, cancelable: true }), { pointerId: 1, pointerType, button: 0, clientX: 10, clientY: 10 }));

beforeEach(() => {
  vi.useFakeTimers();
  vi.stubGlobal("fetch", vi.fn(() => Promise.reject(new Error("offline"))));
  useInventoryStore().inventory = { screens: [{ id: "living", name: "Living", virtual: true, shape: { width: 480, height: 480 }, screensaver: {
    show: true, media: "", more: [], camera: "", order: ["media", "camera", "clock"], off: [], weather: "auto", items: [], ready: true,
    pictures: true, standby: true } }], entities: [], builtin: [], header: { max_items: 6, builtin: [] } } as unknown as Inventory;
  useScreenStore().selected = "living";
});

describe("a dragged list that closes while a row is held", () => {
  it.each(["touch", "mouse"])("the screensaver's steps let go of the page (%s)", async (pointer) => {
    const tracked = liveListeners(document), live = () => following(tracked);
    const card = mount(ScreensaverCard, { attachTo: document.body });
    press(card.find('.saver-row[data-kind="clock"]').element, pointer);
    expect(live()).toEqual(["pointercancel", "pointermove", "pointerup"]);
    card.unmount();
    expect(live()).toEqual([]);
    // The hold a finger starts would have begun the drag after the drawer was gone, and held the page's scrolling.
    await vi.advanceTimersByTimeAsync(1000);
    expect(live()).toEqual([]);
  });

  it.each(["touch", "mouse"])("a top bar's items let go of the page (%s)", async (pointer) => {
    loadLayout({ title: "Living", tiles: [], header: { items: [{ type: "clock" }, { type: "date" }] } } as any);
    insp.openBar(0);
    const tracked = liveListeners(document), live = () => following(tracked);
    const drawer = mount(TopbarInspector, { props: { index: 0 }, attachTo: document.body });
    press(drawer.find(".bar-strip .strip-item[data-index='1']").element, pointer);
    expect(live()).toEqual(["pointercancel", "pointermove", "pointerup"]);
    drawer.unmount();
    expect(live()).toEqual([]);
    await vi.advanceTimersByTimeAsync(1000);
    expect(live()).toEqual([]);
  });
});

describe("a top bar's items dragged into another order", () => {
  it("takes the place the pointer is over, keeps the drawer on the item, and is no click when let go", async () => {
    loadLayout({ title: "Living", tiles: [], header: { items: [{ type: "clock" }, { type: "date" }, { type: "analog" }] } } as any);
    insp.openBar(0);
    const drawer = mount(TopbarInspector, { props: { index: 0 }, attachTo: document.body });
    // The items stand side by side in the bar (app 0.4.89).
    drawer.findAll(".bar-strip .strip-item[data-index]").forEach((slot, i) => {
      slot.element.getBoundingClientRect = () => ({ left: i * 60, width: 60, right: i * 60 + 60, top: 0, bottom: 34, height: 34 }) as DOMRect;
    });
    const at = (x: number, type: string) => Object.assign(new Event(type, { bubbles: true, cancelable: true }), { pointerId: 1, pointerType: "mouse", button: 0, clientX: x, clientY: 10 });
    drawer.find(".bar-strip .strip-item[data-index='0']").element.dispatchEvent(at(10, "pointerdown"));
    document.dispatchEvent(at(70, "pointermove"));
    expect(insp.inspector).toEqual({ kind: "bar", index: 1 });
    document.dispatchEvent(at(70, "pointerup"));
    expect(doc.document!.pages[0].topbar.trailing.map((item) => item.type)).toEqual(["date", "clock", "analog"]);
    // The click the browser sends after the drag opens nothing.
    await drawer.find(".bar-strip .strip-item[data-index='0']").trigger("click");
    expect(insp.inspector).toEqual({ kind: "bar", index: 1 });
  });
});
