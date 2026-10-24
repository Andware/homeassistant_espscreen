<script setup lang="ts">
// The doorbell's wizard (Events, Doorbell; docs/DOORBELL.md): what rings, its camera, the screens, a name, then a ring now
// to see it and the automation into Home Assistant, "Tessera: Doorbell <name>". One step at a time in UiWizard; what is
// chosen stays while the window is open, and the page lists the automation once it is saved.
import { computed, reactive, ref, watch } from "vue";
import { useBusy } from "../../composables/useBusy";
import { automationLink } from "../../composables/useAutomations";
import { t } from "../../i18n";
import { screenValue } from "../../model/alert-yaml";
import { automationYaml } from "../../model/automation";
import { doorbellAutomation, triggerKind, type DoorbellKind, type DoorbellReference } from "../../model/doorbell-yaml";
import { glyph } from "../../model/topbar";
import { useInventoryStore } from "../../stores/inventory";
import { useScreenStore } from "../../stores/screen";
import { useUiStore } from "../../stores/ui";
import CheckRow from "../ui/CheckRow.vue";
import Icon from "../ui/Icon.vue";
import UiChoiceCards from "../ui/UiChoiceCards.vue";
import UiPicker from "../ui/UiPicker.vue";
import UiWizard from "../ui/UiWizard.vue";
import TriggerStep, { type Suggestion } from "./TriggerStep.vue";

const props = defineProps<{ open: boolean; save: (automation: ReturnType<typeof doorbellAutomation>, id?: string | null) => Promise<{ id: string }> }>();
const emit = defineEmits<{ "update:open": [open: boolean] }>();
const inv = useInventoryStore();
const scr = useScreenStore();
const ui = useUiStore();
const reference = computed(() => inv.inventory.doorbell as DoorbellReference | undefined);
const entities = computed(() => inv.inventory.entities || []);
const nameOf = (id: string) => entities.value.find((e) => e.id === id)?.name || id;
const cameras = computed(() => entities.value.filter((e) => e.id.startsWith("camera.")).map((e) => ({ id: e.id, name: e.name || e.id, hint: [e.area, e.id].filter(Boolean).join(" · ") })));
const screens = computed(() => inv.inventory.screens.filter((s) => !s.virtual));
const kindOf = (screen: { doorbell?: DoorbellKind }) => screen.doorbell || "plain";
const suggestions = computed<(Suggestion & { camera: string | null })[]>(() => (reference.value?.suggestions || [])
  .map((s) => ({ ...s, detail: [s.trigger, s.camera].filter(Boolean).join(" · ") })));

const steps = computed(() => ["rings", "camera", "screens", "name", "try", "add"].map((id) => ({
  id, title: t(`editor.doorbell.wizard.steps.${id}.title`), help: t(`editor.doorbell.wizard.steps.${id}.help`) })));
const step = ref(1);
const choice = reactive({ trigger: "", camera: "", screens: [] as string[], title: "", timeout: undefined as number | undefined });
function suggest(s: Suggestion) {
  const camera = suggestions.value.find((item) => item.trigger === s.trigger)?.camera;
  if (camera) choice.camera = camera;
}
const cameraCards = computed(() => cameras.value.slice(0, 4).map((c) => ({ value: c.id, title: c.name, detail: c.hint, glyph: glyph("F07AE") })));
const toggle = (value: string) => { choice.screens = choice.screens.includes(value) ? choice.screens.filter((v) => v !== value) : [...choice.screens, value]; };
const automation = computed(() => doorbellAutomation(reference.value, { ...choice, name: choice.camera ? nameOf(choice.camera) : "" }));
const yaml = computed(() => automationYaml(automation.value));
const ready = computed(() => ({ rings: Boolean(choice.trigger && triggerKind(reference.value, choice.trigger)), camera: Boolean(choice.camera) } as Record<string, boolean>)[steps.value[step.value - 1]?.id] ?? true);
const chosenScreens = computed(() => choice.screens.length ? screens.value.filter((s) => choice.screens.includes(screenValue(s))).map((s) => s.name).join(", ") : t("editor.events.wizard.every_screen"));

