<script setup lang="ts">
// The doorbell, a part of Events (EventsView, docs/DOORBELL.md): an automation fires esp_screens_doorbell with the
// doorbell's camera, started by whatever rings at that house, for the screens it names. Any doorbell fits: its wizard
// points at the person's own entities (what rings, the camera, the screens), offers first the doorbells Home Assistant
// knows as such (doorbell.suggestions), builds the automation to copy (model/doorbell-yaml.ts) and tries a ring with it.
import { computed, reactive, ref } from "vue";
import { useBusy } from "../composables/useBusy";
import { t } from "../i18n";
import { screenValue } from "../model/alert-yaml";
import { automationYaml as written } from "../model/automation";
import { dismissYaml, doorbellAutomation, missedYaml, type DoorbellKind, type DoorbellReference } from "../model/doorbell-yaml";
import { glyph } from "../model/topbar";
import { useInventoryStore } from "../stores/inventory";
import { useScreenStore } from "../stores/screen";
import { useUiStore } from "../stores/ui";
import Icon from "./ui/Icon.vue";
import UiChips from "./ui/UiChips.vue";
import UiPicker from "./ui/UiPicker.vue";
import AutomationInstall from "./events/AutomationInstall.vue";
import TriggerStep, { type Suggestion } from "./events/TriggerStep.vue";
import theme from "virtual:esp-screens-theme";

const ui = useUiStore();
const inv = useInventoryStore();
const scr = useScreenStore();
const reference = computed(() => inv.inventory.doorbell as DoorbellReference | undefined);
const physicalScreens = computed(() => inv.inventory.screens.filter((s) => !s.virtual));
const entities = computed(() => inv.inventory.entities || []);
const item = (e: { id: string; name?: string; area?: string }) => ({ id: e.id, name: e.name || e.id, hint: e.area || e.id });
const cameras = computed(() => entities.value.filter((e) => e.id.startsWith("camera.")).map(item));
const nameOf = (id: string) => entities.value.find((e) => e.id === id)?.name || id;
// The doorbells Home Assistant knows, offered first, each with the camera on its device.
const suggestions = computed<(Suggestion & { camera: string | null })[]>(() => (reference.value?.suggestions || [])
  .map((s) => ({ ...s, detail: [s.trigger, s.camera].filter(Boolean).join(" · ") })));
const kindOf = (screen: { doorbell?: DoorbellKind }) => screen.doorbell || "plain";
// The mock of the view in the firmware's own colours (theme.h): the camera's black page, its ink and note, the doorbell's
// amber, the round key's grey and the accent of the Sound key.
const hex = (role: string) => `#${theme.roles[role].light.toString(16).padStart(6, "0")}`;
const mock = {
  "--db-page": hex("CAMERA_PAGE"), "--db-ink": hex("CAMERA_INK"), "--db-note": hex("CAMERA_NOTE"), "--db-amber": hex("MARK_AMBER"),
  "--db-key": hex("KEY"), "--db-key-ink": hex("INK"), "--db-accent": hex("ACCENT"), "--db-on-accent": hex("ON_ACCENT"), "--db-track": hex("CAMERA_TRACK"),
};

// The wizard: what rings, the camera, the screens, its title and timeout. A suggestion picks the first two at once.
const choice = reactive({ trigger: "", camera: "", title: "", screens: [] as string[], timeout: undefined as number | undefined });
function suggest(s: Suggestion) {
  const camera = suggestions.value.find((item) => item.trigger === s.trigger)?.camera;
  if (camera) choice.camera = camera;
}
const screenChips = computed(() => physicalScreens.value.map((s) => ({ value: screenValue(s), label: s.name })));
const automation = computed(() => doorbellAutomation(reference.value, { ...choice, name: choice.camera ? nameOf(choice.camera) : "" }));
const yaml = computed(() => written(automation.value));
const ready = computed(() => Boolean(choice.trigger && choice.camera));

// Try it: one ring as this automation would send it, now.
const { busy: trying, run: whileTrying } = useBusy();
const result = ref("");
const tryIt = () => whileTrying(async () => {
  result.value = "";
  try {
    const answer = await scr.sendTestDoorbell("", { camera: choice.camera, title: choice.title, timeout: choice.timeout || undefined,
      ...(choice.screens.length ? { screen: choice.screens } : {}) });
    result.value = answer.sent || answer.alerts
      ? [answer.sent ? t("editor.doorbell.try.views", answer.sent) : "", answer.alerts ? t("editor.doorbell.try.alerts", answer.alerts) : ""].filter(Boolean).join(" ")
      : t("editor.doorbell.try.nothing");
  } catch (e: any) {
    result.value = e.message;
  }
});
</script>

