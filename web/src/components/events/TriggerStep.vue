<script setup lang="ts">
// The first step of a wizard of Events: what starts the automation. Any entity that can (model/automation.ts
// triggerKind): an event entity, a sensor or switch that turns on, a button, offered in a searchable list of the
// person's own, with the ones Home Assistant says fit best first (`suggestions`, such as its doorbells).
import { computed } from "vue";
import { t } from "../../i18n";
import { triggerKind, type TriggerKind } from "../../model/automation";
import { useInventoryStore } from "../../stores/inventory";
import Icon from "../ui/Icon.vue";
import UiPicker from "../ui/UiPicker.vue";

export type Suggestion = { trigger: string; kind: TriggerKind; name: string; detail?: string };
const props = defineProps<{ modelValue: string; suggestions?: Suggestion[]; domains?: Record<string, TriggerKind>; mark?: string; id: string }>();
const emit = defineEmits<{ "update:modelValue": [entity: string]; suggest: [suggestion: Suggestion] }>();
const inv = useInventoryStore();
const known = computed(() => Object.fromEntries((props.suggestions || []).map((s) => [s.trigger, s.kind])));
const entities = computed(() => (inv.inventory.entities || []).filter((e) => triggerKind(e.id, known.value, props.domains))
  .map((e) => ({ id: e.id, name: e.name || e.id, hint: e.area || e.id })));
const nameOf = (id: string) => (inv.inventory.entities || []).find((e) => e.id === id)?.name || id;
const kind = computed(() => (props.modelValue ? triggerKind(props.modelValue, known.value, props.domains) : null));
function suggest(s: Suggestion) {
  emit("update:modelValue", s.trigger);
  emit("suggest", s);
}
</script>

<template>
  <div class="trigger-step">
    <div v-if="suggestions?.length" class="trigger-suggestions">
      <span class="hint">{{ t("editor.events.wizard.suggested") }}</span>
      <button v-for="s in suggestions" :key="s.trigger" type="button" class="suggestion" :data-trigger="s.trigger"
        :aria-pressed="modelValue === s.trigger ? 'true' : 'false'" @click="suggest(s)">
        <span v-if="mark" class="mdi">{{ mark }}</span><span><strong>{{ s.name }}</strong><small>{{ s.detail || s.trigger }}</small></span>
      </button>
    </div>
    <div class="trigger-row">
      <UiPicker :id="id" :items="entities" :placeholder="t('editor.plugin_tile.search')" :empty="t('editor.events.wizard.none')" width="340px"
        @pick="(entity: string) => emit('update:modelValue', entity)">
        <template #trigger><button type="button" class="ghost-btn"><Icon name="magnify" />{{ modelValue ? nameOf(modelValue) : t("editor.events.wizard.pick_trigger") }}</button></template>
      </UiPicker>
      <code v-if="modelValue">{{ modelValue }}</code>
    </div>
    <p v-if="kind" class="hint" :id="`${id}-kind`">{{ t(`editor.events.wizard.kind.${kind}`) }}</p>
  </div>
</template>

<style scoped>
.trigger-step { display: grid; gap: 8px; }
.trigger-row, .trigger-suggestions { display: flex; flex-wrap: wrap; align-items: center; gap: 8px; }
.suggestion { display: inline-flex; align-items: center; gap: 8px; padding: 8px 12px; border-radius: 12px; border: 1px solid var(--line); background: var(--surface); color: var(--ink); font: inherit; text-align: left; cursor: pointer; }
.suggestion > span:last-child { display: grid; }
.suggestion small { color: var(--muted); font-size: 11.5px; }
.suggestion[aria-pressed="true"] { border-color: var(--accent); background: var(--accent-soft); }
</style>
