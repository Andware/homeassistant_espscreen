<script setup lang="ts">
// The alert's wizard (Events, Alerts): what starts it, what it says (with the card as the screen draws it), how it looks,
// the screens, then one now to see it and the automation into Home Assistant, "Tessera: <its title>". One step at a time
// in UiWizard; the page lists the automation once it is saved.
import { computed, reactive, ref, watch } from "vue";
import { useBusy } from "../../composables/useBusy";
import { automationLink } from "../../composables/useAutomations";
import { t } from "../../i18n";
import { alertAutomation, screenValue, type AlertReference } from "../../model/alert-yaml";
import { automationYaml, triggerKind } from "../../model/automation";
import { glyph } from "../../model/topbar";
import { useInventoryStore } from "../../stores/inventory";
import { useScreenStore } from "../../stores/screen";
import { useUiStore } from "../../stores/ui";
import CheckRow from "../ui/CheckRow.vue";
import Icon from "../ui/Icon.vue";
import UiPicker from "../ui/UiPicker.vue";
import UiSwitch from "../ui/UiSwitch.vue";
import UiWizard from "../ui/UiWizard.vue";
import TriggerStep from "./TriggerStep.vue";

const props = defineProps<{ open: boolean; save: (automation: ReturnType<typeof alertAutomation>, id?: string | null) => Promise<{ id: string }> }>();
const emit = defineEmits<{ "update:open": [open: boolean] }>();
const inv = useInventoryStore();
const scr = useScreenStore();
const ui = useUiStore();
const alerts = computed(() => inv.inventory.alerts as (AlertReference & { suggested_icons?: { name: string; cp: string }[]; colors?: { name: string; label: string; color: string }[]; min_firmware?: string }) | undefined);
const entities = computed(() => inv.inventory.entities || []);
const nameOf = (id: string) => entities.value.find((e) => e.id === id)?.name || id;
const cameras = computed(() => entities.value.filter((e) => e.id.startsWith("camera.") || e.id.startsWith("image."))
  .map((e) => ({ id: e.id, name: e.name || e.id, hint: [e.area, e.id].filter(Boolean).join(" · ") })));
const screens = computed(() => inv.inventory.screens.filter((s) => !s.virtual));

const steps = computed(() => ["trigger", "words", "look", "screens", "try", "add"].map((id) => ({
  id, title: t(`editor.alerts.wizard.steps.${id}.title`), help: t(`editor.alerts.wizard.steps.${id}.help`) })));
const step = ref(1);
const fresh = () => ({ title: t("editor.alerts.example.title"), subtitle: t("editor.alerts.example.subtitle"), icon: "doorbell", color: "orange",
  button_text: t("editor.alerts.example.button"), timeout: 30, flash: true });
const form = reactive(fresh());
const choice = reactive({ trigger: "", camera: "", screens: [] as string[] });
const toggle = (value: string) => { choice.screens = choice.screens.includes(value) ? choice.screens.filter((v) => v !== value) : [...choice.screens, value]; };
const automation = computed(() => alertAutomation(alerts.value, { ...choice, kind: choice.trigger ? triggerKind(choice.trigger) : null, fields: { ...form } }));
const yaml = computed(() => automationYaml(automation.value));
const ready = computed(() => ({ trigger: Boolean(choice.trigger && triggerKind(choice.trigger)), words: Boolean(form.title.trim()) } as Record<string, boolean>)[steps.value[step.value - 1]?.id] ?? true);
const chosenScreens = computed(() => choice.screens.length ? screens.value.filter((s) => choice.screens.includes(screenValue(s))).map((s) => s.name).join(", ") : t("editor.events.wizard.every_screen"));
// The card as the screen draws it: its colour, its icon, its words and its button.
const colour = computed(() => alerts.value?.colors?.find((c) => c.name === form.color)?.color);
const iconCp = computed(() => alerts.value?.suggested_icons?.find((i) => i.name === form.icon)?.cp);

const { busy: trying, run: whileTrying } = useBusy();
const tried = ref("");
const tryIt = () => whileTrying(async () => {
  tried.value = "";
  try {
    const answer = await scr.sendTestAlert("all", { ...form, ...(choice.camera ? { camera: choice.camera } : {}), ...(choice.screens.length ? { screen: choice.screens } : {}) });
    tried.value = answer.sent ? t("editor.alerts.try.sent_all", answer.sent) : t("editor.alerts.try.nothing", { version: alerts.value?.min_firmware || "0.2.31" });
  } catch (e: any) {
    tried.value = e.message;
  }
});

const { busy: saving, run: whileSaving } = useBusy();
const saved = ref<{ id: string } | null>(null);
const error = ref("");
const add = () => whileSaving(async () => {
  error.value = "";
  try { saved.value = await props.save(automation.value, saved.value?.id); } catch (e: any) { error.value = e.message; }
});
watch(() => props.open, (open) => { if (open && saved.value) { Object.assign(form, fresh()); Object.assign(choice, { trigger: "", camera: "", screens: [] }); saved.value = null; tried.value = ""; step.value = 1; } });
</script>

