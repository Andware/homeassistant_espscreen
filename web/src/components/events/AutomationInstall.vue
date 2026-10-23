<script setup lang="ts">
// The end of a wizard of Events: the automation it built, into Home Assistant (screen_manager/app/automations.py), the
// way Home Assistant's own automation editor saves one, and the ones of Tessera that are there already ("Tessera: ..."),
// each with a link to it in Home Assistant. Only an automation this app saved can be saved again or removed here.
import { computed, onMounted, ref } from "vue";
import { getJson, send } from "../../api";
import { useBusy } from "../../composables/useBusy";
import { useConfirm } from "../../composables/useConfirm";
import { t } from "../../i18n";
import type { Automation } from "../../model/automation";

type Listed = { entity: string; id: string | null; kind: string | null; name: string; on: boolean; last?: string | null };
const props = defineProps<{ kind: "doorbell" | "alert"; automation: Automation; ready: boolean; notReady?: string }>();
const { confirm } = useConfirm();
const { busy, run } = useBusy();
const saved = ref<{ id: string; path: string } | null>(null);
const message = ref("");
const listed = ref<Listed[]>([]);
// Home Assistant's own page of an automation, in the frame around the editor (ingress): the address is the same origin.
const link = (id: string) => `/config/automation/edit/${encodeURIComponent(id)}`;
const editing = computed(() => listed.value.find((item) => item.id === saved.value?.id));

async function refresh() {
  try { listed.value = (await getJson<{ automations: Listed[] }>(`automations?kind=${props.kind}`)).automations || []; } catch { listed.value = []; }
}
onMounted(refresh);
const add = () => run(async () => {
  message.value = "";
  try {
    saved.value = await send<{ id: string; path: string }>("automations", "POST", { kind: props.kind, id: saved.value?.id, config: props.automation });
    message.value = t("editor.events.install.saved");
    setTimeout(refresh, 1500);  // Home Assistant has the new entity a moment after it reloaded the automation
  } catch (e: any) {
    message.value = e.message;
  }
});
const remove = (item: Listed) => run(async () => {
  if (!item.id || !(await confirm(t("editor.events.install.remove_confirm", { name: item.name }), { confirm: t("editor.events.install.remove"), danger: true }))) return;
  try {
    await send(`automations/${encodeURIComponent(item.id)}`, "DELETE");
    if (saved.value?.id === item.id) saved.value = null;
    listed.value = listed.value.filter((other) => other.id !== item.id);
  } catch (e: any) {
    message.value = e.message;
  }
});
</script>

<template>
  <div class="automation-install">
    <div class="actions">
      <button type="button" class="btn primary" :id="`${kind}-install`" :disabled="busy || !ready" @click="add">
        {{ busy ? t("editor.events.install.saving") : saved ? t("editor.events.install.save_again") : t("editor.events.install.add") }}</button>
      <a v-if="saved" class="btn quiet" :href="link(saved.id)" target="_top">{{ t("editor.events.install.open") }}</a>
      <span class="status-line" role="status">{{ message || (!ready ? notReady : "") }}</span>
    </div>
    <p class="hint">{{ t("editor.events.install.hint", { title: automation.alias }) }}</p>
    <div v-if="listed.length" class="installed">
      <span class="f-label">{{ t("editor.events.install.listed") }}</span>
      <div v-for="item in listed" :key="item.entity" class="installed-row" :class="{ current: editing && item === editing }">
        <span class="dot" :class="{ off: !item.on }" :title="item.on ? t('editor.events.install.on') : t('editor.events.install.off')" />
        <span class="installed-name">{{ item.name }}</span>
        <a v-if="item.id" class="btn quiet mini" :href="link(item.id)" target="_top">{{ t("editor.events.install.open") }}</a>
        <button v-if="item.kind" type="button" class="btn quiet mini" @click="remove(item)">{{ t("editor.events.install.remove") }}</button>
      </div>
    </div>
  </div>
</template>

<style scoped>
.automation-install { display: grid; gap: 8px; }
.installed { display: grid; gap: 6px; }
.installed-row { display: flex; align-items: center; gap: 8px; padding: 6px 10px; border: 1px solid var(--line); border-radius: 10px; }
.installed-row.current { border-color: var(--accent); }
.installed-name { flex: 1; min-width: 0; overflow: hidden; text-overflow: ellipsis; white-space: nowrap; }
.dot { width: 8px; height: 8px; border-radius: 50%; background: var(--good, var(--accent)); flex: none; }
.dot.off { background: var(--muted); }
</style>
