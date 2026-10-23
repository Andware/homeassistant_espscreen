<script setup lang="ts">
// The alert's wizard (Events, Alerts, its first card): what starts it (any entity of the person's own), the alert's fields
// as an automation sends them, its camera and its screens; then the automation into Home Assistant (AutomationInstall,
// "Tessera: <its title>"), or one alert now to see it. The example is the doorbell, in the editor's language.
import { computed, reactive, ref } from "vue";
import { useBusy } from "../../composables/useBusy";
import { t } from "../../i18n";
import { alertAutomation, screenValue, type AlertReference } from "../../model/alert-yaml";
import { automationYaml, triggerKind } from "../../model/automation";
import { useInventoryStore } from "../../stores/inventory";
import { useScreenStore } from "../../stores/screen";
import { useUiStore } from "../../stores/ui";
import AutomationInstall from "../events/AutomationInstall.vue";
import TriggerStep from "../events/TriggerStep.vue";
import Icon from "../ui/Icon.vue";
import UiChips from "../ui/UiChips.vue";
import UiPicker from "../ui/UiPicker.vue";

const inv = useInventoryStore();
const scr = useScreenStore();
const ui = useUiStore();
const alerts = computed(() => inv.inventory.alerts);
const form = reactive({
  title: t("editor.alerts.example.title"), subtitle: t("editor.alerts.example.subtitle"), icon: "doorbell", color: "orange",
  button_text: t("editor.alerts.example.button"), timeout: 30, flash: true,
});
const choice = reactive({ trigger: "", camera: "", screens: [] as string[] });
const physicalScreens = computed(() => inv.inventory.screens.filter((s) => !s.virtual));
const readyScreens = computed(() => physicalScreens.value.filter((s) => scr.canAlert(s) && s.online));
const screenChips = computed(() => physicalScreens.value.map((s) => ({ value: screenValue(s), label: s.name, disabled: !scr.canAlert(s) })));
const cameras = computed(() => (inv.inventory.entities || []).filter((e) => e.id.startsWith("camera.") || e.id.startsWith("image."))
  .map((e) => ({ id: e.id, name: e.name || e.id, hint: e.area || e.id })));
const nameOf = (id: string) => (inv.inventory.entities || []).find((e) => e.id === id)?.name || id;
const automation = computed(() => alertAutomation(alerts.value as AlertReference | undefined,
  { ...choice, kind: choice.trigger ? triggerKind(choice.trigger) : null, fields: { ...form } }));
const yaml = computed(() => automationYaml(automation.value));

// One alert now, as the event would send it, to the chosen screens or to every screen that can show one.
const { busy: trying, run: whileTrying } = useBusy();
const result = ref("");
const send = () => whileTrying(async () => {
  result.value = "";
  try {
    const data = { ...form, ...(choice.camera ? { camera: choice.camera } : {}), ...(choice.screens.length ? { screen: choice.screens } : {}) };
    const answer = await scr.sendTestAlert("all", data);
    result.value = answer.sent
      ? [t("editor.alerts.try.sent_all", answer.sent), answer.skipped ? t("editor.alerts.try.skipped", answer.skipped) : "",
         answer.unusable?.length ? t("editor.alerts.try.left_empty", { fields: answer.unusable.join(", ") }) : ""].filter(Boolean).join(" ")
      : t("editor.alerts.try.nothing", { version: alerts.value?.min_firmware || "0.2.31" });
  } catch (e: any) {
    result.value = e.message;
  }
});
</script>