<template>
  <UiWizard :open="open" :title="t('editor.alerts.wizard.title')" :steps="steps" v-model:step="step" :ready="ready" @update:open="(value) => emit('update:open', value)">
    <template #trigger>
      <TriggerStep id="alert-trigger" v-model="choice.trigger" />
    </template>
    <template #words>
      <div class="with-preview">
        <div class="fields">
          <div class="field"><label class="f-label" for="alert-title">{{ t("editor.alerts.try.title") }}</label><input id="alert-title" v-model="form.title" maxlength="64" /></div>
          <div class="field"><label class="f-label" for="alert-subtitle">{{ t("editor.alerts.try.subtitle") }}</label><input id="alert-subtitle" v-model="form.subtitle" maxlength="240" /></div>
          <div class="field"><label class="f-label" for="alert-button">{{ t("editor.alerts.try.button") }}</label><input id="alert-button" v-model="form.button_text" maxlength="16" /></div>
        </div>
        <div class="alert-mock" aria-hidden="true"><div class="alert-mock-card" :style="colour ? { background: colour } : undefined">
          <span class="mdi alert-mock-icon">{{ iconCp ? glyph(iconCp) : "" }}</span><strong>{{ form.title }}</strong><span>{{ form.subtitle }}</span><span class="fake">{{ form.button_text || "OK" }}</span></div></div>
      </div>
    </template>
    <template #look>
      <div class="with-preview">
        <div class="fields">
          <span class="f-label">{{ t("editor.alerts.try.icon") }}</span>
          <div class="icon-grid">
            <button v-for="icon in (alerts?.suggested_icons || []).slice(0, 14)" :key="icon.name" type="button" class="icon-pick mdi" :aria-pressed="form.icon === icon.name ? 'true' : 'false'" :title="icon.name" @click="form.icon = icon.name">{{ glyph(icon.cp) }}</button>
          </div>
          <span class="f-label">{{ t("editor.alerts.try.color") }}</span>
          <div class="swatches">
            <button type="button" class="swatch white" :aria-pressed="!form.color ? 'true' : 'false'" :title="t('editor.alerts.white')" @click="form.color = ''" />
            <button v-for="c in alerts?.colors || []" :key="c.name" type="button" class="swatch" :style="{ background: c.color }" :aria-pressed="form.color === c.name ? 'true' : 'false'" :title="c.label" @click="form.color = c.name" />
          </div>
          <span class="f-label">{{ t("editor.alerts.wizard.camera") }}</span>
          <div class="pick-row">
            <UiPicker id="alert-camera" :items="cameras" :placeholder="t('editor.plugin_tile.search')" :empty="t('editor.doorbell.no_camera')" width="340px" @pick="(id: string) => (choice.camera = id)">
              <template #trigger><button type="button" class="ghost-btn"><Icon name="magnify" />{{ choice.camera ? nameOf(choice.camera) : t("editor.alerts.wizard.no_camera") }}</button></template>
            </UiPicker>
            <button v-if="choice.camera" type="button" class="btn quiet mini" @click="choice.camera = ''"><Icon name="close" /></button>
          </div>
          <label class="switch-row" for="alert-flash"><UiSwitch id="alert-flash" v-model="form.flash" /><span>{{ t("editor.alerts.try.flash") }}<small>{{ t("editor.alerts.try.flash_hint") }}</small></span></label>
        </div>
        <div class="alert-mock" aria-hidden="true"><div class="alert-mock-card" :style="colour ? { background: colour } : undefined">
          <span class="mdi alert-mock-icon">{{ iconCp ? glyph(iconCp) : "" }}</span><strong>{{ form.title }}</strong><span>{{ form.subtitle }}</span><span class="fake">{{ form.button_text || "OK" }}</span></div></div>
      </div>
    </template>
    <template #screens>
      <div class="screen-rows">
        <CheckRow v-for="screen in screens" :key="screen.id" :checked="choice.screens.includes(screenValue(screen))" :disabled="!scr.canAlert(screen)" @toggle="toggle(screenValue(screen))">
          <span class="screen-row"><strong>{{ screen.name }}</strong><small>{{ scr.canAlert(screen) ? (screen.pictures ? t("editor.alerts.wizard.with_picture") : t("editor.alerts.wizard.without_picture")) : t("editor.alerts.screens.unknown") }}</small></span>
        </CheckRow>
      </div>
      <p class="hint">{{ t("editor.alerts.wizard.screens_hint") }}</p>
      <div class="field"><label class="f-label" for="alert-timeout">{{ t("editor.alerts.try.timeout") }}</label><input id="alert-timeout" v-model.number="form.timeout" type="number" min="0" max="86400" /></div>
    </template>
    <template #try>
      <button type="button" class="btn primary big-action" id="alert-try" :disabled="trying" @click="tryIt"><span class="mdi">{{ iconCp ? glyph(iconCp) : "" }}</span>{{ trying ? t("editor.alerts.try.sending") : t("editor.alerts.try.send") }}</button>
      <p class="status-line" id="alert-try-result" role="status">{{ tried }}</p>
    </template>
    <template #add>
      <dl v-if="!saved" class="summary">
        <dt>{{ t("editor.events.wizard.summary.starts") }}</dt><dd>{{ nameOf(choice.trigger) }} · {{ t(`editor.events.wizard.kind.${triggerKind(choice.trigger) || 'event'}`) }}</dd>
        <dt>{{ t("editor.alerts.wizard.summary.says") }}</dt><dd>{{ form.title }}<template v-if="form.subtitle"> · {{ form.subtitle }}</template></dd>
        <dt>{{ t("editor.events.wizard.summary.screens") }}</dt><dd>{{ chosenScreens }}</dd>
        <dt>{{ t("editor.events.wizard.summary.title") }}</dt><dd>{{ automation.alias }}</dd>
      </dl>
      <div v-else class="saved" id="alert-saved">
        <Icon name="check" /><span class="saved-text"><b>{{ t("editor.events.install.saved") }}</b><small>{{ t("editor.events.install.hint", { title: automation.alias }) }}</small></span>
        <a class="btn quiet mini" :href="automationLink(saved.id)" target="_top">{{ t("editor.events.install.open") }}</a>
      </div>
      <p v-if="error" class="status-line error" role="alert">{{ error }}</p>
      <details class="yaml-details">
        <summary>{{ t("editor.events.wizard.yaml") }}</summary>
        <div class="copy-line"><pre id="alerts-automation-yaml">{{ yaml }}</pre>
          <button type="button" class="btn quiet mini" @click="ui.copyText(yaml, undefined, 'yaml')">{{ t("editor.alerts.copy_yaml") }}</button></div>
      </details>
    </template>
    <template #footer="{ last }">
      <template v-if="last">
        <button v-if="!saved" type="button" class="btn primary" id="alert-install" :disabled="saving" @click="add">{{ saving ? t("editor.events.install.saving") : t("editor.events.install.add") }}</button>
        <button v-else type="button" class="btn primary" @click="emit('update:open', false)">{{ t("editor.events.wizard.done") }}</button>
      </template>
    </template>
  </UiWizard>