// A ring now, as the automation would send it.
const { busy: trying, run: whileTrying } = useBusy();
const tried = ref("");
const tryIt = () => whileTrying(async () => {
  tried.value = "";
  try {
    const answer = await scr.sendTestDoorbell("", { camera: choice.camera, title: choice.title, timeout: choice.timeout || undefined,
      ...(choice.screens.length ? { screen: choice.screens } : {}) });
    tried.value = answer.sent || answer.alerts
      ? [answer.sent ? t("editor.doorbell.try.views", answer.sent) : "", answer.alerts ? t("editor.doorbell.try.alerts", answer.alerts) : ""].filter(Boolean).join(" ")
      : t("editor.doorbell.try.nothing");
  } catch (e: any) {
    tried.value = e.message;
  }
});

// Into Home Assistant: a new automation, or the same one again after a change.
const { busy: saving, run: whileSaving } = useBusy();
const saved = ref<{ id: string } | null>(null);
const error = ref("");
const add = () => whileSaving(async () => {
  error.value = "";
  try { saved.value = await props.save(automation.value, saved.value?.id); } catch (e: any) { error.value = e.message; }
});
// A new wizard starts afresh once the last one was added.
watch(() => props.open, (open) => { if (open && saved.value) { Object.assign(choice, { trigger: "", camera: "", screens: [], title: "", timeout: undefined }); saved.value = null; tried.value = ""; step.value = 1; } });
</script>