<template>
  <section id="alerts-try" class="card">
    <h2>{{ t("editor.alerts.nav.try") }}</h2>
    <p>{{ t("editor.alerts.wizard.text") }}</p>
    <ol class="wizard">
      <li>
        <b>{{ t("editor.alerts.wizard.step_trigger") }}</b>
        <TriggerStep id="alert-trigger" v-model="choice.trigger" />
      </li>
      <li>
        <b>{{ t("editor.alerts.wizard.step_alert") }}</b>
        <div class="try-grid">
          <div class="field"><label class="f-label" for="try-title">{{ t("editor.alerts.try.title") }}</label><input id="try-title" v-model="form.title" maxlength="64" /></div>
          <div class="field"><label class="f-label" for="try-subtitle">{{ t("editor.alerts.try.subtitle") }}</label><input id="try-subtitle" v-model="form.subtitle" maxlength="240" /></div>
          <div class="field"><label class="f-label" for="try-icon">{{ t("editor.alerts.try.icon") }}</label><input id="try-icon" v-model="form.icon" list="try-icons" placeholder="doorbell" />
            <datalist id="try-icons"><option v-for="icon in alerts.suggested_icons" :key="icon.name" :value="icon.name">{{ icon.label || icon.name }}</option></datalist></div>
          <div class="field"><label class="f-label" for="try-color">{{ t("editor.alerts.try.color") }}</label>
            <select id="try-color" v-model="form.color"><option value="">{{ t("editor.alerts.white") }}</option><option v-for="colour in alerts.colors" :key="colour.name" :value="colour.name">{{ colour.label }}</option></select></div>
          <div class="field"><label class="f-label" for="try-button">{{ t("editor.alerts.try.button") }}</label><input id="try-button" v-model="form.button_text" maxlength="16" /></div>
          <div class="field"><label class="f-label" for="try-timeout">{{ t("editor.alerts.try.timeout") }}</label><input id="try-timeout" v-model.number="form.timeout" type="number" min="0" max="86400" /></div>
          <div class="field"><span class="f-label">{{ t("editor.alerts.wizard.camera") }}</span>
            <div class="wizard-row">
              <UiPicker id="alert-camera" :items="cameras" :placeholder="t('editor.plugin_tile.search')" :empty="t('editor.doorbell.no_camera')" width="320px" @pick="(id: string) => (choice.camera = id)">
                <template #trigger><button type="button" class="ghost-btn"><Icon name="magnify" />{{ choice.camera ? nameOf(choice.camera) : t("editor.alerts.wizard.no_camera") }}</button></template>
              </UiPicker>
              <button v-if="choice.camera" type="button" class="btn quiet mini" :aria-label="t('editor.plugin_tile.remove', { name: nameOf(choice.camera) })" @click="choice.camera = ''"><Icon name="close" /></button>
            </div></div>
          <label class="check field"><input type="checkbox" id="try-flash" v-model="form.flash" /><span>{{ t("editor.alerts.try.flash") }}<small>{{ t("editor.alerts.try.flash_hint") }}</small></span></label>
        </div>
      </li>
      <li>
        <b>{{ t("editor.alerts.wizard.step_screens") }}</b>
        <UiChips v-model="choice.screens" :chips="screenChips" :label="t('editor.alerts.wizard.step_screens')" />
        <p class="hint">{{ t("editor.alerts.wizard.screens_hint", { ready: readyScreens.length }) }}</p>
      </li>
    </ol>
    <AutomationInstall kind="alert" :automation="automation" :ready="Boolean(choice.trigger)" :not-ready="t('editor.alerts.wizard.not_ready')" />
    <div class="actions">
      <button type="button" class="btn quiet" id="try-send" :disabled="trying || !readyScreens.length" @click="send">{{ trying ? t("editor.alerts.try.sending") : t("editor.alerts.try.send") }}</button>
      <span class="status-line" id="try-result" role="status">{{ result }}</span>
    </div>
    <details class="yaml-details">
      <summary>{{ t("editor.events.wizard.yaml") }}</summary>
      <div class="copy-line">
        <pre id="alerts-automation-yaml">{{ yaml }}</pre>
        <button type="button" class="btn quiet mini" @click="ui.copyText(yaml, undefined, 'yaml')">{{ t("editor.alerts.copy_yaml") }}</button>
      </div>
    </details>
  </section>
</template>

<style scoped>
.wizard { margin: 0; padding-left: 20px; display: grid; gap: 14px; }
.wizard > li { display: grid; gap: 8px; }
.wizard-row { display: flex; flex-wrap: wrap; align-items: center; gap: 8px; }
.yaml-details summary { cursor: pointer; color: var(--muted); font-size: 13px; }
</style>