<template>
  <div class="doorbell" id="doorbell-part">
    <p class="events-intro">{{ t("editor.doorbell.intro") }}</p>
    <div class="alert-hero">
      <div class="doorbell-mock" aria-hidden="true" :style="mock">
        <div class="doorbell-mock-head"><span class="mdi doorbell-mock-disc">{{ glyph("F12E6") }}</span>
          <span><strong>{{ t("editor.doorbell.example.title") }}</strong><small>{{ t("editor.doorbell.example.when") }}</small></span>
          <span class="mdi doorbell-mock-close">{{ glyph("F0156") }}</span></div>
        <div class="doorbell-mock-body"><span class="doorbell-mock-picture" /><span class="doorbell-mock-key"><span class="mdi">{{ glyph("F057E") }}</span>{{ t("editor.doorbell.example.sound") }}</span></div>
      </div>
      <ol class="alerts-list">
        <li>{{ t("editor.doorbell.steps.ring") }}</li>
        <li>{{ t("editor.doorbell.steps.answer") }}</li>
        <li>{{ t("editor.doorbell.steps.missed") }}</li>
      </ol>
    </div>

    <section id="doorbell-wizard" class="card">
      <h2>{{ t("editor.doorbell.wizard.title") }}</h2>
      <p>{{ t("editor.doorbell.wizard.text") }}</p>
      <ol class="wizard">
        <li>
          <b>{{ t("editor.doorbell.wizard.step_trigger") }}</b>
          <TriggerStep id="doorbell-trigger" v-model="choice.trigger" :suggestions="suggestions" :domains="reference?.trigger_domains" :mark="glyph('F12E6')" @suggest="suggest" />
        </li>
        <li>
          <b>{{ t("editor.doorbell.wizard.step_camera") }}</b>
          <div class="wizard-row">
            <UiPicker id="doorbell-camera" :items="cameras" :placeholder="t('editor.plugin_tile.search')" :empty="t('editor.doorbell.no_camera')" width="340px" @pick="(id: string) => (choice.camera = id)">
              <template #trigger><button type="button" class="ghost-btn"><Icon name="magnify" />{{ choice.camera ? nameOf(choice.camera) : t("editor.doorbell.wizard.pick_camera") }}</button></template>
            </UiPicker>
            <code v-if="choice.camera">{{ choice.camera }}</code>
          </div>
          <p class="hint">{{ t("editor.doorbell.wizard.camera_hint") }}</p>
        </li>
        <li>
          <b>{{ t("editor.doorbell.wizard.step_screens") }}</b>
          <UiChips v-model="choice.screens" :chips="screenChips" :label="t('editor.doorbell.wizard.step_screens')" />
          <p class="hint">{{ t("editor.doorbell.wizard.screens_hint") }}</p>
        </li>
        <li>
          <b>{{ t("editor.doorbell.wizard.step_details") }}</b>
          <div class="try-grid">
            <div class="field"><label class="f-label" for="doorbell-title">{{ t("editor.doorbell.fields.title") }}</label><input id="doorbell-title" v-model="choice.title" maxlength="60" :placeholder="choice.camera ? nameOf(choice.camera) : ''" /></div>
            <div class="field"><label class="f-label" for="doorbell-timeout">{{ t("editor.doorbell.fields.timeout") }}</label><input id="doorbell-timeout" v-model.number="choice.timeout" type="number" :min="reference?.timeout?.min ?? 10" :max="reference?.timeout?.max ?? 600" :placeholder="String(reference?.timeout?.default ?? 60)" /></div>
          </div>
        </li>
      </ol>
      <AutomationInstall kind="doorbell" :automation="automation" :ready="ready" :not-ready="t('editor.doorbell.wizard.not_ready')" />
      <div class="actions">
        <button type="button" class="btn quiet" id="doorbell-try" :disabled="trying || !choice.camera" @click="tryIt">{{ trying ? t("editor.alerts.try.sending") : t("editor.doorbell.try.send") }}</button>
        <span class="status-line" id="doorbell-try-result" role="status">{{ result }}</span>
      </div>
      <p class="hint">{{ t("editor.doorbell.wizard.limits") }}</p>
      <details class="yaml-details">
        <summary>{{ t("editor.events.wizard.yaml") }}</summary>
        <div class="copy-line">
          <pre id="doorbell-automation-yaml">{{ yaml }}</pre>
          <button type="button" class="btn quiet mini" id="doorbell-automation-copy" @click="ui.copyText(yaml, undefined, 'yaml')">{{ t("editor.alerts.copy_yaml") }}</button>
        </div>
      </details>
    </section>

    <section id="doorbell-screens" class="card">
      <h2>{{ t("editor.doorbell.screens.title") }}</h2>
      <p>{{ t("editor.doorbell.screens.text") }}</p>
      <div class="options">
        <p v-if="!physicalScreens.length" class="hint">{{ t("editor.alerts.screens.none") }}</p>
        <div v-for="screen in physicalScreens" :key="screen.id" class="alert-screen">
          <div class="alert-screen-head">
            <strong>{{ screen.name }}</strong>
            <span class="chip" :class="kindOf(screen) === 'view' ? 'good' : ''" :data-kind="kindOf(screen)">{{ t(`editor.doorbell.screens.${kindOf(screen)}`) }}</span>
          </div>
          <div class="copy-line alert-screen-value">
            <span class="copy-label">{{ t("editor.alerts.one.value") }}</span><code>{{ screenValue(screen) }}</code>
            <button type="button" class="btn quiet mini" @click="ui.copyText(screenValue(screen), undefined, 'screen_name')">{{ t("editor.common.copy") }}</button>
          </div>
        </div>
      </div>
    </section>

    <section id="doorbell-fields" class="card">
      <h2>{{ t("editor.doorbell.fields_title") }}</h2>
      <table class="fields-table">
        <tr v-for="field in reference?.fields || []" :key="field.name"><td><code>{{ field.name }}</code></td><td>{{ field.help }}</td></tr>
      </table>
    </section>

    <section id="doorbell-after" class="card">
      <h2>{{ t("editor.doorbell.after.title") }}</h2>
      <i18n-t keypath="editor.doorbell.after.text" tag="p" scope="global">
        <template #answered><code>{{ reference?.answered || "esp_screens_doorbell_answered" }}</code></template>
        <template #missed><code>{{ reference?.missed || "esp_screens_doorbell_missed" }}</code></template>
      </i18n-t>
      <div class="copy-line">
        <pre id="doorbell-missed-yaml">{{ missedYaml(reference) }}</pre>
        <button type="button" class="btn quiet mini" @click="ui.copyText(missedYaml(reference), undefined, 'yaml')">{{ t("editor.alerts.copy_yaml") }}</button>
      </div>
      <p>{{ t("editor.doorbell.after.dismiss") }}</p>
      <div class="copy-line">
        <pre id="doorbell-dismiss-yaml">{{ dismissYaml(reference) }}</pre>
        <button type="button" class="btn quiet mini" @click="ui.copyText(dismissYaml(reference), undefined, 'yaml')">{{ t("editor.alerts.copy_yaml") }}</button>
      </div>
    </section>
  </div>