<template>
  <UiWizard :open="open" :title="t('editor.doorbell.wizard.title')" :steps="steps" v-model:step="step" :ready="ready" @update:open="(value) => emit('update:open', value)">
    <template #rings>
      <TriggerStep id="doorbell-trigger" v-model="choice.trigger" :suggestions="suggestions" :domains="reference?.trigger_domains" :mark="glyph('F12E6')" @suggest="suggest" />
    </template>
    <template #camera>
      <UiChoiceCards v-if="cameraCards.length" :model-value="choice.camera" :cards="cameraCards" :label="t('editor.doorbell.wizard.steps.camera.title')" @update:model-value="(id) => (choice.camera = id)" />
      <UiPicker v-if="cameras.length > 4 || !cameras.length" id="doorbell-camera" :items="cameras" :placeholder="t('editor.plugin_tile.search')" :empty="t('editor.doorbell.no_camera')" width="380px" @pick="(id: string) => (choice.camera = id)">
        <template #trigger><button type="button" class="ghost-btn wide-pick"><Icon name="magnify" />{{ choice.camera && !cameraCards.some((c) => c.value === choice.camera) ? nameOf(choice.camera) : t("editor.doorbell.wizard.more_cameras") }}</button></template>
      </UiPicker>
      <p class="hint">{{ t("editor.doorbell.wizard.camera_hint") }}</p>
    </template>
    <template #screens>
      <div class="screen-rows">
        <CheckRow v-for="screen in screens" :key="screen.id" :checked="choice.screens.includes(screenValue(screen))" @toggle="toggle(screenValue(screen))">
          <span class="screen-row"><strong>{{ screen.name }}</strong><small :data-kind="kindOf(screen)">{{ t(`editor.doorbell.screens.${kindOf(screen)}`) }}</small></span>
        </CheckRow>
      </div>
      <p class="hint">{{ t("editor.doorbell.wizard.screens_hint") }}</p>
    </template>
    <template #name>
      <div class="field"><label class="f-label" for="doorbell-title">{{ t("editor.doorbell.fields.title") }}</label>
        <input id="doorbell-title" v-model="choice.title" maxlength="60" :placeholder="choice.camera ? nameOf(choice.camera) : ''" /></div>
      <div class="field"><label class="f-label" for="doorbell-timeout">{{ t("editor.doorbell.fields.timeout") }}</label>
        <input id="doorbell-timeout" v-model.number="choice.timeout" type="number" :min="reference?.timeout?.min ?? 10" :max="reference?.timeout?.max ?? 600" :placeholder="String(reference?.timeout?.default ?? 60)" />
        <small class="hint">{{ t("editor.doorbell.wizard.timeout_hint") }}</small></div>
    </template>
    <template #try>
      <button type="button" class="btn primary big-action" id="doorbell-try" :disabled="trying" @click="tryIt"><span class="mdi">{{ glyph("F12E6") }}</span>{{ trying ? t("editor.alerts.try.sending") : t("editor.doorbell.try.send") }}</button>
      <p class="status-line" id="doorbell-try-result" role="status">{{ tried }}</p>
      <p class="hint">{{ t("editor.doorbell.wizard.limits") }}</p>
    </template>
    <template #add>
      <dl v-if="!saved" class="summary">
        <dt>{{ t("editor.events.wizard.summary.starts") }}</dt><dd>{{ nameOf(choice.trigger) }} · {{ t(`editor.events.wizard.kind.${triggerKind(reference, choice.trigger) || 'event'}`) }}</dd>
        <dt>{{ t("editor.doorbell.wizard.summary.camera") }}</dt><dd>{{ nameOf(choice.camera) }}</dd>
        <dt>{{ t("editor.events.wizard.summary.screens") }}</dt><dd>{{ chosenScreens }}</dd>
        <dt>{{ t("editor.events.wizard.summary.title") }}</dt><dd>{{ automation.alias }}</dd>
      </dl>
      <div v-else class="saved" id="doorbell-saved">
        <Icon name="check" /><span class="saved-text"><b>{{ t("editor.events.install.saved") }}</b><small>{{ t("editor.events.install.hint", { title: automation.alias }) }}</small></span>
        <a class="btn quiet mini" :href="automationLink(saved.id)" target="_top">{{ t("editor.events.install.open") }}</a>
      </div>
      <p v-if="error" class="status-line error" role="alert">{{ error }}</p>
      <details class="yaml-details">
        <summary>{{ t("editor.events.wizard.yaml") }}</summary>
        <div class="copy-line"><pre id="doorbell-automation-yaml">{{ yaml }}</pre>
          <button type="button" class="btn quiet mini" @click="ui.copyText(yaml, undefined, 'yaml')">{{ t("editor.alerts.copy_yaml") }}</button></div>
      </details>
    </template>
    <template #footer="{ last }">
      <template v-if="last">
        <button v-if="!saved" type="button" class="btn primary" id="doorbell-install" :disabled="saving" @click="add">{{ saving ? t("editor.events.install.saving") : t("editor.events.install.add") }}</button>
        <button v-else type="button" class="btn primary" @click="emit('update:open', false)">{{ t("editor.events.wizard.done") }}</button>
      </template>
    </template>
  </UiWizard>
</template>

<style scoped>
.wide-pick { width: 100%; justify-content: flex-start; }
.screen-rows { display: grid; gap: 6px; }
.screen-row { display: grid; gap: 2px; text-align: left; }
.screen-row small { color: var(--muted); font-size: 12px; }
.screen-row small[data-kind="view"] { color: var(--accent); }
.big-action { justify-self: start; display: inline-flex; align-items: center; gap: 8px; padding: 12px 20px; font-size: 14px; }
.summary { display: grid; grid-template-columns: max-content 1fr; gap: 8px 16px; margin: 0; font-size: 13.5px; }
.summary dt { color: var(--muted); }
.summary dd { margin: 0; overflow-wrap: anywhere; }
.saved { display: flex; align-items: center; gap: 10px; padding: 12px 14px; border-radius: 12px; background: var(--accent-soft); }
.saved-text { display: grid; gap: 2px; flex: 1; min-width: 0; }
.saved small { color: var(--muted); }
.yaml-details summary { cursor: pointer; color: var(--muted); font-size: 13px; }
.status-line.error { color: var(--danger); }
</style>