</template>

<style scoped>
.with-preview { display: grid; grid-template-columns: minmax(0, 1fr) 240px; gap: 16px; align-items: start; }
.fields { display: grid; gap: 10px; }
.icon-grid { display: flex; flex-wrap: wrap; gap: 6px; }
.icon-pick { width: 36px; height: 36px; border-radius: 10px; border: 1px solid var(--line); background: var(--surface); color: var(--ink); font-size: 18px; cursor: pointer; }
.icon-pick[aria-pressed="true"] { border-color: var(--accent); background: var(--accent-soft); color: var(--accent); }
.swatches { display: flex; flex-wrap: wrap; gap: 8px; }
.swatch { width: 28px; height: 28px; border-radius: 50%; border: 1px solid var(--line); cursor: pointer; }
.swatch.white { background: var(--surface); }
.swatch[aria-pressed="true"] { box-shadow: 0 0 0 2px var(--surface), 0 0 0 4px var(--accent); }
.pick-row { display: flex; align-items: center; gap: 8px; }
.switch-row { display: flex; align-items: center; gap: 10px; cursor: pointer; }
.switch-row > span { display: grid; }
.switch-row small { color: var(--muted); font-size: 12px; }
.screen-rows { display: grid; gap: 6px; }
.screen-row { display: grid; gap: 2px; text-align: left; }
.screen-row small { color: var(--muted); font-size: 12px; }
.big-action { justify-self: start; display: inline-flex; align-items: center; gap: 8px; padding: 12px 20px; font-size: 14px; }
.summary { display: grid; grid-template-columns: max-content 1fr; gap: 8px 16px; margin: 0; font-size: 13.5px; }
.summary dt { color: var(--muted); }
.summary dd { margin: 0; overflow-wrap: anywhere; }
.saved { display: flex; align-items: center; gap: 10px; padding: 12px 14px; border-radius: 12px; background: var(--accent-soft); }
.saved-text { display: grid; gap: 2px; flex: 1; min-width: 0; }
.saved small { color: var(--muted); }
.yaml-details summary { cursor: pointer; color: var(--muted); font-size: 13px; }
.status-line.error { color: var(--danger); }
@media (max-width: 640px) { .with-preview { grid-template-columns: minmax(0, 1fr); } }
</style>