</template>

<style scoped>
.doorbell { display: grid; gap: 14px; }
.doorbell h2 { font-size: 15px; }
.wizard { margin: 0; padding-left: 20px; display: grid; gap: 14px; }
.wizard > li { display: grid; gap: 8px; }
.wizard-row { display: flex; flex-wrap: wrap; align-items: center; gap: 8px; }
.yaml-details summary { cursor: pointer; color: var(--muted); font-size: 13px; }
.doorbell-mock { width: 300px; max-width: 100%; aspect-ratio: 16 / 10; border-radius: 14px; background: var(--db-page); color: var(--db-ink); padding: 10px; display: grid; grid-template-rows: auto 1fr; gap: 8px; }
.doorbell-mock-head { display: flex; align-items: center; gap: 8px; font-size: 12px; }
.doorbell-mock-head > span:nth-child(2) { display: grid; flex: 1; line-height: 1.2; }
.doorbell-mock-head small { color: var(--db-note); font-size: 10px; }
.doorbell-mock-disc { width: 26px; height: 26px; border-radius: 50%; background: var(--db-amber); color: var(--db-page); display: grid; place-items: center; font-size: 15px; }
.doorbell-mock-close { width: 24px; height: 24px; border-radius: 50%; background: var(--db-key); color: var(--db-key-ink); display: grid; place-items: center; font-size: 14px; }
.doorbell-mock-body { display: grid; grid-template-columns: 1fr 34%; gap: 8px; align-items: end; }
.doorbell-mock-picture { align-self: stretch; border-radius: 8px; background: var(--db-track); }
.doorbell-mock-key { display: inline-flex; align-items: center; gap: 4px; padding: 6px 8px; border-radius: 999px; background: var(--db-accent); color: var(--db-on-accent); font-size: 10px; }
.fields-table { border-collapse: collapse; width: 100%; font-size: 13px; }
.fields-table td { padding: 6px 8px; border-top: 1px solid var(--line); vertical-align: top; }
.fields-table td:first-child { white-space: nowrap; width: 1%; }
</style>
