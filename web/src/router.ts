// The editor's address (vue-router, hash history): the page lives in Home Assistant's ingress iframe, where only the
// hash is the page's own, so every view is `#/...`. The address is the one place that says what is open: the overview,
// a screen, a page of the app, or a screen's Override YAML. A reload opens the same again, the browser's Back and
// Forward walk through what was open, and a link to a screen or an override can be kept. The hashes from before the
// router (`#settings`, `#plugins`) still work: vue-router reads `#settings` as `/settings`.
//
// App.vue chooses what to draw from the route's name and the stores (a screen route draws the screen once its layout
// is there, the overview when there are screens, else the empty page), so every route carries the same empty component.
import { createRouter, createWebHashHistory, START_LOCATION, type RouteLocationRaw } from "vue-router";

const Nothing = { render: () => null };

export const router = createRouter({
  history: createWebHashHistory(),
  routes: [
    { path: "/", name: "home", component: Nothing },
    // A screen by its inbox entity id (text.kitchen_tile_settings): what the inventory calls it.
    { path: "/screens/:id", name: "screen", component: Nothing },
    { path: "/settings", name: "settings", component: Nothing },
    { path: "/new-screen", name: "new-screen", component: Nothing },
    { path: "/firmware", name: "firmware", component: Nothing },
    { path: "/alerts", name: "alerts", component: Nothing },
    { path: "/plugins", name: "plugins", component: Nothing },
    // The Override YAML of a screen's ESPHome profile (kitchen.yaml); without a profile the page says so.
    { path: "/override/:profile?", name: "override", component: Nothing },
    { path: "/:pathMatch(.*)*", redirect: "/" },
  ],
});

// Every new view starts at the top, as a page does.
router.afterEach((to, from) => { if (to.path !== from.path) window.scrollTo(0, 0); });

// Home Assistant's sidebar shows the editor in an iframe it makes anew on every reload of its page, at the editor's bare
// address: the hash would be gone, and with it the open screen. So the last address is kept in this tab (sessionStorage:
// this tab alone, gone with it), and a page that starts at the overview goes back to it. An address someone typed or
// kept (#/screens/..., #settings) wins: it is there, and nothing is restored over it; and a tab whose last address was
// the overview itself stays there. (The router has already read the address when this runs: a bare one is `/` by then.)
const KEPT = "esp-screens.route";
router.afterEach((to, from) => {
  // The router's first navigation reads the address the page started with: a bare one is the overview, and keeping that
  // would write over the address this tab had before the reload, which restoreKeptRoute is about to read.
  if (from === START_LOCATION && to.path === "/") return;
  try { sessionStorage.setItem(KEPT, to.fullPath); } catch { /* storage may be off */ }
});
export async function restoreKeptRoute() {
  if (router.currentRoute.value.path !== "/") return;
  let kept: string | null = null;
  try { kept = sessionStorage.getItem(KEPT); } catch { /* storage may be off */ }
  if (kept && kept !== "/") await router.replace(kept);
}

export const screenRoute = (id: string): RouteLocationRaw => ({ name: "screen", params: { id } });
export const overrideRoute = (profile: string | null): RouteLocationRaw => ({ name: "override", params: { profile: profile ?? "" } });
